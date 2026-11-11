#ifndef QTREGBPAINT_PAINT_UI_ROLES_H
#define QTREGBPAINT_PAINT_UI_ROLES_H

#include <imgui.h>

// Semantic colors shared by the custom-drawn parts of the UI. They are read
// from the current ImGui style on every call, so switching themes needs no
// cached values or extra refresh step.
namespace PaintUi
{
namespace Roles
{
ImU32 accent();          // active tool, primary action
ImU32 selection();       // marquee, canvas bounds, draggable handles
ImU32 guide();           // tool guides (Curve control polygon)
ImU32 border();
ImU32 surface();
ImU32 text();
ImU32 text_muted();
ImU32 hover_overlay();   // neutral hover, derived from the text color
ImU32 pressed_overlay(); // neutral press
ImU32 selected_fill();   // selected option row/cell
ImU32 bevel_light();
ImU32 bevel_dark();
} // namespace Roles
} // namespace PaintUi

#endif
