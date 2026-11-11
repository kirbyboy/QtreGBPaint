#include "paint/ui_roles.h"

#include "paint/ui_tokens.h"

namespace PaintUi
{
namespace Roles
{
ImU32 accent()
{
	return ImGui::GetColorU32(ImGuiCol_ButtonActive);
}

ImU32 selection()
{
	return ImGui::GetColorU32(ImGuiCol_HeaderActive);
}

ImU32 guide()
{
	return ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.85f);
}

ImU32 border()
{
	return ImGui::GetColorU32(ImGuiCol_Border);
}

ImU32 surface()
{
	return ImGui::GetColorU32(ImGuiCol_FrameBg);
}

ImU32 text()
{
	return ImGui::GetColorU32(ImGuiCol_Text);
}

ImU32 text_muted()
{
	return ImGui::GetColorU32(ImGuiCol_TextDisabled);
}

ImU32 hover_overlay()
{
	return ImGui::GetColorU32(ImGuiCol_Text, Alpha::kHover);
}

ImU32 pressed_overlay()
{
	return ImGui::GetColorU32(ImGuiCol_Text, Alpha::kPressed);
}

ImU32 selected_fill()
{
	return ImGui::GetColorU32(ImGuiCol_HeaderActive, Alpha::kSelected);
}

ImU32 bevel_light()
{
	return IM_COL32(255, 255, 255, 255);
}

ImU32 bevel_dark()
{
	return IM_COL32(0, 0, 0, 255);
}
} // namespace Roles
} // namespace PaintUi
