#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <memory>
#include <utility>
#include <vector>

#include "paint/paint_frontend.h"
#include "paint/checkerboard.h"
#include "paint/color_imgui.h"
#include "paint/png_image.h"
#include "paint/window_title.h"
#include "tools/tool_option.h"

#include "ImGuiFileDialog.h"

using namespace std;

namespace
{
constexpr float kToolButtonSize = 32.0f;
constexpr int kToolbarColumns = 2;
constexpr float kToolOptionsHeight = 116.0f;
constexpr float kToolOptionIconWidth = 60.0f;
constexpr float kToolOptionIconHeight = 16.0f;
constexpr float kToolOptionRowHeight = 24.0f;
constexpr float kCanvasResizeMargin = 12.0f;
constexpr float kPaletteColorSize = 50.0f;
constexpr float kSwatchSize = 28.0f;
constexpr float kSwatchGap = 6.0f;
constexpr float kSwatchBevel = 2.0f;
constexpr int kPaletteReferenceCells = 10;
constexpr int kPaletteSingleRowLimit = 16;
constexpr float kMinCanvasWidth = 256.0f;

float choice_preview_extent(const std::vector<PaintCore::Vec2>& offsets)
{
	if (offsets.empty())
	{
		return 0.0f;
	}

	int min_x = offsets.front().x;
	int max_x = offsets.front().x;
	int min_y = offsets.front().y;
	int max_y = offsets.front().y;
	for (const PaintCore::Vec2& offset : offsets)
	{
		min_x = std::min(min_x, offset.x);
		max_x = std::max(max_x, offset.x);
		min_y = std::min(min_y, offset.y);
		max_y = std::max(max_y, offset.y);
	}

	return static_cast<float>(
		std::max(max_x - min_x + 1, max_y - min_y + 1));
}

void draw_choice_preview(ImDrawList* draw_list,
                         const std::vector<PaintCore::Vec2>& offsets,
                         const ImVec2& cell_min, const ImVec2& cell_max,
                         float reference_extent, ImU32 color)
{
	if (!draw_list || offsets.empty())
	{
		return;
	}

	int min_x = offsets.front().x;
	int max_x = offsets.front().x;
	int min_y = offsets.front().y;
	int max_y = offsets.front().y;
	for (const PaintCore::Vec2& offset : offsets)
	{
		min_x = std::min(min_x, offset.x);
		max_x = std::max(max_x, offset.x);
		min_y = std::min(min_y, offset.y);
		max_y = std::max(max_y, offset.y);
	}

	const float cell_width = cell_max.x - cell_min.x;
	const float cell_height = cell_max.y - cell_min.y;
	const float requested_scale = std::min(
		(cell_width - 4.0f) / reference_extent,
		(cell_height - 4.0f) / reference_extent);
	const float scale = std::max(1.0f, std::floor(requested_scale));
	const float width = (max_x - min_x + 1) * scale;
	const float height = (max_y - min_y + 1) * scale;
	const ImVec2 origin(
		std::floor(cell_min.x + (cell_width - width) * 0.5f),
		std::floor(cell_min.y + (cell_height - height) * 0.5f));

	for (const PaintCore::Vec2& offset : offsets)
	{
		const ImVec2 p0(
			origin.x + (offset.x - min_x) * scale,
			origin.y + (offset.y - min_y) * scale);
		draw_list->AddRectFilled(
			p0, ImVec2(p0.x + scale, p0.y + scale), color);
	}
}

struct ToolbarLayout
{
	float width = 0.0f;
	float contentHeight = 0.0f;
};

ToolbarLayout compute_toolbar_layout(int toolCount, const ImGuiStyle& style)
{
	const int rows = (toolCount + kToolbarColumns - 1) / kToolbarColumns;
	const float columnGap = style.ItemSpacing.x;
	const float rowGap = style.ItemSpacing.y;

	const float toolsWidth =
		kToolButtonSize * kToolbarColumns +
		columnGap * (kToolbarColumns - 1);
	const float toolsHeight =
		rows > 0 ? rows * kToolButtonSize + (rows - 1) * rowGap : 0.0f;

	const float contentHeight =
		style.WindowPadding.y * 2.0f +
		(rows > 0 ? toolsHeight + rowGap : 0.0f) +
		kToolOptionsHeight;

	return ToolbarLayout{
		toolsWidth + style.WindowPadding.x * 2.0f,
		contentHeight,
	};
}

float compute_palette_grid_width(const ImGuiStyle& style)
{
	return kPaletteReferenceCells * kPaletteColorSize +
		(kPaletteReferenceCells - 1) * style.ItemSpacing.x;
}

float compute_palette_panel_width(const ImGuiStyle& style)
{
	return kPaletteColorSize + style.ItemSpacing.x +
		compute_palette_grid_width(style) +
		style.WindowPadding.x * 2.0f;
}

int palette_row_count(int colorCount)
{
	return colorCount <= kPaletteSingleRowLimit ? 1 : 2;
}

int palette_column_count(int colorCount, int rows)
{
	return (colorCount + rows - 1) / rows;
}

float compute_palette_height(const ImGuiStyle& style)
{
	return style.WindowPadding.y * 2.0f +
		kPaletteColorSize + style.ItemSpacing.y +
		ImGui::GetFrameHeight();
}

float compute_status_bar_height(const ImGuiStyle& style)
{
	return style.WindowPadding.y * 2.0f + ImGui::GetTextLineHeight();
}

std::string ensure_extension(std::string path, const char* extension)
{
	if (path.empty())
	{
		return path;
	}
	std::filesystem::path file_path(path);
	if (file_path.extension() != extension)
	{
		file_path.replace_extension(extension);
	}
	return file_path.string();
}

std::string ensure_idx_extension(std::string path)
{
	return ensure_extension(std::move(path), ".idx");
}

std::string recent_file_name(const std::string& path)
{
	const std::string name = std::filesystem::path(path).filename().string();
	return name.empty() ? path : name;
}

bool accessible_directory(const std::filesystem::path& path)
{
	std::error_code error;
	if (!std::filesystem::is_directory(path, error) || error)
	{
		return false;
	}
	// is_directory() only inspects metadata; opening the iterator also checks
	// that the process can actually read the directory.
	std::filesystem::directory_iterator probe(path, error);
	return !error;
}

std::string format_byte_size(std::size_t bytes)
{
	if (bytes < 1024)
	{
		return std::to_string(bytes) + " B";
	}
	char buffer[64] = {};
	std::snprintf(buffer, sizeof(buffer), "%.1f KiB",
	              static_cast<double>(bytes) / 1024.0);
	return buffer;
}

const char* indexed_format_description(IndexedFormat format)
{
	switch (format)
	{
		case IndexedFormat::Index:
			return "Plain decimal text, one value per pixel. Easy to inspect, "
			       "but the largest format.";
		case IndexedFormat::Binary:
			return "One raw byte per pixel, without row packing. Simple and "
			       "exact, but not human-readable.";
		case IndexedFormat::Packed:
			return "MSB-first row packing stored as hexadecimal text. Compact "
			       "and still inspectable; 2bpp halves the data.";
		case IndexedFormat::BinaryPacked:
			return "MSB-first row packing stored as raw bytes. The smallest "
			       "format, not human-readable.";
	}
	return "";
}
}

PaintFrontend::PaintFrontend(PaintBackend& backend) :
m_backend(backend),
m_canvas_area(backend.backend(), backend.session())
{
}

PaintFrontend::~PaintFrontend()
{
	if (m_about_logo_texture)
	{
		if (IGuiBackend* backend = m_backend.backend())
		{
			backend->destroyTexture(m_about_logo_texture);
		}
		m_about_logo_texture = nullptr;
	}
}

void PaintFrontend::ensure_about_logo()
{
	if (m_about_logo_attempted || m_about_logo_path.empty())
	{
		return;
	}

	m_about_logo_attempted = true;
	PaintCore::RgbaImage image;
	std::string error;
	if (!PaintCore::load_png_rgba(m_about_logo_path, image, error))
	{
		std::fprintf(stderr, "About logo: %s\n", error.c_str());
		return;
	}

	IGuiBackend* backend = m_backend.backend();
	if (!backend)
	{
		return;
	}

	void* texture = backend->createTexture(image.width, image.height);
	if (!texture)
	{
		std::fprintf(stderr, "About logo: could not create the texture\n");
		return;
	}

	backend->updateTexture(texture, image.pixels.data(), image.width,
	                       image.height);
	m_about_logo_texture = texture;
	m_about_logo_width = image.width;
	m_about_logo_height = image.height;
}

void PaintFrontend::draw_checkerboard(ImDrawList* draw_list, const ImVec2& min,
                                      const ImVec2& max, const ImVec2& origin)
{
	if (!m_checkerboard.valid())
	{
		if (IGuiBackend* backend = m_backend.backend())
		{
			// Swatches and option previews use 4 px cells; the texture is
			// small and created once on first use.
			m_checkerboard.create(backend, 4);
		}
	}

	if (m_checkerboard.valid())
	{
		m_checkerboard.draw(draw_list, min, max, origin);
		return;
	}

	PaintUi::draw_checkerboard(draw_list, min, max, 4.0f, origin);
}

void PaintFrontend::select_tool(const std::string& name)
{
	m_backend.selectButton(name);
}

void PaintFrontend::open_initial_file(const std::string& path)
{
	if (!path.empty())
	{
		open_recent_path(path);
	}
}

std::string PaintFrontend::file_dialog_directory() const
{
	if (!m_last_directory.empty() && accessible_directory(m_last_directory))
	{
		return m_last_directory;
	}

	// A stale or unreadable last directory must never produce an error. Fall
	// back silently to Pictures, then the home directory, then the cwd.
	if (const char* pictures = std::getenv("XDG_PICTURES_DIR"))
	{
		if (pictures[0] != '\0' && accessible_directory(pictures))
		{
			return pictures;
		}
	}

	if (const char* home = std::getenv("HOME"))
	{
		if (home[0] != '\0')
		{
			const std::filesystem::path home_path(home);
			const std::filesystem::path pictures_path = home_path / "Pictures";
			if (accessible_directory(pictures_path))
			{
				return pictures_path.string();
			}
			if (accessible_directory(home_path))
			{
				return home_path.string();
			}
		}
	}

	return ".";
}

void PaintFrontend::remember_file_directory(const std::string& path)
{
	const std::filesystem::path parent =
		std::filesystem::path(path).parent_path();
	if (!parent.empty())
	{
		m_last_directory = parent.string();
	}
}

void PaintFrontend::set_theme(Theme theme)
{
	m_theme = theme;
	// The smoke harness creates the frontend before the ImGui context, so the
	// style is applied lazily on the first render when no context exists yet.
	if (ImGui::GetCurrentContext())
	{
		apply_theme_style();
		m_theme_applied = true;
	}
}

void PaintFrontend::apply_theme_style()
{
	switch (m_theme)
	{
		case Theme::Dark:
			ImGui::StyleColorsDark();
			break;
		case Theme::Light:
			ImGui::StyleColorsLight();
			break;
		case Theme::Classic:
			ImGui::StyleColorsClassic();
			break;
	}
}

bool PaintFrontend::render()
{
	if (!m_theme_applied)
	{
		apply_theme_style();
		m_theme_applied = true;
	}

	// A window-manager close request is handled here so unsaved changes can be
	// offered to the user first.
	if (IGuiBackend* backend = m_backend.backend())
	{
		if (backend->quit_requested())
		{
			backend->clear_quit_request();
			request_close_window();
			if (m_should_close)
			{
				return false;
			}
		}
	}

	const std::string windowTitle =
		PaintCore::make_window_title(
			m_backend.file_display_name(), m_backend.is_modified()) +
		"###QtreGBPaintMain";

	// Full window with menu bar
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
	ImGui::Begin(windowTitle.c_str(), nullptr,
				 ImGuiWindowFlags_NoCollapse |
				 ImGuiWindowFlags_NoResize |
				 ImGuiWindowFlags_NoMove |
				 ImGuiWindowFlags_NoScrollbar |
				 ImGuiWindowFlags_NoScrollWithMouse |
				 ImGuiWindowFlags_NoSavedSettings |
				 ImGuiWindowFlags_MenuBar);
	ImGui::PopStyleVar();

	renderMenuBar();

	// Compute available space for layout
	ImVec2 contentRegion = ImGui::GetContentRegionAvail();
	const ImGuiStyle& style = ImGui::GetStyle();
	const float columnGap = style.ItemSpacing.x;
	const float palettePanelWidth = compute_palette_panel_width(style);
	const float paletteHeight = compute_palette_height(style);
	const float statusHeight = compute_status_bar_height(style);
	const ToolbarLayout toolbar = compute_toolbar_layout(
		static_cast<int>(m_backend.buttons().size()), style);
	// Reserve the always-visible status bar plus the spacing between the
	// canvas, the palette and the bar itself.
	const float bodyHeight = std::max(
		0.0f, contentRegion.y - statusHeight - style.ItemSpacing.y * 2.0f);
	const float toolbarHeight = std::max(toolbar.contentHeight, bodyHeight - paletteHeight);

	const float minCanvasWidth = kMinCanvasWidth + style.WindowPadding.x * 2.0f;
	const float minContentWidth = std::max(
		toolbar.width + columnGap + minCanvasWidth, palettePanelWidth);
	const float chromeWidth = ImGui::GetWindowWidth() - contentRegion.x;
	const float chromeHeight = ImGui::GetWindowHeight() - contentRegion.y;
	const int minWindowWidth = static_cast<int>(std::ceil(
		chromeWidth + minContentWidth));
	const int minWindowHeight = static_cast<int>(std::ceil(
		chromeHeight + toolbar.contentHeight + paletteHeight + statusHeight +
		style.ItemSpacing.y * 2.0f));
	if (IGuiBackend* backend = m_backend.backend())
	{
		backend->set_minimum_window_size(minWindowWidth, minWindowHeight);
	}

	// --- Left toolbar ---
	ImGui::BeginChild("Toolbar", ImVec2(toolbar.width, toolbarHeight), true,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderToolbar();
	ImGui::EndChild();

	ImGui::SameLine();

	// --- Canvas and palette column ---

	// --- Canvas area ---
	ImVec2 canvasRegion = ImVec2(contentRegion.x - toolbar.width - columnGap,
	                             std::max(0.0f, bodyHeight - paletteHeight));

	// When zoom changes, the canvas content size shrinks/grows before ImGui
	// would normally recalculate ScrollMax. If the view is scrolled to an edge,
	// that leaves one frame with a stale scroll and a visible blank strip.
	// Declaring the new content size before BeginChild makes ImGui clamp the
	// scroll in the same frame as the new zoom.
	if (m_zoom != m_last_canvas_zoom)
	{
		const int content_width = std::max(
			m_backend.session().getWidth(),
			m_canvas_area.resize_preview_width());
		const int content_height = std::max(
			m_backend.session().getHeight(),
			m_canvas_area.resize_preview_height());
		ImGui::SetNextWindowContentSize(ImVec2(
			static_cast<float>(content_width * m_zoom) + kCanvasResizeMargin,
			static_cast<float>(content_height * m_zoom) + kCanvasResizeMargin));
		m_last_canvas_zoom = m_zoom;
	}
	if (m_apply_pending_canvas_scroll)
	{
		// Keep the zoom anchor fixed by applying the scroll computed from the
		// previous view together with the new content size.
		ImGui::SetNextWindowScroll(m_pending_canvas_scroll);
		m_apply_pending_canvas_scroll = false;
	}

	ImGui::BeginChild("Canvas", canvasRegion, true, ImGuiWindowFlags_HorizontalScrollbar);
	renderCanvas();
	ImGui::EndChild();

	// --- Palette area ---
	ImGui::BeginChild("Palette", ImVec2(contentRegion.x, paletteHeight), true,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderPalette();
	ImGui::EndChild();

	// --- Status bar (always visible) ---
	renderStatusBar();

	apply_zoom_request();

	ImGui::End();

	renderFileDialogs();

	return !m_should_close;
}

void PaintFrontend::renderMenuBar()
{
	PaintSession& session = m_backend.session();

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N"))
			{
				request_new_document();
			}
			if (ImGui::MenuItem("Open", "Ctrl+O"))
			{
				request_open_document();
			}
			ImGui::Separator();
			const bool can_save =
				m_backend.is_modified() || !m_backend.has_file_path();
			if (ImGui::MenuItem("Save", "Ctrl+S", false, can_save))
			{
				request_save();
			}
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
			{
				request_save_as();
			}
			ImGui::Separator();
			if (ImGui::BeginMenu("Export"))
			{
				if (ImGui::MenuItem("Bitmap..."))
				{
					request_export(ImageExportFormat::Bmp);
				}
				if (ImGui::MenuItem("PNG..."))
				{
					request_export(ImageExportFormat::Png);
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			const std::vector<std::string>& recent = m_backend.recent_files();
			if (!recent.empty())
			{
				const std::size_t count =
					std::min<std::size_t>(recent.size(), 10);
				for (std::size_t index = 0; index < count; ++index)
				{
					const std::string& path = recent[index];
					const std::string name = recent_file_name(path);
					ImGui::PushID(static_cast<int>(index));
					if (ImGui::MenuItem(name.c_str()))
					{
						request_open_path(path);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", path.c_str());
					}
					ImGui::PopID();
				}
				ImGui::Separator();
			}
			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				request_close_window();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_backend.can_undo()))
			{
				m_backend.undo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_backend.can_redo()))
			{
				m_backend.redo();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Select All", "Ctrl+A"))
			{
				select_all();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Grid", "Ctrl+G", &m_show_grid);
			if (ImGui::MenuItem("Zoom +", "Ctrl++", false, m_zoom < 64))
			{
				request_zoom_steps(1);
			}
			if (ImGui::MenuItem("Zoom -", "Ctrl+-", false, m_zoom > 1))
			{
				request_zoom_steps(-1);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Selection"))
		{
			const bool has_floating = session.has_floating_region();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, has_floating))
			{
				copy_selection();
			}
			if (ImGui::MenuItem("Cut", "Ctrl+X", false, has_floating))
			{
				cut_selection();
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, session.has_clipboard()))
			{
				paste_selection();
			}
			if (ImGui::MenuItem("Delete", "Del", false, has_floating))
			{
				session.delete_floating_region();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Flip/Rotate...", "Ctrl+R", false,
			                    has_floating))
			{
				open_flip_rotate_dialog(FlipRotateTarget::Selection);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Invert Colors", nullptr, false, has_floating))
			{
				session.invert_floating_region_colors();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Image"))
		{
			if (ImGui::MenuItem("Flip/Rotate...", "Ctrl+R"))
			{
				open_flip_rotate_dialog(FlipRotateTarget::Canvas);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Properties...", "Ctrl+E"))
			{
				open_properties_dialog();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Palette"))
		{
			const auto palette_item = [this](const char* label, PaletteMode mode)
			{
				if (ImGui::MenuItem(label, nullptr,
				                    m_backend.palette_mode() == mode))
				{
					m_backend.select_palette(mode);
				}
			};

			palette_item("DMG", PaletteMode::Dmg);
			palette_item("Grayscale", PaletteMode::Grayscale);
			palette_item("Red", PaletteMode::Red);
			palette_item("Green", PaletteMode::Green);
			palette_item("Blue", PaletteMode::Blue);

			ImGui::Separator();
			if (ImGui::MenuItem("Invert Palette", nullptr,
			                    m_backend.palette_inverted()))
			{
				m_backend.toggle_palette_inverted();
			}

			ImGui::Separator();
			const std::optional<PaintCore::ColorIndex> transparent =
				m_backend.transparent_index();
			const PaintCore::ColorIndex foreground =
				m_backend.foreground_index();
			const bool foreground_is_transparent =
				transparent.has_value() && *transparent == foreground;
			const bool foreground_visible = is_visual_palette_index(
				foreground, m_backend.session().getBitsPerPixel());
			if (ImGui::MenuItem("Transparent Foreground", nullptr, false,
			                    foreground_visible &&
			                    !foreground_is_transparent))
			{
				m_backend.set_transparent_foreground();
			}
			if (ImGui::MenuItem("Clear Transparent Color", nullptr, false,
			                    m_backend.has_transparent_color()))
			{
				m_backend.clear_transparent_color();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings"))
		{
			if (ImGui::BeginMenu("Theme"))
			{
				const auto theme_item = [this](const char* label, Theme value)
				{
					if (ImGui::MenuItem(label, nullptr, m_theme == value))
					{
						set_theme(value);
					}
				};

				theme_item("Light", Theme::Light);
				theme_item("Dark", Theme::Dark);
				theme_item("Classic", Theme::Classic);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About QtreGBPaint..."))
			{
				m_open_about_popup = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	if (m_open_about_popup)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f,
			       viewport->Pos.y + viewport->Size.y * 0.5f),
			ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::OpenPopup("About QtreGBPaint");
		m_open_about_popup = false;
	}

	if (ImGui::BeginPopupModal("About QtreGBPaint", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ensure_about_logo();
		if (m_about_logo_texture)
		{
			const float max_logo = 160.0f;
			const float largest = static_cast<float>(
				std::max(m_about_logo_width, m_about_logo_height));
			const float scale = std::min(1.0f, max_logo / largest);
			const ImVec2 logo_size(
				static_cast<float>(m_about_logo_width) * scale,
				static_cast<float>(m_about_logo_height) * scale);
			const float available = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(
				ImGui::GetCursorPosX() +
				std::max(0.0f, (available - logo_size.x) * 0.5f));
			ImGui::Image(
				reinterpret_cast<ImTextureID>(m_about_logo_texture),
				logo_size);
			ImGui::Spacing();
		}

		const auto centered_text = [](const char* text)
		{
			const float text_width = ImGui::CalcTextSize(text).x;
			const float available = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(
				ImGui::GetCursorPosX() +
				std::max(0.0f, (available - text_width) * 0.5f));
			ImGui::TextUnformatted(text);
		};

		centered_text("QtreGBPaint");
		centered_text("Made by kirbyboy");
		ImGui::Separator();
		ImGui::TextUnformatted("Copyright (C) 2026 the QtreGBPaint project");
		ImGui::TextUnformatted(
			"License: GNU General Public License v3.0 or later");
		ImGui::TextDisabled("Code: GPL-3.0-or-later");
		ImGui::TextDisabled("Visual assets: CC0 1.0");
		ImGui::TextDisabled("https://www.gnu.org/licenses/gpl-3.0.html");
		ImGui::TextDisabled("https://github.com/kirbyboy/QtreGBPaint");
		ImGui::Spacing();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_open_properties_popup)
	{
		ImGui::OpenPopup("Properties");
		m_open_properties_popup = false;
	}

	if (ImGui::BeginPopupModal("Properties", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputInt("Width", &m_canvas_size_width);
		ImGui::InputInt("Height", &m_canvas_size_height);
		m_canvas_size_width = std::max(1, m_canvas_size_width);
		m_canvas_size_height = std::max(1, m_canvas_size_height);
		ImGui::SliderInt("Bits per pixel", &m_canvas_size_bpp, 1, 8);
		m_canvas_size_bpp = std::clamp(m_canvas_size_bpp, 1, 8);
		ImGui::Text("Colors: %d", palette_usable_color_count(m_canvas_size_bpp));
		ImGui::TextDisabled("New area uses the current background color.");
		if (m_canvas_size_bpp != m_backend.session().getBitsPerPixel())
		{
			ImGui::Spacing();
			ImGui::TextUnformatted(
				"Changing bits per pixel rewrites the image:");
			ImGui::TextUnformatted(
				"all pixels are remapped to the closest tone");
			ImGui::TextUnformatted(
				"in the new palette; the saved file uses the new format.");
		}

		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			const int target_bpp = m_canvas_size_bpp;
			m_backend.resize_canvas(m_canvas_size_width, m_canvas_size_height);
			if (target_bpp != m_backend.session().getBitsPerPixel())
			{
				m_backend.change_bits_per_pixel(target_bpp);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_open_flip_rotate_popup)
	{
		ImGui::OpenPopup("Flip/Rotate");
		m_open_flip_rotate_popup = false;
	}

	if (ImGui::BeginPopupModal("Flip/Rotate", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted(
			m_flip_rotate_target == FlipRotateTarget::Selection
				? "Transform selection"
				: "Transform image");
		ImGui::RadioButton("Flip horizontal", &m_flip_rotate_mode, 0);
		ImGui::RadioButton("Flip vertical", &m_flip_rotate_mode, 1);
		ImGui::RadioButton("Rotate", &m_flip_rotate_mode, 2);
		if (m_flip_rotate_mode == 2)
		{
			ImGui::Indent();
			ImGui::RadioButton("90 degrees", &m_flip_rotate_angle, 90);
			ImGui::SameLine();
			ImGui::RadioButton("180 degrees", &m_flip_rotate_angle, 180);
			ImGui::SameLine();
			ImGui::RadioButton("270 degrees", &m_flip_rotate_angle, 270);
			ImGui::Unindent();
		}
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			apply_flip_rotate();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Global shortcuts. Shortcut respects focus and text widgets, and does not
	// consume the key when there is no history available. Skip them while a
	// popup (menu, file dialog or modal) owns the input.
	const bool popup_open = ImGui::IsPopupOpen(
		"", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	if (!popup_open)
	{
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) && m_backend.can_undo())
		{
			m_backend.undo();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) && m_backend.can_redo())
		{
			m_backend.redo();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
		{
			request_new_document();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
		{
			request_open_document();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal) &&
		    (m_backend.is_modified() || !m_backend.has_file_path()))
		{
			request_save();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
		                    ImGuiInputFlags_RouteGlobal))
		{
			request_save_as();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_G,
		                    ImGuiInputFlags_RouteGlobal))
		{
			m_show_grid = !m_show_grid;
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A,
		                    ImGuiInputFlags_RouteGlobal))
		{
			select_all();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_R,
		                    ImGuiInputFlags_RouteGlobal))
		{
			// With a floating selection Ctrl+R transforms it; otherwise it
			// transforms the whole image. The Image menu always targets the
			// canvas.
			open_flip_rotate_dialog(
				m_backend.session().has_floating_region()
					? FlipRotateTarget::Selection
					: FlipRotateTarget::Canvas);
		}
		const bool zoom_in =
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal,
			                ImGuiInputFlags_RouteGlobal) ||
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Equal,
			                ImGuiInputFlags_RouteGlobal) ||
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadAdd,
			                ImGuiInputFlags_RouteGlobal);
		if (zoom_in)
		{
			request_zoom_steps(1);
		}
		const bool zoom_out =
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus,
			                ImGuiInputFlags_RouteGlobal) ||
			ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_KeypadSubtract,
			                ImGuiInputFlags_RouteGlobal);
		if (zoom_out)
		{
			request_zoom_steps(-1);
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E,
		                    ImGuiInputFlags_RouteGlobal))
		{
			open_properties_dialog();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C,
		                    ImGuiInputFlags_RouteGlobal) &&
		    m_backend.session().has_floating_region())
		{
			copy_selection();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X,
		                    ImGuiInputFlags_RouteGlobal) &&
		    m_backend.session().has_floating_region())
		{
			cut_selection();
		}
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V,
		                    ImGuiInputFlags_RouteGlobal) &&
		    m_backend.session().has_clipboard())
		{
			paste_selection();
		}
		if (m_backend.session().has_floating_region() &&
		    !ImGui::GetIO().WantTextInput &&
		    ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			m_backend.session().delete_floating_region();
		}
		if (m_backend.session().has_pending_edit())
		{
			if (ImGui::Shortcut(ImGuiKey_Enter, ImGuiInputFlags_RouteGlobal))
			{
				m_backend.session().commit_pending_edit();
			}
			if (ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteGlobal))
			{
				m_backend.session().cancel_pending_edit();
			}
		}
	}
}

void PaintFrontend::request_new_document()
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::New;
		m_pending_open_path.clear();
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	m_new_width = 256;
	m_new_height = 256;
	m_new_bpp = m_backend.session().getBitsPerPixel();
	m_open_new_popup = true;
}

void PaintFrontend::request_open_document()
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::Open;
		m_pending_open_path.clear();
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	open_open_dialog();
}

void PaintFrontend::request_open_path(std::string path)
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::OpenPath;
		m_pending_open_path = std::move(path);
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	m_pending_open_path.clear();
	open_recent_path(std::move(path));
}

void PaintFrontend::open_recent_path(std::string path)
{
	std::string error;
	if (!m_backend.open_document(path, error))
	{
		// Opening a recent entry is a single-shot action: report the error and
		// leave the current document untouched.
		show_file_error(error);
		return;
	}
	remember_file_directory(path);
}

void PaintFrontend::request_save()
{
	if (!m_backend.has_file_path())
	{
		request_save_as();
		return;
	}
	save_to_path(m_backend.file().path(), false);
}

void PaintFrontend::request_save_as()
{
	m_save_then_pending = false;
	open_save_as_dialog();
}

void PaintFrontend::request_export(ImageExportFormat format)
{
	const bool bitmap = format == ImageExportFormat::Bmp;
	const char* extension = bitmap ? ".bmp" : ".png";
	const char* dialog_key = bitmap ? "ExportBitmap" : "ExportPng";
	const char* title = bitmap ? "Export Bitmap" : "Export PNG";
	const char* filters = bitmap ? "Bitmap{.bmp}" : "PNG{.png}";

	std::string base_name = "untitled";
	std::string directory = file_dialog_directory();
	if (m_backend.has_file_path())
	{
		const std::filesystem::path document_path(m_backend.file().path());
		if (!document_path.stem().empty())
		{
			base_name = document_path.stem().string();
		}
		const std::filesystem::path parent = document_path.parent_path();
		if (!parent.empty() && accessible_directory(parent))
		{
			directory = parent.string();
		}
	}

	IGFD::FileDialogConfig config;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Default;
	config.path = directory;
	config.fileName = base_name + extension;
	ImGuiFileDialog::Instance()->OpenDialog(dialog_key, title, filters, config);
}

void PaintFrontend::export_to_path(const std::string& path,
                                   ImageExportFormat format)
{
	std::string error;
	if (!m_backend.export_image(path, format, error))
	{
		show_file_error(error);
		return;
	}
	remember_file_directory(path);
}

void PaintFrontend::copy_selection()
{
	if (m_backend.session().has_floating_region())
	{
		m_backend.session().copy_to_clipboard();
	}
}

void PaintFrontend::cut_selection()
{
	if (m_backend.session().has_floating_region())
	{
		m_backend.session().cut_to_clipboard();
	}
}

void PaintFrontend::paste_selection()
{
	if (!m_backend.session().has_clipboard())
	{
		return;
	}

	// Paste enters selection mode so the pasted region can use and toggle the
	// current selection transparency option.
	select_tool("Selection");
	m_backend.session().paste_from_clipboard(
		m_canvas_area.visible_image_origin());
	if (ToolBase* tool = m_backend.session().active_tool())
	{
		tool->sync_floating_region(m_backend.session());
	}
}

void PaintFrontend::select_all()
{
	// The same path as pressing the Selection button and dragging over the
	// whole canvas: the tool applies its current Normal/Transparent mode.
	select_tool("Selection");
	if (ToolBase* tool = m_backend.session().active_tool())
	{
		tool->select_all(m_backend.session());
	}
}

void PaintFrontend::open_properties_dialog()
{
	m_canvas_size_width = m_backend.session().getWidth();
	m_canvas_size_height = m_backend.session().getHeight();
	m_canvas_size_bpp = m_backend.session().getBitsPerPixel();
	m_open_properties_popup = true;
}

void PaintFrontend::open_flip_rotate_dialog(FlipRotateTarget target)
{
	if (target == FlipRotateTarget::Selection &&
	    !m_backend.session().has_floating_region())
	{
		return;
	}

	m_flip_rotate_target = target;
	m_flip_rotate_mode = 0;
	m_flip_rotate_angle = 90;
	m_open_flip_rotate_popup = true;
}

void PaintFrontend::apply_flip_rotate()
{
	using Transform = PaintCore::ImageTransform;

	Transform transform = Transform::FlipHorizontal;
	if (m_flip_rotate_mode == 1)
	{
		transform = Transform::FlipVertical;
	}
	else if (m_flip_rotate_mode == 2)
	{
		switch (m_flip_rotate_angle)
		{
			case 180:
				transform = Transform::Rotate180;
				break;
			case 270:
				transform = Transform::Rotate270;
				break;
			default:
				transform = Transform::Rotate90;
				break;
		}
	}

	if (m_flip_rotate_target == FlipRotateTarget::Selection)
	{
		m_backend.session().transform_floating_region(transform);
	}
	else
	{
		// The session commits a floating region/pending edit first, then
		// records the canvas transform as a second history step.
		m_backend.transform_canvas(transform);
	}
}

void PaintFrontend::request_zoom_steps(int steps)
{
	m_zoom_key_steps += steps;
}

void PaintFrontend::apply_zoom_request()
{
	int target = m_zoom;
	bool has_target = false;
	ImVec2 anchor{};

	if (m_zoom_slider_target >= 0)
	{
		target = m_zoom_slider_target;
		anchor = m_canvas_area.visible_image_center();
		has_target = true;
	}
	else if (m_zoom_key_steps != 0 || m_zoom_wheel_steps != 0)
	{
		target = std::clamp(m_zoom + m_zoom_key_steps + m_zoom_wheel_steps,
		                    1, 64);
		if (m_zoom_wheel_steps != 0)
		{
			anchor = m_zoom_wheel_anchor;
		}
		else if (m_canvas_area.mouse_over_canvas() &&
		         m_canvas_area.pointer_over_image(ImGui::GetMousePos()))
		{
			anchor = ImGui::GetMousePos();
		}
		else
		{
			anchor = m_canvas_area.visible_image_center();
		}
		has_target = true;
	}

	if (has_target && target != m_zoom)
	{
		ImVec2 scroll{};
		if (m_canvas_area.scroll_for_zoom_anchor(
				anchor, static_cast<float>(target), scroll))
		{
			m_pending_canvas_scroll = scroll;
			m_apply_pending_canvas_scroll = true;
		}
		else
		{
			m_apply_pending_canvas_scroll = false;
		}
		m_zoom = target;
	}

	m_zoom_key_steps = 0;
	m_zoom_wheel_steps = 0;
	m_zoom_slider_target = -1;
}

void PaintFrontend::request_close_window()
{
	if (m_backend.is_modified())
	{
		m_pending_file_action = FileAction::Quit;
		m_pending_open_path.clear();
		m_open_unsaved_popup = true;
		return;
	}

	m_should_close = true;
}

void PaintFrontend::perform_pending_file_action()
{
	const FileAction action = m_pending_file_action;
	const std::string open_path = std::move(m_pending_open_path);
	m_pending_file_action = FileAction::None;
	m_pending_open_path.clear();

	switch (action)
	{
		case FileAction::New:
			m_new_width = 256;
			m_new_height = 256;
			m_new_bpp = m_backend.session().getBitsPerPixel();
			m_open_new_popup = true;
			break;
		case FileAction::Open:
			open_open_dialog();
			break;
		case FileAction::OpenPath:
			open_recent_path(open_path);
			break;
		case FileAction::Quit:
			m_should_close = true;
			break;
		case FileAction::None:
			break;
	}
}

void PaintFrontend::open_open_dialog()
{
	IGFD::FileDialogConfig config;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Modal |
	               ImGuiFileDialogFlags_HideColumnType;
	if (m_backend.has_file_path())
	{
		const std::filesystem::path parent = std::filesystem::path(
			m_backend.file().path()).parent_path();
		if (!parent.empty() && accessible_directory(parent))
		{
			config.path = parent.string();
		}
		else
		{
			config.path = file_dialog_directory();
		}
	}
	else
	{
		config.path = file_dialog_directory();
	}
	ImGuiFileDialog::Instance()->OpenDialog(
		"OpenImage", "Open Image",
		"Indexed Image{.idx},All files{.*}", config);
}

void PaintFrontend::open_save_as_dialog()
{
	IGFD::FileDialogConfig config;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Default;
	if (m_backend.has_file_path())
	{
		const std::filesystem::path document_path(m_backend.file().path());
		const std::filesystem::path parent = document_path.parent_path();
		if (!parent.empty() && accessible_directory(parent))
		{
			config.filePathName = m_backend.file().path();
		}
		else
		{
			config.path = file_dialog_directory();
			config.fileName = document_path.filename().string();
		}
	}
	else
	{
		config.path = file_dialog_directory();
		config.fileName = "untitled.idx";
	}
	ImGuiFileDialog::Instance()->OpenDialog(
		"SaveImage", "Save Image", ".idx", config);
}

void PaintFrontend::save_to_path(const std::string& path, bool continue_pending)
{
	save_to_path(path, continue_pending, m_backend.file_format());
}

void PaintFrontend::save_to_path(const std::string& path, bool continue_pending,
                                 IndexedFormat format)
{
	std::string error;
	if (!m_backend.save_document_as(path, format, error))
	{
		m_save_then_pending = false;
		m_pending_file_action = FileAction::None;
		m_pending_open_path.clear();
		show_file_error(error);
		return;
	}

	remember_file_directory(path);

	if (continue_pending)
	{
		perform_pending_file_action();
	}
}

void PaintFrontend::show_file_error(const std::string& message)
{
	m_error_message = message.empty() ? "Unknown error" : message;
	m_open_error_popup = true;
}

void PaintFrontend::renderFileDialogs()
{
	if (m_open_new_popup)
	{
		m_open_new_popup = false;
		ImGui::OpenPopup("New Image");
	}
	if (m_open_unsaved_popup)
	{
		m_open_unsaved_popup = false;
		ImGui::OpenPopup("Unsaved Changes");
	}
	if (m_open_error_popup)
	{
		m_open_error_popup = false;
		ImGui::OpenPopup("File Error");
	}

	if (ImGuiFileDialog::Instance()->Display("OpenImage",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string error;
			const std::string path =
				ImGuiFileDialog::Instance()->GetFilePathName();
			if (!m_backend.open_document(path, error))
			{
				show_file_error(error);
			}
			else
			{
				remember_file_directory(path);
			}
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("SaveImage",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			m_pending_save_path = ensure_idx_extension(
				ImGuiFileDialog::Instance()->GetFilePathName());
			m_save_format = m_backend.file_format();
			m_open_idx_options_popup = true;
		}
		else
		{
			m_save_then_pending = false;
			if (m_pending_file_action != FileAction::None)
			{
				m_pending_file_action = FileAction::None;
			}
			m_pending_open_path.clear();
			m_pending_save_path.clear();
			m_open_idx_options_popup = false;
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("ExportBitmap",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			const std::string path = ensure_extension(
				ImGuiFileDialog::Instance()->GetFilePathName(), ".bmp");
			export_to_path(path, ImageExportFormat::Bmp);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("ExportPng",
	                                         ImGuiWindowFlags_NoCollapse,
	                                         ImVec2(720.0f, 480.0f),
	                                         ImVec2(FLT_MAX, FLT_MAX)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			const std::string path = ensure_extension(
				ImGuiFileDialog::Instance()->GetFilePathName(), ".png");
			export_to_path(path, ImageExportFormat::Png);
		}
		ImGuiFileDialog::Instance()->Close();
	}

	if (m_open_idx_options_popup)
	{
		m_open_idx_options_popup = false;
		if (!m_pending_save_path.empty())
		{
			ImGui::OpenPopup("IDX Options");
		}
	}

	if (ImGui::BeginPopupModal("IDX Options", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Storage format for the .idx file:");
		ImGui::TextUnformatted("Format");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(180.0f);
		static const char* formats[] = {
			"Index", "Binary", "Packed", "BinaryPacked"};
		int format_index = static_cast<int>(m_save_format);
		if (ImGui::Combo("##idx_format", &format_index, formats, 4))
		{
			m_save_format = static_cast<IndexedFormat>(format_index);
		}

		ImGui::BeginChild("##idx_format_help", ImVec2(470.0f, 64.0f), true);
		ImGui::TextWrapped("%s", indexed_format_description(m_save_format));
		ImGui::EndChild();

		const int width = m_backend.session().getWidth();
		const int height = m_backend.session().getHeight();
		const int bpp = m_backend.session().getBitsPerPixel();
		ImGui::TextUnformatted("Approx. file sizes:");
		if (ImGui::BeginTable("##idx_sizes", 2,
		                      ImGuiTableFlags_SizingFixedFit |
		                          ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Format");
			ImGui::TableSetupColumn("Approx. size");
			ImGui::TableHeadersRow();
			for (IndexedFormat format : {IndexedFormat::Index,
			                             IndexedFormat::Binary,
			                             IndexedFormat::Packed,
			                             IndexedFormat::BinaryPacked})
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(indexed_format_name(format));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(format_byte_size(
					indexed_image_estimated_file_size(
						width, height, bpp, format)).c_str());
			}
			ImGui::EndTable();
		}
		const std::size_t data_size = indexed_image_data_size(
			width, height, bpp, m_save_format);
		ImGui::TextDisabled("Selected data: %s + ~64 B header",
		                    format_byte_size(data_size).c_str());

		ImGui::Separator();
		if (ImGui::Button("Save", ImVec2(80.0f, 0.0f)))
		{
			const std::string path = std::move(m_pending_save_path);
			m_pending_save_path.clear();
			const bool continue_pending = m_save_then_pending;
			m_save_then_pending = false;
			ImGui::CloseCurrentPopup();
			save_to_path(path, continue_pending, m_save_format);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			m_save_then_pending = false;
			m_pending_file_action = FileAction::None;
			m_pending_open_path.clear();
			m_pending_save_path.clear();
		}
		ImGui::EndPopup();
	}

	// If the modal was dismissed without pressing Save/Cancel, discard the
	// pending path so the next Save/Save As starts cleanly.
	if (!m_open_idx_options_popup && !m_pending_save_path.empty() &&
	    !ImGui::IsPopupOpen("IDX Options"))
	{
		m_pending_save_path.clear();
		m_save_then_pending = false;
		m_pending_file_action = FileAction::None;
		m_pending_open_path.clear();
	}

	if (ImGui::BeginPopupModal("New Image", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputInt("Width", &m_new_width);
		ImGui::InputInt("Height", &m_new_height);
		m_new_width = std::max(1, m_new_width);
		m_new_height = std::max(1, m_new_height);
		ImGui::SliderInt("Bits per pixel", &m_new_bpp, 1, 8);
		m_new_bpp = std::clamp(m_new_bpp, 1, 8);
		ImGui::Text("Colors: %d", palette_usable_color_count(m_new_bpp));
		ImGui::Separator();

		if (ImGui::Button("Create", ImVec2(80.0f, 0.0f)))
		{
			std::string error;
			if (m_backend.new_document(m_new_width, m_new_height,
			                           m_new_bpp, error))
			{
				ImGui::CloseCurrentPopup();
			}
			else
			{
				ImGui::CloseCurrentPopup();
				show_file_error(error);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		const std::string name = m_backend.file_display_name();
		const char* question = m_pending_file_action == FileAction::Quit
			? "before closing?"
			: "before continuing?";
		ImGui::Text("Save changes to \"%s\" %s", name.c_str(), question);
		ImGui::Separator();

		if (ImGui::Button("Save", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			if (!m_backend.has_file_path())
			{
				m_save_then_pending = true;
				open_save_as_dialog();
			}
			else
			{
				std::string error;
				if (m_backend.save_document(error))
				{
					perform_pending_file_action();
				}
				else
				{
					m_pending_file_action = FileAction::None;
					m_pending_open_path.clear();
					show_file_error(error);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			perform_pending_file_action();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			m_pending_file_action = FileAction::None;
			m_pending_open_path.clear();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("File Error", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("%s", m_error_message.c_str());
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void PaintFrontend::renderToolbar()
{
	int index = 0;

	for (const auto& btn : m_backend.buttons())
	{
		if (index % kToolbarColumns != 0)
		{
			ImGui::SameLine();
		}

		const bool active = (m_backend.activeButton() == btn.name);
		// Inactive tools blend with the toolbar background; only the selected
		// tool keeps the theme's ButtonActive blue. Hover/press use a neutral
		// translucent text color instead of ImGui's button background.
		ImU32 button_color = IM_COL32(0, 0, 0, 0);
		ImU32 hover_color = ImGui::GetColorU32(ImGuiCol_Text, 0.08f);
		ImU32 pressed_color = ImGui::GetColorU32(ImGuiCol_Text, 0.14f);
		if (active)
		{
			const ImU32 active_color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
			button_color = active_color;
			hover_color = active_color;
			pressed_color = active_color;
		}
		ImGui::PushStyleColor(ImGuiCol_Button, button_color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover_color);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressed_color);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		const ImVec2 buttonSize(kToolButtonSize, kToolButtonSize);
		const bool clicked = btn.icon
			? ImGui::ImageButton(btn.name.c_str(), btn.icon, buttonSize)
			: ImGui::Button(btn.name.c_str(), buttonSize);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", btn.name.c_str());
		}

		if (clicked)
		{
			select_tool(btn.name);
		}

		++index;
	}

	ImGui::Spacing();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 8.0f));
	ImGui::BeginChild("ToolOptions", ImVec2(0.0f, kToolOptionsHeight), true,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderToolOptions();
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void PaintFrontend::renderToolOptions()
{
	ToolBase* tool = m_backend.session().active_tool();
	if (!tool)
	{
		return;
	}

	// The eyedropper previews the color under the pointer by filling the whole
	// options area while a button is held, like Paint's color box.
	PaintCore::ColorIndex preview_color = 0;
	if (tool->color_pick_preview(preview_color))
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 window_pos = ImGui::GetWindowPos();
		const ImVec2 window_size = ImGui::GetWindowSize();
		const float border = style.ChildBorderSize;
		const ImVec2 fill_min(window_pos.x + border, window_pos.y + border);
		const ImVec2 fill_max(window_pos.x + window_size.x - border,
		                      window_pos.y + window_size.y - border);
		if (m_backend.palette().is_transparent(preview_color))
		{
			draw_checkerboard(draw_list, fill_min, fill_max, fill_min);
		}
		else
		{
			draw_list->AddRectFilled(
				fill_min, fill_max,
				toImU32(m_backend.palette().getColor(preview_color)));
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	for (const ToolOption& option : tool->options())
	{
		if (!option.visible_in_options_bar)
		{
			continue;
		}

		ImGui::PushID(option.id.c_str());
		switch (option.type)
		{
			case ToolOptionType::Choice:
			{
				if (option.grid_columns > 1)
				{
					const int columns = option.grid_columns;
					const int count =
						static_cast<int>(option.choices.size());
					const int rows = (count + columns - 1) / columns;
					const ImVec2 grid_pos = ImGui::GetCursorScreenPos();
					const float grid_width = ImGui::GetContentRegionAvail().x;
					const float cell_width = grid_width / columns;
					const float cell_height = kToolOptionRowHeight;

					float reference_extent = 1.0f;
					for (const ToolOptionChoice& choice : option.choices)
					{
						reference_extent = std::max(
							reference_extent,
							choice_preview_extent(choice.preview));
					}

					for (int index = 0; index < count; ++index)
					{
						const ToolOptionChoice& choice =
							option.choices[static_cast<std::size_t>(index)];
						const ImVec2 cell_min(
							grid_pos.x + (index % columns) * cell_width,
							grid_pos.y + (index / columns) * cell_height);
						const ImVec2 cell_max(
							cell_min.x + cell_width,
							cell_min.y + cell_height);

						ImGui::SetCursorScreenPos(cell_min);
						ImGui::PushID(choice.value);
						ImGui::InvisibleButton(
							"##option_cell", ImVec2(cell_width, cell_height));
						const bool clicked = ImGui::IsItemClicked();
						const bool hovered = ImGui::IsItemHovered();

						if (clicked)
						{
							tool->set_option(option.id, choice.value);
						}

						ImDrawList* draw_list = ImGui::GetWindowDrawList();
						const bool selected = (option.value == choice.value);
						if (selected)
						{
							draw_list->AddRectFilled(
								cell_min, cell_max,
								ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.65f));
						}
						else if (hovered)
						{
							draw_list->AddRectFilled(
								cell_min, cell_max, IM_COL32(255, 255, 255, 18));
						}

						if (!choice.preview.empty())
						{
							draw_choice_preview(
								draw_list, choice.preview, cell_min, cell_max,
								reference_extent,
								ImGui::GetColorU32(ImGuiCol_Text));
						}
						else
						{
							const ImVec2 text_size =
								ImGui::CalcTextSize(choice.short_label.c_str());
							const ImVec2 text_pos(
								cell_min.x + (cell_width - text_size.x) * 0.5f,
								cell_min.y + (cell_height - text_size.y) * 0.5f);
							draw_list->AddText(
								text_pos, ImGui::GetColorU32(ImGuiCol_Text),
								choice.short_label.c_str());
						}

						if (hovered)
						{
							ImGui::SetTooltip("%s", choice.label.c_str());
						}

						ImGui::PopID();
					}

					ImGui::SetCursorScreenPos(
						ImVec2(grid_pos.x, grid_pos.y + rows * cell_height));
					break;
				}

				float list_reference_extent = 1.0f;
				for (const ToolOptionChoice& choice : option.choices)
				{
					list_reference_extent = std::max(
						list_reference_extent,
						choice_preview_extent(choice.preview));
				}

				// Icon-based options distribute the full options area between
				// their rows; preview-based lists keep rows compact but shrink
				// them when there are more than fit at the default height
				// (Line/Curve show five widths).
				const bool icon_list =
					!option.choices.empty() &&
					option.choices.front().preview.empty();
				float row_height = kToolOptionRowHeight;
				if (!option.choices.empty())
				{
					const float available_height =
						ImGui::GetContentRegionAvail().y;
					if (available_height > 0.0f)
					{
						const float distributed = std::max(
							1.0f,
							available_height /
								static_cast<float>(option.choices.size()));
						row_height = icon_list
							? distributed
							: std::min(kToolOptionRowHeight, distributed);
					}
				}

				for (const ToolOptionChoice& choice : option.choices)
				{
					ImGui::PushID(choice.value);

					const ImVec2 row_pos = ImGui::GetCursorScreenPos();
					const ImVec2 row_size(ImGui::GetContentRegionAvail().x,
					                      row_height);
					ImGui::InvisibleButton("##option_row", row_size);
					const bool clicked = ImGui::IsItemClicked();
					const bool hovered = ImGui::IsItemHovered();

					if (clicked)
					{
						tool->set_option(option.id, choice.value);
					}

					ImDrawList* draw_list = ImGui::GetWindowDrawList();
					const bool selected = (option.value == choice.value);
					if (selected)
					{
						draw_list->AddRectFilled(
							row_pos, ImVec2(row_pos.x + row_size.x, row_pos.y + row_size.y),
							ImGui::GetColorU32(ImGuiCol_HeaderActive, 0.65f));
					}
					else if (hovered)
					{
						draw_list->AddRectFilled(
							row_pos, ImVec2(row_pos.x + row_size.x, row_pos.y + row_size.y),
							IM_COL32(255, 255, 255, 18));
					}

					if (!choice.preview.empty())
					{
						draw_choice_preview(
							draw_list, choice.preview, row_pos,
							ImVec2(row_pos.x + row_size.x,
							       row_pos.y + row_size.y),
							list_reference_extent,
							ImGui::GetColorU32(ImGuiCol_Text));

						const ImVec2 text_size =
							ImGui::CalcTextSize(choice.short_label.c_str());
						const ImVec2 text_pos(
							row_pos.x + row_size.x - text_size.x - 6.0f,
							row_pos.y + (row_size.y - text_size.y) * 0.5f);
						draw_list->AddText(
							text_pos, ImGui::GetColorU32(ImGuiCol_Text),
							choice.short_label.c_str());
					}
					else
					{
						const ImTextureID icon = m_backend.optionIcon(choice.icon);
						const float icon_width = icon_list
							? std::max(1.0f, row_size.x - 4.0f)
							: std::min(kToolOptionIconWidth,
							           std::max(1.0f, row_size.x - 4.0f));
						const float icon_height = icon_list
							? std::max(1.0f, row_size.y - 4.0f)
							: kToolOptionIconHeight;
						const ImVec2 icon_pos(
							row_pos.x + (row_size.x - icon_width) * 0.5f,
							row_pos.y + (row_size.y - icon_height) * 0.5f);
						if (icon)
						{
							draw_list->AddImage(
								icon, icon_pos,
								ImVec2(icon_pos.x + icon_width,
								       icon_pos.y + icon_height));
						}
						else
						{
							const ImVec2 text_size =
								ImGui::CalcTextSize(choice.short_label.c_str());
							const ImVec2 text_pos(
								row_pos.x + (row_size.x - text_size.x) * 0.5f,
								row_pos.y + (row_size.y - text_size.y) * 0.5f);
							draw_list->AddText(
								text_pos,
								ImGui::GetColorU32(ImGuiCol_Text),
								choice.short_label.c_str());
						}
					}

					if (hovered)
					{
						ImGui::SetTooltip("%s", choice.label.c_str());
					}

					ImGui::PopID();
				}
				break;
			}
			case ToolOptionType::Integer:
			{
				int value = option.value;
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::SliderInt("##value", &value,
				                     option.min_value, option.max_value))
				{
					tool->set_option(option.id, value);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", option.label.c_str());
				}
				break;
			}
		}
		ImGui::PopID();
	}

	ImGui::PopStyleVar();
	tool->sync_floating_region(m_backend.session());
}

void PaintFrontend::renderPalette()
{
	const int colorCount = m_backend.palette_visual_count();
	if (colorCount <= 0)
	{
		return;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const int rows = palette_row_count(colorCount);
	const int columns = palette_column_count(colorCount, rows);
	const float gridWidth = compute_palette_grid_width(style);
	const float cellWidth =
		(gridWidth - (columns - 1) * style.ItemSpacing.x) / columns;
	const float rowGap = style.ItemSpacing.y;
	const float gridHeight = kPaletteColorSize;
	const float cellHeight = std::max(
		1.0f, (gridHeight - (rows - 1) * rowGap) / rows);

	const ImVec2 swatchPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float swatchSize = kPaletteColorSize;
	const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
	const ImU32 bevelLight = IM_COL32(255, 255, 255, 255);
	const ImU32 bevelDark = IM_COL32(0, 0, 0, 255);
	const ImVec2 swatchEnd(
		swatchPos.x + swatchSize,
		swatchPos.y + swatchSize);

	ImGui::InvisibleButton("##fg_bg_swatch", ImVec2(swatchSize, swatchSize));

	auto draw_beveled_rect = [&](const ImVec2& pos, const ImVec2& end,
	                             ImU32 fill, bool transparent)
	{
		if (transparent)
		{
			draw_checkerboard(drawList, pos, end, pos);
		}
		else
		{
			drawList->AddRectFilled(pos, end, fill);
		}
		drawList->AddLine(
			ImVec2(pos.x + 1.0f, pos.y + 1.0f),
			ImVec2(end.x - 1.0f, pos.y + 1.0f),
			bevelLight, kSwatchBevel);
		drawList->AddLine(
			ImVec2(pos.x + 1.0f, pos.y + 1.0f),
			ImVec2(pos.x + 1.0f, end.y - 1.0f),
			bevelLight, kSwatchBevel);
		drawList->AddLine(
			ImVec2(pos.x + 1.0f, end.y - 1.0f),
			ImVec2(end.x - 1.0f, end.y - 1.0f),
			bevelDark, kSwatchBevel);
		drawList->AddLine(
			ImVec2(end.x - 1.0f, pos.y + 1.0f),
			ImVec2(end.x - 1.0f, end.y - 1.0f),
			bevelDark, kSwatchBevel);
		drawList->AddRect(pos, end, borderColor);
	};

	draw_beveled_rect(swatchPos, swatchEnd,
		ImGui::GetColorU32(ImGuiCol_FrameBg), false);

	const float swatchOffset = swatchSize - kSwatchGap - kSwatchSize;
	const ImVec2 fgPos(swatchPos.x + kSwatchGap, swatchPos.y + kSwatchGap);
	const ImVec2 bgPos(swatchPos.x + swatchOffset, swatchPos.y + swatchOffset);
	const auto draw_swatch = [&](const ImVec2& pos,
	                             const PaintCore::Color& color,
	                             PaintCore::ColorIndex index)
	{
		draw_beveled_rect(pos, ImVec2(pos.x + kSwatchSize, pos.y + kSwatchSize),
			toImU32(color), m_backend.palette().is_transparent(index));
	};

	draw_swatch(bgPos, m_backend.palette().getColor(
		m_backend.background_index()), m_backend.background_index());
	draw_swatch(fgPos, m_backend.palette().getColor(
		m_backend.foreground_index()), m_backend.foreground_index());

	const ImVec2 gridPos(
		swatchPos.x + swatchSize + style.ItemSpacing.x,
		swatchPos.y);

	for (int i = 0; i < colorCount; ++i)
	{
		const int column = i % columns;
		const int row = i / columns;
		const ImVec2 cellPos(
			gridPos.x + column * (cellWidth + style.ItemSpacing.x),
			gridPos.y + row * (cellHeight + rowGap));

		const PaintCore::ColorIndex colorIndex =
			m_backend.palette_visual_index(i);
		const bool transparent =
			m_backend.palette().is_transparent(colorIndex);

		ImGui::SetCursorScreenPos(cellPos);
		ImGui::InvisibleButton(
			("##palette" + std::to_string(i)).c_str(),
			ImVec2(cellWidth, cellHeight));

		draw_beveled_rect(cellPos,
			ImVec2(cellPos.x + cellWidth, cellPos.y + cellHeight),
			toImU32(m_backend.palette().getColor(colorIndex)), transparent);

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			m_backend.set_foreground_index(colorIndex);
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_backend.set_background_index(colorIndex);
		}
		if (transparent && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Transparent color (%d)",
			                  static_cast<int>(colorIndex));
		}
	}

	const float bottom = std::max(swatchPos.y + swatchSize, gridPos.y + gridHeight) +
		style.ItemSpacing.y;

	const float paletteContentWidth =
		compute_palette_panel_width(style) - style.WindowPadding.x * 2.0f;
	const float zoomLabelWidth = ImGui::CalcTextSize("Zoom").x;
	const float zoomSliderWidth = std::max(
		1.0f, paletteContentWidth - style.ItemInnerSpacing.x - zoomLabelWidth);

	ImGui::SetCursorScreenPos(ImVec2(swatchPos.x, bottom));
	ImGui::SetNextItemWidth(zoomSliderWidth);
	int zoom = m_zoom;
	if (ImGui::SliderInt("Zoom", &zoom, 1, 64))
	{
		m_zoom_slider_target = zoom;
	}
}

void PaintFrontend::renderStatusBar()
{
	const ImGuiStyle& style = ImGui::GetStyle();
	const float statusHeight = compute_status_bar_height(style);
	ImGui::BeginChild("Status", ImVec2(0.0f, statusHeight), true,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 mouse = ImGui::GetMousePos();
	const bool over_image = m_canvas_area.mouse_over_canvas() &&
	                        m_canvas_area.pointer_over_image(mouse);
	const PaintCore::Vec2 pos = m_canvas_area.image_position_at(mouse);

	PaintCore::Vec2 anchor{};
	bool has_anchor = false;
	if (ToolBase* tool = m_backend.session().active_tool())
	{
		has_anchor = tool->drag_anchor(anchor);
	}

	const float gap = style.ItemSpacing.x * 2.0f;
	const auto separator = [&]()
	{
		ImGui::SameLine(0.0f, gap);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0.0f, gap);
	};

	ImGui::Text("Canvas: %dx%d",
	            m_backend.session().getWidth(),
	            m_backend.session().getHeight());
	separator();
	ImGui::Text("Zoom: %dx", m_zoom);
	separator();

	if (has_anchor)
	{
		// Two-point operations keep the press position fixed and show the
		// pointer position until the operation is confirmed or cancelled.
		ImGui::Text("X: %d  Y: %d  ->  X: %d  Y: %d",
		            anchor.x, anchor.y, pos.x, pos.y);
	}
	else if (over_image)
	{
		ImGui::Text("X: %d  Y: %d", pos.x, pos.y);
	}
	else
	{
		ImGui::TextDisabled("X: --  Y: --");
	}
	separator();

	PaintCore::Vec2 pixel{};
	PaintCore::ColorIndex color = 0;
	if (m_canvas_area.image_pixel_at(mouse, pixel, color))
	{
		const float swatch = ImGui::GetTextLineHeight();
		const ImVec2 swatch_pos = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(swatch, swatch));
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::Text("Color: %d", static_cast<int>(color));

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec2 swatch_end(swatch_pos.x + swatch, swatch_pos.y + swatch);
		if (m_backend.palette().is_transparent(color))
		{
			draw_checkerboard(draw_list, swatch_pos, swatch_end, swatch_pos);
		}
		else
		{
			draw_list->AddRectFilled(
				swatch_pos, swatch_end,
				toImU32(m_backend.palette().getColor(color)));
		}
		draw_list->AddRect(swatch_pos, swatch_end,
		                   ImGui::GetColorU32(ImGuiCol_Border));
	}
	else
	{
		ImGui::TextDisabled("Color: --");
	}
	separator();
	ImGui::TextUnformatted(m_backend.is_modified() ? "Modified" : "Saved");

	ImGui::EndChild();
}

void PaintFrontend::renderCanvas()
{
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const int content_width = std::max(
		m_backend.session().getWidth(),
		m_canvas_area.resize_preview_width());
	const int content_height = std::max(
		m_backend.session().getHeight(),
		m_canvas_area.resize_preview_height());
	ImVec2 content_size(
		content_width * m_zoom + kCanvasResizeMargin,
		content_height * m_zoom + kCanvasResizeMargin);

	// A single transformation per frame, shared by input and render.
	ImGui::InvisibleButton("canvas_area", content_size);

	ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
	m_canvas_area.update_view(canvasPos, canvasSize, scroll, static_cast<float>(m_zoom));
	m_canvas_area.set_resize_fill(m_backend.background_index());
	m_canvas_area.set_grid_enabled(m_show_grid);

	// The cursor belongs to the selected toolbar button and is only shown
	// while the pointer is over the image (not the gray margin). The eraser
	// uses an invisible cursor; resize/floating-region handlers can override
	// it later in handle_input().
	const bool canvas_hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
	const bool canvas_tool_cursor =
		canvas_hovered &&
		m_canvas_area.pointer_over_image(ImGui::GetMousePos()) &&
		!(m_backend.activeCursor() == PaintCore::CursorType::Eraser &&
		  m_backend.session().has_floating_region());
	if (IGuiBackend* backend = m_backend.backend())
	{
		backend->set_cursor(canvas_tool_cursor ? m_backend.activeCursor()
		                                       : PaintCore::CursorType::Arrow);
	}

	m_canvas_area.handle_input(ImGui::GetMousePos());
	m_backend.apply_pending_color_pick();

	// Ctrl+wheel zooms around the pointer. ImGui already skips its normal
	// wheel scrolling while Ctrl is held, so both behaviors do not conflict.
	const ImGuiIO& io = ImGui::GetIO();
	if (m_canvas_area.mouse_over_canvas() && io.KeyCtrl)
	{
		if (io.MouseWheel != 0.0f)
		{
			m_zoom_wheel_accum += io.MouseWheel;
			const int steps = static_cast<int>(m_zoom_wheel_accum);
			if (steps != 0)
			{
				m_zoom_wheel_steps += steps;
				m_zoom_wheel_anchor = io.MousePos;
				m_zoom_wheel_accum -= static_cast<float>(steps);
			}
		}
	}
	else
	{
		m_zoom_wheel_accum = 0.0f;
	}

	m_canvas_area.render(drawList, m_backend.palette(),
	                     m_backend.foreground_index(),
	                     m_backend.background_index());

	// The grid can be drawn on top here using drawList.
	// drawGrid(drawList, canvasPos, canvas.getWidth(), canvas.getHeight(), zoom);

}
