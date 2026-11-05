#include <string>
#include <memory>

#include "paint/paint_frontend.h"
#include "paint/color_imgui.h"
#include "tools/square_tool.h"

using namespace std;

PaintFrontend::PaintFrontend(PaintBackend& backend) :
m_backend(backend),
m_canvas_area(backend.backend()),
m_palette(Palette::dmg())
{}

void PaintFrontend::select_tool(const std::string& name)
{
	m_canvas_area.canvas().set_tool(m_backend.selectButton(name));
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
				 ImGuiWindowFlags_MenuBar);

	renderMenuBar();

	// Compute available space for layout
	ImVec2 contentRegion = ImGui::GetContentRegionAvail();
	float toolbarWidth = 120.0f;
	float paletteHeight = 100.0f;

	// --- Left toolbar ---
	ImGui::BeginChild("Toolbar", ImVec2(toolbarWidth, contentRegion.y - paletteHeight), true);
	renderToolbar();
	ImGui::EndChild();

	ImGui::SameLine();

	// --- Canvas and palette column ---
	//ImGui::BeginGroup();

	// --- Canvas area ---
	ImVec2 canvasRegion = ImVec2(contentRegion.x - toolbarWidth, contentRegion.y - paletteHeight);
	ImGui::BeginChild("Canvas", canvasRegion, true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
	renderCanvas();
	ImGui::EndChild();

	// --- Palette area ---
	ImGui::BeginChild("Palette", ImVec2(canvasRegion.x, paletteHeight), true);
	renderPalette();
	ImGui::EndChild();

	//ImGui::EndGroup();

	ImGui::End();
}

void PaintFrontend::renderMenuBar()
{
	Canvas& canvas = m_canvas_area.canvas();

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New", nullptr, false, true);
			ImGui::MenuItem("Open", nullptr, false, true);
			ImGui::MenuItem("Save", nullptr, false, true);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canvas.can_undo()))
			{
				canvas.undo();
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canvas.can_redo()))
			{
				canvas.redo();
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	// Atajos globales. Shortcut respeta el foco y los widgets de texto,
	// y no consume la tecla si no hay historial disponible.
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) && canvas.can_undo())
	{
		canvas.undo();
	}
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) && canvas.can_redo())
	{
		canvas.redo();
	}
}

void PaintFrontend::renderToolbar()
{
	ImGui::Text("Tools");
	ImGui::Separator();

	float avail = ImGui::GetContentRegionAvail().x;
	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float x = 0.0f;

	for (const auto& btn : m_backend.buttons())
	{
		if (x > 0.0f && x + 32.0f > avail)
		{
			x = 0.0f;
		}
		else if (x > 0.0f)
		{
			ImGui::SameLine();
		}

		ImVec2 pos = ImGui::GetCursorScreenPos();
		bool active = (m_backend.activeButton() == btn.name);

		ImTextureID icon = btn.iconNormal;
		if (active && btn.iconPressed) icon = btn.iconPressed;

		if (icon)
		{
			ImGui::Image(icon, ImVec2(32, 32));
		}
		else
		{
			ImGui::Button(btn.name.c_str(), ImVec2(32, 32));
		}

		bool hovered = ImGui::IsItemHovered();

		if (!active && hovered && btn.iconHover)
		{
			ImGui::SetCursorScreenPos(pos);
			ImGui::Image(btn.iconHover, ImVec2(32, 32));
		}

		ImGui::SetCursorScreenPos(pos);
		bool clicked = ImGui::InvisibleButton(btn.name.c_str(), ImVec2(32, 32));

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", btn.name.c_str());
		}

		if (clicked)
		{
			select_tool(btn.name);
		}

		x += 32.0f + spacing;
	}

}

void PaintFrontend::renderPalette()
{
	ImGui::Text("Palette");
	ImGui::Separator();

	ImGui::BeginGroup();
	for (size_t i = 0; i < m_palette.size(); ++i)
	{
		ImGui::SameLine();
		const PaintCore::Color color = m_palette.getColor(static_cast<PaintCore::ColorIndex>(i));
		ImGui::ColorButton(("##col" + std::to_string(i)).c_str(), toImVec4(color),
						   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
						   ImVec2(50, 50));
	}
	ImGui::EndGroup();

	// Zoom slider
	ImGui::SliderInt("Zoom", &m_zoom, 1, 64);
}

void PaintFrontend::renderCanvas()
{
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 content_size(m_canvas_area.canvas().getWidth() * m_zoom, m_canvas_area.canvas().getHeight() * m_zoom);

	// Una sola transformación por frame, compartida por input y render.
	ImGui::InvisibleButton("canvas_area", content_size);
	ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
	m_canvas_area.update_view(canvasPos, canvasSize, scroll, static_cast<float>(m_zoom));

	m_canvas_area.handle_input(ImGui::GetMousePos());
	m_canvas_area.render(drawList, m_palette);

	// Aquí se puede dibujar la rejilla encima usando drawList
	// drawGrid(drawList, canvasPos, canvas.getWidth(), canvas.getHeight(), zoom);

}
