#include "paint/paint_backend.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <utility>

#include "paint/cursor_asset.h"

PaintBackend::PaintBackend(IGuiBackend* backend) : m_backend(backend)
{
	regenerate_palette();
	apply_default_colors();

	m_option_icon_palette.setColor(0, {0, 0, 0, 0});
	m_icon_palette.setColor(0, {0, 0, 0, 0});

	// Default icon (question mark) with the same 3D frame as the others.
	static const uint8_t kDefaultIcon[16 * 16] = {
		7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 1, 1, 1, 1, 1, 1, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 1, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 1,
		6, 5, 5, 5, 1, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 1, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 1, 1, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 1, 1, 5, 5, 5, 5, 5, 5, 1,
		6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 1,
		5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	};

	if (!m_backend) return;

	void* tex = m_backend->createTexture(16, 16);
	if (!tex) return;

	std::vector<PaintCore::Color> pixels(16 * 16);
	for (int i = 0; i < 16 * 16; ++i)
	{
		uint8_t level = kDefaultIcon[i];
		if (level >= m_icon_palette.size()) level = static_cast<uint8_t>(m_icon_palette.size() - 1);
		pixels[i] = m_icon_palette.getColor(level);
	}
	m_backend->updateTexture(tex, pixels.data(), 16, 16);
	m_defaultIcon = reinterpret_cast<ImTextureID>(tex);
}

PaintBackend::~PaintBackend()
{
	if (!m_backend) return;

	for (void* texture : m_textures)
	{
		m_backend->destroyTexture(texture);
	}
	m_textures.clear();

	if (m_defaultIcon != 0)
	{
		m_backend->destroyTexture(reinterpret_cast<void*>(m_defaultIcon));
		m_defaultIcon = 0;
	}
}

int PaintBackend::palette_bits_per_pixel() const
{
	return m_session.getBitsPerPixel();
}

int PaintBackend::palette_usable_count() const
{
	return palette_usable_color_count(palette_bits_per_pixel());
}

int PaintBackend::palette_visual_count() const
{
	return palette_visual_color_count(palette_bits_per_pixel());
}

PaintCore::ColorIndex PaintBackend::palette_visual_index(int slot) const
{
	return static_cast<PaintCore::ColorIndex>(
		palette_visual_color_index(slot, palette_bits_per_pixel()));
}

void PaintBackend::select_palette(PaletteMode mode)
{
	m_palette_mode = mode;
	// Choosing a tone always shows it light-to-dark; Invert Palette is then
	// applied on top of the current tone. The selected foreground/background
	// indices are preserved; only their colors change.
	m_palette_inverted = false;
	regenerate_palette();
	apply_tool_colors();
}

void PaintBackend::toggle_palette_inverted()
{
	m_palette_inverted = !m_palette_inverted;
	regenerate_palette();
	apply_tool_colors();
}

void PaintBackend::set_foreground_index(PaintCore::ColorIndex index)
{
	const int usable = palette_usable_count();
	if (usable <= 0)
	{
		return;
	}

	m_foreground_index = static_cast<PaintCore::ColorIndex>(
		std::min<int>(index, usable - 1));
	apply_tool_colors();
}

void PaintBackend::set_background_index(PaintCore::ColorIndex index)
{
	const int usable = palette_usable_count();
	if (usable <= 0)
	{
		return;
	}

	m_background_index = static_cast<PaintCore::ColorIndex>(
		std::min<int>(index, usable - 1));
	apply_tool_colors();
}

bool PaintBackend::can_undo() const
{
	return m_session.can_undo();
}

bool PaintBackend::can_redo() const
{
	return m_session.can_redo();
}

bool PaintBackend::undo()
{
	return m_session.undo();
}

bool PaintBackend::redo()
{
	return m_session.redo();
}

bool PaintBackend::resize_canvas(int width, int height)
{
	return m_session.resize(width, height, m_background_index);
}

bool PaintBackend::apply_pending_color_pick()
{
	if (!m_activeTool)
	{
		return false;
	}

	ToolBase::ColorPick pick;
	if (!m_activeTool->take_color_pick(pick))
	{
		return false;
	}

	const int usable = palette_usable_count();
	if (pick.color >= usable)
	{
		return false;
	}

	if (pick.background)
	{
		set_background_index(pick.color);
	}
	else
	{
		set_foreground_index(pick.color);
	}

	// The picker is only a temporary tool: go back to the last editing tool
	// after sampling. If no editing tool has been selected yet, stay here.
	if (!m_last_edit_button.empty())
	{
		selectButton(m_last_edit_button);
	}
	return true;
}

bool PaintBackend::is_modified() const
{
	return m_file.is_modified(m_session.current_state_id(),
	                          m_session.has_floating_region()) ||
	       m_session.has_pending_edit() ||
	       (m_activeTool && m_activeTool->is_drawing());
}

bool PaintBackend::new_document(int width, int height, std::string& error)
{
	return new_document(width, height, m_session.getBitsPerPixel(), error);
}

bool PaintBackend::new_document(int width, int height, int bits_per_pixel,
                                std::string& error)
{
	error.clear();
	if (width < 1 || height < 1)
	{
		error = "Width and height must be positive";
		return false;
	}
	if (bits_per_pixel < 1 || bits_per_pixel > 8)
	{
		error = "Bits per pixel must be in the range 1..8";
		return false;
	}

	// New documents start with the default background: the first visible
	// swatch. Its actual color follows the current palette/inversion.
	const PaintCore::ColorIndex fill = static_cast<PaintCore::ColorIndex>(
		palette_visual_color_index(0, bits_per_pixel));

	try
	{
		ImageBuffer image(width, height, bits_per_pixel);
		image.fill(fill);
		m_session.load_image(std::move(image));
	}
	catch (const std::exception& exception)
	{
		error = exception.what();
		return false;
	}

	regenerate_palette();
	apply_default_colors();
	apply_tool_colors();
	m_file.mark_new();
	return true;
}

bool PaintBackend::open_document(const std::string& path, std::string& error)
{
	error.clear();
	if (path.empty())
	{
		error = "No file path";
		return false;
	}

	IndexedImage loaded = IndexedImage::loadFromFile(path);
	if (!loaded.isValid())
	{
		error = loaded.error().empty() ? "Could not load the file" : loaded.error();
		return false;
	}

	try
	{
		ImageBuffer image(loaded.width(), loaded.height(), loaded.bitsPerPixel());
		image.write_region(
			PaintCore::Rect{0, 0, loaded.width(), loaded.height()},
			loaded.data().data());
		m_session.load_image(std::move(image));
	}
	catch (const std::exception& exception)
	{
		error = exception.what();
		return false;
	}

	// The loaded file may use a different bpp; rebuild the palette and reset
	// the selected colors to the new usable range.
	regenerate_palette();
	apply_default_colors();
	apply_tool_colors();
	m_file.mark_saved(path, m_session.current_state_id());
	return true;
}

bool PaintBackend::save_document(std::string& error)
{
	if (!m_file.has_path())
	{
		error = "No file path";
		return false;
	}
	return save_document_as(m_file.path(), error);
}

bool PaintBackend::save_document_as(const std::string& path, std::string& error)
{
	error.clear();
	if (path.empty())
	{
		error = "No file path";
		return false;
	}

	// Saving captures what the user sees: finish or discard transient state
	// before writing.
	if (m_activeTool && m_activeTool->is_drawing())
	{
		m_activeTool->on_cancel(m_session);
	}
	if (m_session.has_floating_region())
	{
		m_session.commit_floating_region();
	}
	if (m_session.has_pending_edit())
	{
		m_session.commit_pending_edit();
	}

	const ImageBuffer& image = m_session.get_main_buffer();
	if (!save_indexed_image(path, image.width(), image.height(),
	                        image.bitsPerPixel(), image.data(), error))
	{
		return false;
	}

	m_file.mark_saved(path, m_session.current_state_id());
	return true;
}

void PaintBackend::regenerate_palette()
{
	m_palette = make_palette(m_palette_mode, palette_bits_per_pixel(),
	                         m_palette_inverted);
}

void PaintBackend::apply_default_colors()
{
	const int bpp = palette_bits_per_pixel();
	const int visual = palette_visual_color_count(bpp);
	if (visual <= 0)
	{
		m_foreground_index = 0;
		m_background_index = 0;
		return;
	}

	// New/opened documents use the last visible swatch as foreground and the
	// first one as background. Changing only the palette keeps these indices
	// untouched.
	m_foreground_index = static_cast<PaintCore::ColorIndex>(
		palette_visual_color_index(visual - 1, bpp));
	m_background_index = static_cast<PaintCore::ColorIndex>(
		palette_visual_color_index(0, bpp));
}

void PaintBackend::apply_tool_colors()
{
	for (const ToolButton& button : m_buttons)
	{
		if (button.tool)
		{
			button.tool->set_colors(m_foreground_index, m_background_index);
		}
	}

	if (m_activeTool)
	{
		m_activeTool->sync_floating_region(m_session);
	}
}

ImTextureID PaintBackend::loadIcon(const std::string& path, const Palette& palette)
{
	if (!m_backend) return m_defaultIcon;

	IndexedImage image = IndexedImage::loadFromFile(path);
	if (!image.isValid()) return m_defaultIcon;

	ImTextureID texture = createIconTexture(image, 0, 0,
	                                        image.width(), image.height(), palette);
	return texture ? texture : m_defaultIcon;
}

bool PaintBackend::loadOptionIcon(const std::string& key,
                                  const std::string& sheetPath,
                                  int x, int y, int width, int height)
{
	if (!m_backend) return false;

	IndexedImage image = IndexedImage::loadFromFile(sheetPath);
	if (!image.isValid()) return false;

	ImTextureID texture = createIconTexture(image, x, y, width, height,
	                                        m_option_icon_palette);
	if (!texture) return false;

	m_option_icons[key] = texture;
	return true;
}

ImTextureID PaintBackend::optionIcon(const std::string& key) const
{
	auto it = m_option_icons.find(key);
	return it == m_option_icons.end() ? 0 : it->second;
}

ImTextureID PaintBackend::createIconTexture(const IndexedImage& image,
                                            int x, int y,
                                            int width, int height,
                                            const Palette& palette)
{
	if (!m_backend) return 0;
	if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
	    x + width > image.width() || y + height > image.height())
	{
		return 0;
	}

	void* tex = m_backend->createTexture(width, height);
	if (!tex) return 0;

	std::vector<PaintCore::Color> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
	const auto& data = image.data();
	for (int row = 0; row < height; ++row)
	{
		for (int col = 0; col < width; ++col)
		{
			std::uint8_t level = data[
				static_cast<std::size_t>(y + row) * image.width() + (x + col)];
			if (level >= palette.size())
			{
				level = static_cast<std::uint8_t>(palette.size() - 1);
			}
			pixels[static_cast<std::size_t>(row) * width + col] =
				palette.getColor(level);
		}
	}

	m_backend->updateTexture(tex, pixels.data(), width, height);
	m_textures.push_back(tex);
	return reinterpret_cast<ImTextureID>(tex);
}

void PaintBackend::addButton(const std::string& name,
							 std::shared_ptr<ToolBase> tool,
							 PaintCore::CursorType cursor,
							 const std::string& iconPath,
							 const Palette& palette)
{
	ToolButton btn;
	btn.name = name;
	btn.tool = std::move(tool);
	btn.cursor = cursor;
	btn.icon = loadIcon(iconPath, palette);

	if (btn.tool)
	{
		btn.tool->set_colors(m_foreground_index, m_background_index);
	}

	m_buttons.push_back(std::move(btn));
}

ToolBase* PaintBackend::selectButton(const std::string& name)
{
	for (const auto& btn : m_buttons)
	{
		if(btn.name != name)
		{
			continue;
		}

		m_activeButton = name;
		m_activeTool = btn.tool;
		m_activeCursor = btn.cursor;

		// Remember the last editing tool. Selection and Pick Color do not
		// replace it: picking a color returns to the previous editing tool.
		if (btn.tool && name != "Selection" && name != "Pick Color")
		{
			m_last_edit_button = name;
		}
		break;
	}

	ToolBase* tool = m_activeTool.get();
	m_session.set_tool(tool);
	if (tool)
	{
		tool->set_colors(m_foreground_index, m_background_index);
		tool->sync_floating_region(m_session);
	}
	return tool;
}

bool PaintBackend::loadCursor(const std::string& buttonName,
                              const std::string& path,
                              std::string& error)
{
	error.clear();

	for (const auto& btn : m_buttons)
	{
		if (btn.name != buttonName)
		{
			continue;
		}

		return loadCursor(btn.cursor, path, error);
	}

	error = "Button not found: " + buttonName;
	return false;
}

bool PaintBackend::loadCursor(PaintCore::CursorType cursor,
                              const std::string& path,
                              std::string& error)
{
	error.clear();

	if (!m_backend)
	{
		error = "No GUI backend";
		return false;
	}

	PaintCore::CursorImage image;
	const bool black_as_dark_gray =
		(cursor == PaintCore::CursorType::Brush);
	if (!PaintCore::load_cursor_asset(path, image, &error,
	                                  black_as_dark_gray))
	{
		return false;
	}
	if (!m_backend->set_cursor_image(cursor, image))
	{
		error = "Backend could not create the cursor";
		return false;
	}
	return true;
}

bool PaintBackend::setCursorImage(PaintCore::CursorType cursor,
                                  const PaintCore::CursorImage& image)
{
	return m_backend && m_backend->set_cursor_image(cursor, image);
}
