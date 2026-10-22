#include <string>
#include <memory>

#include "paint/paint_frontend.h"
#include "tools/square_tool.h"

using namespace std;

PaintFrontend::PaintFrontend() :
m_canvas_area(CanvasArea()),
m_palette(Palette(2))
{
	m_palette.setColor(0, 0xFFe0f8d0);
	m_palette.setColor(1, 0xFF88c070);
	m_palette.setColor(2, 0xFF346856);
	m_palette.setColor(3, 0xFF081820);
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
			ImGui::MenuItem("Undo", "Ctrl+Z");
			ImGui::MenuItem("Redo", "Ctrl+Y");
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void PaintFrontend::renderToolbar()
{
	ImGui::Text("Tools");
	ImGui::Separator();
	if (ImGui::Button("Pencil", ImVec2(-FLT_MIN, 30))) {}
	if (ImGui::Button("Fill", ImVec2(-FLT_MIN, 30))) {}
	if (ImGui::Button("Rect", ImVec2(-FLT_MIN, 30)))
	{
		auto rectTool = std::make_shared<SquareTool>(1,2);
		m_canvas_area.canvas().set_tool(rectTool);
	}
	if (ImGui::Button("Circle", ImVec2(-FLT_MIN, 30))) {}
}

void PaintFrontend::renderPalette()
{
	ImGui::Text("Palette");
	ImGui::Separator();

	ImGui::BeginGroup();
	const ImVec4 colors[4] =
	{
		ImVec4(0.89f, 0.91f, 0.67f, 1.0f), // Lightest
		ImVec4(0.67f, 0.78f, 0.45f, 1.0f),
		ImVec4(0.40f, 0.57f, 0.25f, 1.0f),
		ImVec4(0.18f, 0.31f, 0.16f, 1.0f)  // Darkest
	};

	for (int i = 0; i < 4; ++i)
	{
		ImGui::SameLine();
		ImGui::ColorButton(("##col" + std::to_string(i)).c_str(), colors[i],
						   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
						   ImVec2(50, 50));
	}
	ImGui::EndGroup();

	// Zoom slider
	ImGui::SliderInt("Zoom", &m_zoom, 1, 8);	
}

void PaintFrontend::renderCanvas()
{
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 content_size(m_canvas_area.canvas().getWidth() * m_zoom, m_canvas_area.canvas().getHeight() * m_zoom);

	// Manejo de input
	ImGui::InvisibleButton("canvas_area", content_size);
	ImVec2 scroll(ImGui::GetScrollX(), ImGui::GetScrollY());
	bool isActive = ImGui::IsItemActive();

	ImVec2 mousePos = ImGui::GetMousePos();
	m_canvas_area.handle_input(canvasPos, canvasSize, m_zoom);

	// Render del buffer
	m_canvas_area.render(drawList, canvasPos, canvasSize, scroll, m_zoom, m_palette);

	// Aquí se puede dibujar la rejilla encima usando drawList
	// drawGrid(drawList, canvasPos, canvas.getWidth(), canvas.getHeight(), zoom);

}
