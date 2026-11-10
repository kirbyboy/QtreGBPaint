#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <string>
#include <memory>
#include <vector>

#include "paint/paint_frontend.h"
#include "paint/color_imgui.h"
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

std::string ensure_idx_extension(std::string path)
{
	if (path.empty())
	{
		return path;
	}
	std::filesystem::path file_path(path);
	if (file_path.extension() != ".idx")
	{
		file_path.replace_extension(".idx");
	}
	return file_path.string();
}
}

PaintFrontend::PaintFrontend(PaintBackend& backend) :
m_backend(backend),
m_canvas_area(backend.backend(), backend.session())
{
}

void PaintFrontend::select_tool(const std::string& name)
{
	m_backend.selectButton(name);
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

void PaintFrontend::render()
{
	if (!m_theme_applied)
	{
		apply_theme_style();
		m_theme_applied = true;
	}

	// Full window with menu bar
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
	ImGui::Begin("PaintGB", nullptr,
				 ImGuiWindowFlags_NoCollapse |
				 ImGuiWindowFlags_NoResize |
				 ImGuiWindowFlags_NoMove |
				 ImGuiWindowFlags_NoScrollbar |
				 ImGuiWindowFlags_NoScrollWithMouse |
				 ImGuiWindowFlags_MenuBar);

	renderMenuBar();

	// Compute available space for layout
	ImVec2 contentRegion = ImGui::GetContentRegionAvail();
	const ImGuiStyle& style = ImGui::GetStyle();
	const float columnGap = style.ItemSpacing.x;
	const float palettePanelWidth = compute_palette_panel_width(style);
	const float paletteHeight = compute_palette_height(style);
	const ToolbarLayout toolbar = compute_toolbar_layout(
		static_cast<int>(m_backend.buttons().size()), style);
	const float toolbarHeight = std::max(toolbar.contentHeight, contentRegion.y - paletteHeight);

	const float minCanvasWidth = kMinCanvasWidth + style.WindowPadding.x * 2.0f;
	const float minContentWidth = std::max(
		toolbar.width + columnGap + minCanvasWidth, palettePanelWidth);
	const float chromeWidth = ImGui::GetWindowWidth() - contentRegion.x;
	const float chromeHeight = ImGui::GetWindowHeight() - contentRegion.y;
	const int minWindowWidth = static_cast<int>(std::ceil(
		chromeWidth + minContentWidth));
	const int minWindowHeight = static_cast<int>(std::ceil(
		chromeHeight + toolbar.contentHeight + paletteHeight + style.ItemSpacing.y));
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
	                             contentRegion.y - paletteHeight);

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

	ImGui::BeginChild("Canvas", canvasRegion, true, ImGuiWindowFlags_HorizontalScrollbar);
	renderCanvas();
	ImGui::EndChild();

	// --- Palette area ---
	ImGui::BeginChild("Palette", ImVec2(contentRegion.x, paletteHeight), true,
	                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderPalette();
	ImGui::EndChild();

	ImGui::End();

	renderFileDialogs();
}

void PaintFrontend::renderMenuBar()
{
	PaintSession& session = m_backend.session();

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			std::string title = m_backend.file_display_name();
			if (m_backend.is_modified())
			{
				title += " *";
			}
			ImGui::TextDisabled("%s", title.c_str());
			ImGui::Separator();

			if (ImGui::MenuItem("New"))
			{
				request_new_document();
			}
			if (ImGui::MenuItem("Open"))
			{
				request_open_document();
			}
			ImGui::Separator();
			const bool can_save =
				m_backend.is_modified() || !m_backend.has_file_path();
			if (ImGui::MenuItem("Save", nullptr, false, can_save))
			{
				request_save();
			}
			if (ImGui::MenuItem("Save As..."))
			{
				request_save_as();
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
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Image"))
		{
			ImGui::MenuItem("Grid", nullptr, &m_show_grid);
			ImGui::Separator();
			if (ImGui::MenuItem("Canvas Size..."))
			{
				m_canvas_size_width = session.getWidth();
				m_canvas_size_height = session.getHeight();
				m_open_canvas_size_popup = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Selection"))
		{
			const bool has_floating = session.has_floating_region();
			if (ImGui::MenuItem("Copy", nullptr, false, has_floating))
			{
				session.copy_to_clipboard();
			}
			if (ImGui::MenuItem("Cut", nullptr, false, has_floating))
			{
				session.cut_to_clipboard();
			}
			if (ImGui::MenuItem("Paste", nullptr, false, session.has_clipboard()))
			{
				// Paste enters selection mode so the pasted region can use
				// and toggle the current selection transparency option.
				select_tool("Selection");
				session.paste_from_clipboard(
					m_canvas_area.visible_image_origin());
				if (ToolBase* tool = session.active_tool())
				{
					tool->sync_floating_region(session);
				}
			}
			if (ImGui::MenuItem("Delete", "Del", false, has_floating))
			{
				session.delete_floating_region();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Invert Colors", nullptr, false, has_floating))
			{
				session.invert_floating_region_colors();
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

				theme_item("Dark", Theme::Dark);
				theme_item("Light", Theme::Light);
				theme_item("Classic", Theme::Classic);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	if (m_open_canvas_size_popup)
	{
		ImGui::OpenPopup("Canvas Size");
		m_open_canvas_size_popup = false;
	}

	if (ImGui::BeginPopupModal("Canvas Size", nullptr,
	                           ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputInt("Width", &m_canvas_size_width);
		ImGui::InputInt("Height", &m_canvas_size_height);
		m_canvas_size_width = std::max(1, m_canvas_size_width);
		m_canvas_size_height = std::max(1, m_canvas_size_height);
		ImGui::TextDisabled("New area uses the current background color.");

		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
		{
			m_backend.resize_canvas(m_canvas_size_width, m_canvas_size_height);
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
		m_open_unsaved_popup = true;
		return;
	}

	m_pending_file_action = FileAction::None;
	open_open_dialog();
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

void PaintFrontend::perform_pending_file_action()
{
	const FileAction action = m_pending_file_action;
	m_pending_file_action = FileAction::None;

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
		const std::string parent = std::filesystem::path(
			m_backend.file().path()).parent_path().string();
		config.path = parent.empty() ? "." : parent;
	}
	else
	{
		config.path = ".";
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
		config.filePathName = m_backend.file().path();
	}
	else
	{
		config.path = ".";
		config.fileName = "untitled.idx";
	}
	ImGuiFileDialog::Instance()->OpenDialog(
		"SaveImage", "Save Image", ".idx", config);
}

void PaintFrontend::save_to_path(const std::string& path, bool continue_pending)
{
	std::string error;
	if (!m_backend.save_document_as(path, error))
	{
		m_save_then_pending = false;
		m_pending_file_action = FileAction::None;
		show_file_error(error);
		return;
	}

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
			const std::string path = ensure_idx_extension(
				ImGuiFileDialog::Instance()->GetFilePathName());
			const bool continue_pending = m_save_then_pending;
			m_save_then_pending = false;
			save_to_path(path, continue_pending);
		}
		else
		{
			m_save_then_pending = false;
			if (m_pending_file_action != FileAction::None)
			{
				m_pending_file_action = FileAction::None;
			}
		}
		ImGuiFileDialog::Instance()->Close();
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
		ImGui::Text("Save changes to \"%s\" before continuing?", name.c_str());
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
		if (active)
		{
			const ImU32 color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
			ImGui::PushStyleColor(ImGuiCol_Button, color);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		const ImVec2 buttonSize(kToolButtonSize, kToolButtonSize);
		const bool clicked = btn.icon
			? ImGui::ImageButton(btn.name.c_str(), btn.icon, buttonSize)
			: ImGui::Button(btn.name.c_str(), buttonSize);
		ImGui::PopStyleVar();
		if (active)
		{
			ImGui::PopStyleColor(3);
		}

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

	auto draw_beveled_rect = [&](const ImVec2& pos, const ImVec2& end, ImU32 fill)
	{
		drawList->AddRectFilled(pos, end, fill);
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
		ImGui::GetColorU32(ImGuiCol_FrameBg));

	const float swatchOffset = swatchSize - kSwatchGap - kSwatchSize;
	const ImVec2 fgPos(swatchPos.x + kSwatchGap, swatchPos.y + kSwatchGap);
	const ImVec2 bgPos(swatchPos.x + swatchOffset, swatchPos.y + swatchOffset);
	const auto draw_swatch = [&](const ImVec2& pos, const PaintCore::Color& color)
	{
		draw_beveled_rect(pos, ImVec2(pos.x + kSwatchSize, pos.y + kSwatchSize),
			toImU32(color));
	};

	draw_swatch(bgPos, m_backend.palette().getColor(
		m_backend.background_index()));
	draw_swatch(fgPos, m_backend.palette().getColor(
		m_backend.foreground_index()));

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

		ImGui::SetCursorScreenPos(cellPos);
		ImGui::InvisibleButton(
			("##palette" + std::to_string(i)).c_str(),
			ImVec2(cellWidth, cellHeight));

		draw_beveled_rect(cellPos,
			ImVec2(cellPos.x + cellWidth, cellPos.y + cellHeight),
			toImU32(m_backend.palette().getColor(colorIndex)));

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			m_backend.set_foreground_index(colorIndex);
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_backend.set_background_index(colorIndex);
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
	ImGui::SliderInt("Zoom", &m_zoom, 1, 64);
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
	m_canvas_area.render(drawList, m_backend.palette(),
	                     m_backend.foreground_index(),
	                     m_backend.background_index());

	// The grid can be drawn on top here using drawList.
	// drawGrid(drawList, canvasPos, canvas.getWidth(), canvas.getHeight(), zoom);

}
