#ifndef QTREGBPAINT_PAINT_UI_WIDGETS_H
#define QTREGBPAINT_PAINT_UI_WIDGETS_H

#include <imgui.h>

namespace PaintUi
{
struct OptionCellState
{
	bool clicked = false;
	bool hovered = false;
};

// Common shell for tool-option rows and grid cells: places an invisible
// button at `min`, draws the selected/hover background and returns the
// interaction state. The caller draws the icon, preview or label on top.
OptionCellState option_cell(const char* id, const ImVec2& min,
                            const ImVec2& size, bool selected);

// Toolbar button with the editor's neutral idle state and the theme accent
// for the active tool. `id` must stay stable (use the internal tool name);
// `tooltip` is the user-visible label.
bool tool_button(const char* id, ImTextureID icon, bool active,
                 const char* tooltip);

// Paint-style bevel and border for a palette swatch. The caller fills the
// interior or draws the transparency checkerboard first.
void swatch_frame(ImDrawList* draw_list, const ImVec2& min,
                  const ImVec2& max);

// Modal helpers. `title` is the English source text and is translated by the
// helper; the `###stable_id` suffix keeps the ImGui ID stable. Use the same
// pair for open/begin/is_open.
void open_modal(const char* title, const char* stable_id);
bool begin_modal(const char* title, const char* stable_id,
                 ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize);
void end_modal();
bool is_modal_open(const char* title, const char* stable_id);

// Standard footer button. Returns true when pressed.
bool dialog_button(const char* label);
} // namespace PaintUi

#endif
