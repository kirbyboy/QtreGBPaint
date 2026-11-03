#ifndef QTREGBPAINT_CANVAS_AREA_H
#define QTREGBPAINT_CANVAS_AREA_H

#include <imgui.h>
#include "paint/canvas.h"
#include "paint/canvas_renderer.h"
#include "core/viewport.h"

// Implementa ICanvasRenderer usando ImDrawList internamente
class ImGuiCanvasRenderer : public ICanvasRenderer
{
public:
	ImDrawList* draw_list = nullptr;
	ImVec2 canvas_pos{};          // posición en pantalla del origen del canvas
	PaintCore::Viewport view{};   // scroll (en imagen) + zoom

	void draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color) override;
	void draw_rect(int x, int y, int w, int h, const PaintCore::Color& color, int thickness) override;
	void draw_line(int x0, int y0, int x1, int y1, const PaintCore::Color& color, int thickness) override;
};

struct VisualViewport {
	ImVec2 pos{};        // canvas_pos en pantalla
	ImVec2 size{};       // tamaño visible en píxeles
	ImVec2 scroll_px{};  // scroll en píxeles
	float zoom = 1.0f;
};

class CanvasArea
{
public:
	CanvasArea(int w = 256, int h = 256);

	// Llamado por el frontend cada frame
	void render(ImDrawList* draw_list, 
				const ImVec2& canvas_pos, 
				const ImVec2& size, 
				const ImVec2& scroll_px, 
				float zoom, 
				const Palette& palette);

	Canvas& canvas();
	const Canvas& canvas() const;

	void handle_input(const ImVec2& canvas_pos, const ImVec2& size, float zoom);

private:
	Canvas m_canvas;
	// Estado de vista (ImGui)
	VisualViewport m_viewport{};
	bool m_hovered = false;
	bool m_active = false;

	bool m_mouse_was_down = false;

	// Helpers de conversión
	PaintCore::Vec2 screen_to_image(const ImVec2& mouse) const;
	ImVec2 image_to_screen(int ix, int iy) const;
};

#endif