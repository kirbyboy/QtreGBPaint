#include "paint/ui_widgets.h"

#include <string>

#include "paint/i18n.h"
#include "paint/ui_roles.h"
#include "paint/ui_tokens.h"

namespace PaintUi
{
namespace
{
std::string modal_id(const char* title, const char* stable_id)
{
	return std::string(i18n::tr(title)) + "###" + stable_id;
}
} // namespace

OptionCellState option_cell(const char* id, const ImVec2& min,
                            const ImVec2& size, bool selected)
{
	ImGui::SetCursorScreenPos(min);
	ImGui::InvisibleButton(id, size);

	OptionCellState state;
	state.clicked = ImGui::IsItemClicked();
	state.hovered = ImGui::IsItemHovered();

	const ImVec2 max(min.x + size.x, min.y + size.y);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	if (selected)
	{
		draw_list->AddRectFilled(min, max, Roles::selected_fill());
	}
	else if (state.hovered)
	{
		draw_list->AddRectFilled(min, max, Roles::hover_overlay());
	}

	return state;
}

bool tool_button(const char* id, ImTextureID icon, bool active,
                 const char* tooltip)
{
	const ImU32 normal = active ? Roles::accent() : IM_COL32(0, 0, 0, 0);
	const ImU32 hovered = active ? Roles::accent() : Roles::hover_overlay();
	const ImU32 pressed = active ? Roles::accent() : Roles::pressed_overlay();

	ImGui::PushStyleColor(ImGuiCol_Button, normal);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressed);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

	const ImVec2 size(Size::kToolButton, Size::kToolButton);
	const bool clicked = icon
		? ImGui::ImageButton(id, icon, size)
		: ImGui::Button(id, size);

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	if (tooltip && ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", tooltip);
	}

	return clicked;
}

void swatch_frame(ImDrawList* draw_list, const ImVec2& min,
                  const ImVec2& max)
{
	if (!draw_list)
	{
		return;
	}

	const float bevel = Size::kSwatchBevel;
	draw_list->AddLine(
		ImVec2(min.x + 1.0f, min.y + 1.0f),
		ImVec2(max.x - 1.0f, min.y + 1.0f), Roles::bevel_light(), bevel);
	draw_list->AddLine(
		ImVec2(min.x + 1.0f, min.y + 1.0f),
		ImVec2(min.x + 1.0f, max.y - 1.0f), Roles::bevel_light(), bevel);
	draw_list->AddLine(
		ImVec2(min.x + 1.0f, max.y - 1.0f),
		ImVec2(max.x - 1.0f, max.y - 1.0f), Roles::bevel_dark(), bevel);
	draw_list->AddLine(
		ImVec2(max.x - 1.0f, min.y + 1.0f),
		ImVec2(max.x - 1.0f, max.y - 1.0f), Roles::bevel_dark(), bevel);
	draw_list->AddRect(min, max, Roles::border());
}

void open_modal(const char* title, const char* stable_id)
{
	const std::string id = modal_id(title, stable_id);
	ImGui::OpenPopup(id.c_str());
}

bool begin_modal(const char* title, const char* stable_id,
                 ImGuiWindowFlags flags)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
	                    ImVec2(Space::kLg, Space::kLg));
	const std::string id = modal_id(title, stable_id);
	if (!ImGui::BeginPopupModal(id.c_str(), nullptr, flags))
	{
		ImGui::PopStyleVar();
		return false;
	}
	return true;
}

void end_modal()
{
	ImGui::EndPopup();
	ImGui::PopStyleVar();
}

bool is_modal_open(const char* title, const char* stable_id)
{
	const std::string id = modal_id(title, stable_id);
	return ImGui::IsPopupOpen(id.c_str());
}

bool dialog_button(const char* label)
{
	return ImGui::Button(label, ImVec2(Size::kDialogButton, 0.0f));
}
} // namespace PaintUi
