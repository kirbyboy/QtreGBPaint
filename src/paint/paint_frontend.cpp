#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <memory>

#include "paint/paint_frontend.h"
#include "paint/color_imgui.h"
#include "tools/square_tool.h"
#include "tools/tool_option.h"

using namespace std;

namespace
{
constexpr float kToolButtonSize = 32.0f;
constexpr int kToolbarColumns = 2;
constexpr float kToolOptionsHeight = 88.0f;
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
constexpr int kPaletteMaxColors = 32;
constexpr float kMinCanvasWidth = 256.0f;

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
}

PaintFrontend::PaintFrontend(PaintBackend& backend) :
m_backend(backend),
m_canvas_area(backend.backend()),
m_palette(5)
{
	regenerate_palette();
}

void PaintFrontend::select_tool(const std::string& name)
{
	m_canvas_area.session().set_tool(m_backend.selectButton(name));
	apply_tool_colors();
}

void PaintFrontend::render()
{
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
			m_canvas_area.session().getWidth(),
			m_canvas_area.resize_preview_width());
		const int content_height = std::max(
			m_canvas_area.session().getHeight(),
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
}

void PaintFrontend::renderMenuBar()
{
	PaintSession& session = m_canvas_area.session();

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New", nullptr, false, false);
			ImGui::MenuItem("Open", nullptr, false, false);
			ImGui::MenuItem("Save", nullptr, false, false);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, session.can_undo()))
			{
				session.undo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, session.can_redo()))
			{
				session.redo();
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
			ImGui::Separator();
			if (ImGui::MenuItem("Invert Colors", nullptr, false, has_floating))
			{
				session.invert_floating_region_colors();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Palette (temp)"))
		{
			if (ImGui::MenuItem("Game Boy", nullptr, m_use_dmg_palette))
			{
				m_use_dmg_palette = true;
				regenerate_palette();
			}

			if (ImGui::SliderInt("Colors", &m_palette_color_count, 1, kPaletteMaxColors))
			{
				m_use_dmg_palette = false;
				regenerate_palette();
			}

			if (ImGui::BeginMenu("Gradient"))
			{
				if (ImGui::MenuItem("Red", nullptr,
				                    !m_use_dmg_palette && m_gradient == Gradient::Red))
				{
					m_use_dmg_palette = false;
					m_gradient = Gradient::Red;
					regenerate_palette();
				}
				if (ImGui::MenuItem("Green", nullptr,
				                    !m_use_dmg_palette && m_gradient == Gradient::Green))
				{
					m_use_dmg_palette = false;
					m_gradient = Gradient::Green;
					regenerate_palette();
				}
				if (ImGui::MenuItem("Blue", nullptr,
				                    !m_use_dmg_palette && m_gradient == Gradient::Blue))
				{
					m_use_dmg_palette = false;
					m_gradient = Gradient::Blue;
					regenerate_palette();
				}
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
			session.resize(m_canvas_size_width, m_canvas_size_height,
			              static_cast<PaintCore::ColorIndex>(m_background_index));
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
	// consume the key when there is no history available.
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) && session.can_undo())
	{
		session.undo();
	}
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) && session.can_redo())
	{
		session.redo();
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

		ImVec2 pos = ImGui::GetCursorScreenPos();
		bool active = (m_backend.activeButton() == btn.name);

		ImTextureID icon = btn.iconNormal;
		if (active && btn.iconPressed) icon = btn.iconPressed;

		const ImVec2 buttonSize(kToolButtonSize, kToolButtonSize);

		if (icon)
		{
			ImGui::Image(icon, buttonSize);
		}
		else
		{
			ImGui::Button(btn.name.c_str(), buttonSize);
		}

		bool hovered = ImGui::IsItemHovered();

		if (!active && hovered && btn.iconHover)
		{
			ImGui::SetCursorScreenPos(pos);
			ImGui::Image(btn.iconHover, buttonSize);
		}

		ImGui::SetCursorScreenPos(pos);
		bool clicked = ImGui::InvisibleButton(btn.name.c_str(), buttonSize);

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
	ToolBase* tool = m_canvas_area.session().active_tool();
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
				for (const ToolOptionChoice& choice : option.choices)
				{
					ImGui::PushID(choice.value);

					const ImVec2 row_pos = ImGui::GetCursorScreenPos();
					const ImVec2 row_size(ImGui::GetContentRegionAvail().x,
					                      kToolOptionRowHeight);
					const bool clicked = ImGui::InvisibleButton("##option_row", row_size);
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

					const ImTextureID icon = m_backend.optionIcon(choice.icon);
					const float icon_width = std::min(
						kToolOptionIconWidth,
						std::max(1.0f, row_size.x - 4.0f));
					const ImVec2 icon_pos(
						row_pos.x + (row_size.x - icon_width) * 0.5f,
						row_pos.y + (row_size.y - kToolOptionIconHeight) * 0.5f);
					if (icon)
					{
						draw_list->AddImage(
							icon, icon_pos,
							ImVec2(icon_pos.x + icon_width,
							       icon_pos.y + kToolOptionIconHeight));
					}
					else
					{
						const ImVec2 text_size =
							ImGui::CalcTextSize(choice.short_label.c_str());
						const ImVec2 text_pos(
							row_pos.x + (row_size.x - text_size.x) * 0.5f,
							row_pos.y + (row_size.y - text_size.y) * 0.5f);
						draw_list->AddText(text_pos,
						                   ImGui::GetColorU32(ImGuiCol_Text),
						                   choice.short_label.c_str());
					}

					if (hovered)
					{
						ImGui::SetTooltip("%s", choice.label.c_str());
					}

					ImGui::PopID();
				}
				break;
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
	tool->sync_floating_region(m_canvas_area.session());
}

void PaintFrontend::regenerate_palette()
{
	// The Game Boy palette is fixed at four colors; generated palettes can
	// use up to kPaletteMaxColors.
	if (m_use_dmg_palette)
	{
		m_palette_color_count = 4;
	}
	m_palette_color_count = std::clamp(m_palette_color_count, 1, kPaletteMaxColors);

	PaintCore::Color fill{};

	if (m_use_dmg_palette)
	{
		const Palette dmg = Palette::dmg();
		for (std::size_t i = 0; i < dmg.size(); ++i)
		{
			m_palette.setColor(static_cast<PaintCore::ColorIndex>(i),
			                   dmg.getColor(static_cast<PaintCore::ColorIndex>(i)));
		}
		// Entries above the visible count are never selectable, but keeping
		// them valid avoids the out-of-range sentinel if the palette shrinks.
		fill = dmg.getColor(0);
	}
	else
	{
		PaintCore::Color base;
		switch (m_gradient)
		{
			case Gradient::Red:   base = {255, 0, 0, 255}; break;
			case Gradient::Green: base = {0, 255, 0, 255}; break;
			case Gradient::Blue:  base = {0, 0, 255, 255}; break;
		}

		const PaintCore::Color black{0, 0, 0, 255};
		const PaintCore::Color white{255, 255, 255, 255};

		auto mix = [](const PaintCore::Color& a, const PaintCore::Color& b, float t)
		{
			auto channel = [t](uint8_t av, uint8_t bv)
			{
				return static_cast<uint8_t>(std::lround(av + (bv - av) * t));
			};
			return PaintCore::Color{
				channel(a.r, b.r),
				channel(a.g, b.g),
				channel(a.b, b.b),
				255,
			};
		};

		for (int i = 0; i < m_palette_color_count; ++i)
		{
			const float t = m_palette_color_count == 1
				? 0.5f
				: static_cast<float>(i) / static_cast<float>(m_palette_color_count - 1);

			const PaintCore::Color color = t <= 0.5f
				? mix(black, base, t * 2.0f)
				: mix(base, white, (t - 0.5f) * 2.0f);
			m_palette.setColor(static_cast<PaintCore::ColorIndex>(i), color);
			fill = color;
		}
	}

	for (int i = m_palette_color_count;
	     i < static_cast<int>(m_palette.size()); ++i)
	{
		m_palette.setColor(static_cast<PaintCore::ColorIndex>(i), fill);
	}

	if (m_use_dmg_palette)
	{
		// Paint-like defaults: dark foreground over light background.
		m_foreground_index = 3;
		m_background_index = 0;
	}
	else
	{
		if (m_foreground_index >= static_cast<std::size_t>(m_palette_color_count))
		{
			m_foreground_index = static_cast<std::size_t>(m_palette_color_count - 1);
		}
		if (m_background_index >= static_cast<std::size_t>(m_palette_color_count))
		{
			m_background_index = m_palette_color_count > 1 ? 1 : 0;
		}
	}

	apply_tool_colors();
}

void PaintFrontend::apply_tool_colors()
{
	if (ToolBase* tool = m_canvas_area.session().active_tool())
	{
		tool->set_colors(static_cast<PaintCore::ColorIndex>(m_foreground_index),
		                 static_cast<PaintCore::ColorIndex>(m_background_index));
		tool->sync_floating_region(m_canvas_area.session());
	}
}

void PaintFrontend::renderPalette()
{
	const int colorCount = m_palette_color_count;
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

	draw_swatch(bgPos, m_palette.getColor(
		static_cast<PaintCore::ColorIndex>(m_background_index)));
	draw_swatch(fgPos, m_palette.getColor(
		static_cast<PaintCore::ColorIndex>(m_foreground_index)));

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

		ImGui::SetCursorScreenPos(cellPos);
		ImGui::InvisibleButton(
			("##palette" + std::to_string(i)).c_str(),
			ImVec2(cellWidth, cellHeight));

		draw_beveled_rect(cellPos,
			ImVec2(cellPos.x + cellWidth, cellPos.y + cellHeight),
			toImU32(m_palette.getColor(static_cast<PaintCore::ColorIndex>(i))));

		bool selection_changed = false;

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			m_foreground_index = static_cast<std::size_t>(i);
			selection_changed = true;
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_background_index = static_cast<std::size_t>(i);
			selection_changed = true;
		}

		if (selection_changed)
		{
			apply_tool_colors();
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
		m_canvas_area.session().getWidth(),
		m_canvas_area.resize_preview_width());
	const int content_height = std::max(
		m_canvas_area.session().getHeight(),
		m_canvas_area.resize_preview_height());
	ImVec2 content_size(
		content_width * m_zoom + kCanvasResizeMargin,
		content_height * m_zoom + kCanvasResizeMargin);

	// A single transformation per frame, shared by input and render.
	ImGui::InvisibleButton("canvas_area", content_size);

	// The cursor belongs to the selected toolbar button and is only shown
	// while the pointer is over the canvas. Resize handles can override it.
	const bool canvas_hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
	if (IGuiBackend* backend = m_backend.backend())
	{
		backend->set_cursor(canvas_hovered ? m_backend.activeCursor()
		                                   : PaintCore::CursorType::Arrow);
	}

	ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
	m_canvas_area.update_view(canvasPos, canvasSize, scroll, static_cast<float>(m_zoom));
	m_canvas_area.set_resize_fill(
		static_cast<PaintCore::ColorIndex>(m_background_index));
	m_canvas_area.set_grid_enabled(m_show_grid);

	m_canvas_area.handle_input(ImGui::GetMousePos());
	m_canvas_area.render(drawList, m_palette);

	// The grid can be drawn on top here using drawList.
	// drawGrid(drawList, canvasPos, canvas.getWidth(), canvas.getHeight(), zoom);

}
