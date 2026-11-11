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
#include "paint/i18n.h"
#include "paint/ui_roles.h"
#include "paint/ui_tokens.h"
#include "paint/ui_widgets.h"
#include "paint/window_title.h"
#include "tools/tool_option.h"

using namespace std;

namespace
{
constexpr int kToolbarColumns = 2;
constexpr float kCanvasResizeMargin = 12.0f;
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
		(cell_width - PaintUi::Space::kXs) / reference_extent,
		(cell_height - PaintUi::Space::kXs) / reference_extent);
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
		PaintUi::Size::kToolButton * kToolbarColumns +
		columnGap * (kToolbarColumns - 1);
	const float toolsHeight =
		rows > 0 ? rows * PaintUi::Size::kToolButton + (rows - 1) * rowGap : 0.0f;

	const float contentHeight =
		style.WindowPadding.y * 2.0f +
		(rows > 0 ? toolsHeight + rowGap : 0.0f) +
		PaintUi::Size::kToolOptionsHeight;

	return ToolbarLayout{
		toolsWidth + style.WindowPadding.x * 2.0f,
		contentHeight,
	};
}

float compute_palette_grid_width(const ImGuiStyle& style)
{
	return kPaletteReferenceCells * PaintUi::Size::kPaletteColor +
		(kPaletteReferenceCells - 1) * style.ItemSpacing.x;
}

float compute_palette_panel_width(const ImGuiStyle& style)
{
	return PaintUi::Size::kPaletteColor + style.ItemSpacing.x +
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
		PaintUi::Size::kPaletteColor + style.ItemSpacing.y +
		ImGui::GetFrameHeight();
}

float compute_status_bar_height(const ImGuiStyle& style)
{
	return style.WindowPadding.y * 2.0f + ImGui::GetTextLineHeight();
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

	PaintUi::draw_checkerboard(draw_list, min, max, PaintUi::Space::kXs, origin);
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

void PaintFrontend::set_language(const std::string& language)
{
	m_language = language.empty() ? "system" : language;
	i18n::set_locale(m_language);
}

std::string PaintFrontend::document_display_name() const
{
	if (m_backend.has_file_path())
	{
		return m_backend.file_display_name();
	}
	return i18n::tr("Untitled");
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
			document_display_name(), m_backend.is_modified()) +
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
	renderDialogs();

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
		if (PaintUi::tool_button(btn.name.c_str(), btn.icon, active,
		                         i18n::tr(btn.name.c_str())))
		{
			select_tool(btn.name);
		}

		++index;
	}

	ImGui::Spacing();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 8.0f));
	ImGui::BeginChild("ToolOptions", ImVec2(0.0f, PaintUi::Size::kToolOptionsHeight), true,
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
					const float cell_height = PaintUi::Size::kToolOptionRow;

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

						ImGui::PushID(choice.value);
						const PaintUi::OptionCellState state =
							PaintUi::option_cell(
								"##option_cell", cell_min,
								ImVec2(cell_width, cell_height),
								option.value == choice.value);

						if (state.clicked)
						{
							tool->set_option(option.id, choice.value);
						}

						ImDrawList* draw_list = ImGui::GetWindowDrawList();
						if (!choice.preview.empty())
						{
							draw_choice_preview(
								draw_list, choice.preview, cell_min, cell_max,
								reference_extent, PaintUi::Roles::text());
						}
						else
						{
							const ImVec2 text_size =
								ImGui::CalcTextSize(i18n::tr(choice.short_label.c_str()));
							const ImVec2 text_pos(
								cell_min.x + (cell_width - text_size.x) * 0.5f,
								cell_min.y + (cell_height - text_size.y) * 0.5f);
							draw_list->AddText(
								text_pos, PaintUi::Roles::text(),
								i18n::tr(choice.short_label.c_str()));
						}

						if (state.hovered)
						{
							ImGui::SetTooltip("%s", i18n::tr(choice.label.c_str()));
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
				float row_height = PaintUi::Size::kToolOptionRow;
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
							: std::min(PaintUi::Size::kToolOptionRow, distributed);
					}
				}

				for (const ToolOptionChoice& choice : option.choices)
				{
					ImGui::PushID(choice.value);

					const ImVec2 row_pos = ImGui::GetCursorScreenPos();
					const ImVec2 row_size(ImGui::GetContentRegionAvail().x,
					                      row_height);
					const PaintUi::OptionCellState state =
						PaintUi::option_cell(
							"##option_row", row_pos, row_size,
							option.value == choice.value);

					if (state.clicked)
					{
						tool->set_option(option.id, choice.value);
					}

					ImDrawList* draw_list = ImGui::GetWindowDrawList();

					if (!choice.preview.empty())
					{
						draw_choice_preview(
							draw_list, choice.preview, row_pos,
							ImVec2(row_pos.x + row_size.x,
							       row_pos.y + row_size.y),
							list_reference_extent,
							PaintUi::Roles::text());

						const ImVec2 text_size =
							ImGui::CalcTextSize(i18n::tr(choice.short_label.c_str()));
						const ImVec2 text_pos(
							row_pos.x + row_size.x - text_size.x - PaintUi::Space::kSm,
							row_pos.y + (row_size.y - text_size.y) * 0.5f);
						draw_list->AddText(
							text_pos, PaintUi::Roles::text(),
							i18n::tr(choice.short_label.c_str()));
					}
					else
					{
						const ImTextureID icon = m_backend.optionIcon(choice.icon);
						const float icon_width = icon_list
							? std::max(1.0f, row_size.x - PaintUi::Space::kXs)
							: std::min(PaintUi::Size::kToolOptionIconWidth,
							           std::max(1.0f, row_size.x - PaintUi::Space::kXs));
						const float icon_height = icon_list
							? std::max(1.0f, row_size.y - PaintUi::Space::kXs)
							: PaintUi::Size::kToolOptionIconHeight;
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
								ImGui::CalcTextSize(i18n::tr(choice.short_label.c_str()));
							const ImVec2 text_pos(
								row_pos.x + (row_size.x - text_size.x) * 0.5f,
								row_pos.y + (row_size.y - text_size.y) * 0.5f);
							draw_list->AddText(
								text_pos,
								PaintUi::Roles::text(),
								i18n::tr(choice.short_label.c_str()));
						}
					}

					if (state.hovered)
					{
						ImGui::SetTooltip("%s", i18n::tr(choice.label.c_str()));
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
					ImGui::SetTooltip("%s", i18n::tr(option.label.c_str()));
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
	const float gridHeight = PaintUi::Size::kPaletteColor;
	const float cellHeight = std::max(
		1.0f, (gridHeight - (rows - 1) * rowGap) / rows);

	const ImVec2 swatchPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float swatchSize = PaintUi::Size::kPaletteColor;
	const ImVec2 swatchEnd(
		swatchPos.x + swatchSize,
		swatchPos.y + swatchSize);

	ImGui::InvisibleButton("##fg_bg_swatch", ImVec2(swatchSize, swatchSize));

	const auto draw_swatch = [&](const ImVec2& pos, const ImVec2& end,
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
		PaintUi::swatch_frame(drawList, pos, end);
	};

	draw_swatch(swatchPos, swatchEnd, PaintUi::Roles::surface(), false);

	const float swatchOffset =
		swatchSize - PaintUi::Size::kSwatchGap - PaintUi::Size::kSwatch;
	const ImVec2 fgPos(
		swatchPos.x + PaintUi::Size::kSwatchGap,
		swatchPos.y + PaintUi::Size::kSwatchGap);
	const ImVec2 bgPos(
		swatchPos.x + swatchOffset,
		swatchPos.y + swatchOffset);
	const PaintCore::ColorIndex foreground = m_backend.foreground_index();
	const PaintCore::ColorIndex background = m_backend.background_index();
	draw_swatch(
		bgPos,
		ImVec2(bgPos.x + PaintUi::Size::kSwatch,
		       bgPos.y + PaintUi::Size::kSwatch),
		toImU32(m_backend.palette().getColor(background)),
		m_backend.palette().is_transparent(background));
	draw_swatch(
		fgPos,
		ImVec2(fgPos.x + PaintUi::Size::kSwatch,
		       fgPos.y + PaintUi::Size::kSwatch),
		toImU32(m_backend.palette().getColor(foreground)),
		m_backend.palette().is_transparent(foreground));

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

		draw_swatch(cellPos,
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
			ImGui::SetTooltip("%s %d", i18n::tr("Transparent color"),
			                  static_cast<int>(colorIndex));
		}
	}

	const float bottom = std::max(swatchPos.y + swatchSize, gridPos.y + gridHeight) +
		style.ItemSpacing.y;

	const float paletteContentWidth =
		compute_palette_panel_width(style) - style.WindowPadding.x * 2.0f;
	const float zoomLabelWidth = ImGui::CalcTextSize(i18n::tr("Zoom")).x;
	const float zoomSliderWidth = std::max(
		1.0f, paletteContentWidth - style.ItemInnerSpacing.x - zoomLabelWidth);

	ImGui::SetCursorScreenPos(ImVec2(swatchPos.x, bottom));
	ImGui::SetNextItemWidth(zoomSliderWidth);
	int zoom = m_zoom;
	if (ImGui::SliderInt(i18n::tr("Zoom"), &zoom, 1, 64))
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

	const float gap = PaintUi::Space::kLg;
	const auto separator = [&]()
	{
		ImGui::SameLine(0.0f, gap);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0.0f, gap);
	};

	ImGui::Text("%s %dx%d", i18n::tr("Canvas:"),
	            m_backend.session().getWidth(),
	            m_backend.session().getHeight());
	separator();
	ImGui::Text("%s %dx", i18n::tr("Zoom:"), m_zoom);
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
		ImGui::SameLine(0.0f, PaintUi::Space::kXs);
		ImGui::Text("%s %d", i18n::tr("Color:"), static_cast<int>(color));

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
		                   PaintUi::Roles::border());
	}
	else
	{
		ImGui::TextDisabled("%s --", i18n::tr("Color:"));
	}
	separator();
	ImGui::TextUnformatted(m_backend.is_modified()
		? i18n::tr("Modified")
		: i18n::tr("Saved"));

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
