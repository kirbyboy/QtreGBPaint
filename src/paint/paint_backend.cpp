#include "paint/paint_backend.h"

#include <cstdint>

#include "paint/cursor_asset.h"

PaintBackend::PaintBackend(IGuiBackend* backend) : m_backend(backend)
{
	m_option_icon_palette.setColor(0, {0, 0, 0, 0});

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
							 const std::string& iconNormalPath,
							 const std::string& iconHoverPath,
							 const std::string& iconPressedPath,
							 const Palette& palette)
{
	ToolButton btn;
	btn.name = name;
	btn.tool = std::move(tool);
	btn.cursor = cursor;
	btn.iconNormal  = loadIcon(iconNormalPath, palette);
	btn.iconHover   = loadIcon(iconHoverPath, palette);
	btn.iconPressed = loadIcon(iconPressedPath, palette);
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
		return m_activeTool.get();
	}
	return m_activeTool.get();
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
	if (!PaintCore::load_cursor_asset(path, image, &error))
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
