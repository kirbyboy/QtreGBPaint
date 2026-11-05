#ifndef QTREGBPAINT_CANVAS_AREA_H
#define QTREGBPAINT_CANVAS_AREA_H

#include <array>
#include <vector>

#include <imgui.h>
#include "paint/canvas.h"
#include "paint/canvas_renderer.h"
#include "core/view_transform.h"

class IGuiBackend;

// Implementa ICanvasRenderer usando ImDrawList internamente
class ImGuiCanvasRenderer : public ICanvasRenderer
{
public:
	ImDrawList* draw_list = nullptr;
	PaintCore::ViewTransform view{};

	void draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color) override;
	void draw_rect(int x, int y, int w, int h, const PaintCore::Color& color, int thickness) override;
	void draw_line(int x0, int y0, int x1, int y1, const PaintCore::Color& color, int thickness) override;
};

class CanvasArea
{
public:
	CanvasArea(IGuiBackend* backend = nullptr, int w = 256, int h = 256);
	~CanvasArea();

	CanvasArea(const CanvasArea&) = delete;
	CanvasArea& operator=(const CanvasArea&) = delete;

	// Llamado por el frontend una vez por frame, antes de input/render.
	void update_view(const ImVec2& canvas_pos,
					 const ImVec2& size,
					 const ImVec2& scroll_px,
					 float zoom);

	void render(ImDrawList* draw_list, const Palette& palette);

	Canvas& canvas();
	const Canvas& canvas() const;

	void handle_input(const ImVec2& mouse);

private:
	IGuiBackend* m_backend = nullptr;
	Canvas m_canvas;
	void* m_canvas_texture = nullptr;
	int m_texture_width = 0;
	int m_texture_height = 0;
	std::array<PaintCore::Color, 256> m_lut{};
	bool m_lut_ready = false;
	std::vector<PaintCore::Color> m_upload_pixels;

	PaintCore::ViewTransform m_view{};
	bool m_hovered = false;
	bool m_active = false;

	bool m_mouse_was_down = false;

	void update_canvas_texture(const Palette& palette);
};

#endif