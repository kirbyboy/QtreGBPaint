#ifndef QTREGBPAINT_CANVAS_AREA_H
#define QTREGBPAINT_CANVAS_AREA_H

#include <array>
#include <cstdint>
#include <vector>

#include <imgui.h>
#include "paint/paint_session.h"
#include "paint/canvas_renderer.h"
#include "paint/canvas_grid.h"
#include "paint/canvas_resize_gizmo.h"
#include "core/view_transform.h"

class IGuiBackend;

// Implements ICanvasRenderer using ImDrawList internally.
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

	// Called by the frontend once per frame, before input/render.
	void update_view(const ImVec2& canvas_pos,
					 const ImVec2& size,
					 const ImVec2& scroll_px,
					 float zoom);

	void render(ImDrawList* draw_list, const Palette& palette);

	PaintSession& session();
	const PaintSession& session() const;

	// Image pixel at the top-left of the visible canvas area. Used to place
	// paste near the user's current viewport instead of always at (0,0).
	PaintCore::Vec2 visible_image_origin() const;

	void handle_input(const ImVec2& mouse);

	void set_resize_fill(PaintCore::ColorIndex fill) { m_resize_fill = fill; }
	void set_resize_handle_offset(const ImVec2& offset)
	{
		m_resize_gizmo.set_handle_offset(offset);
	}
	void set_grid_enabled(bool enabled) { m_grid_enabled = enabled; }
	int resize_preview_width() const { return m_resize_gizmo.preview_width(); }
	int resize_preview_height() const { return m_resize_gizmo.preview_height(); }

private:
	IGuiBackend* m_backend = nullptr;
	PaintSession m_session;
	CanvasResizeGizmo m_resize_gizmo;
	CanvasGrid m_grid;
	PaintCore::ColorIndex m_resize_fill = 0;
	bool m_grid_enabled = true;
	void* m_canvas_texture = nullptr;
	int m_texture_width = 0;
	int m_texture_height = 0;
	std::array<PaintCore::Color, 256> m_lut{};
	bool m_lut_ready = false;
	std::vector<PaintCore::Color> m_upload_pixels;

	void* m_floating_texture = nullptr;
	int m_floating_texture_width = 0;
	int m_floating_texture_height = 0;
	std::uint64_t m_floating_texture_revision = 0;

	PaintCore::ViewTransform m_view{};
	bool m_hovered = false;
	bool m_active = false;

	bool m_mouse_was_down = false;
	bool m_floating_dragging = false;
	PaintCore::Vec2 m_floating_last_img{};

	bool update_palette_lut(const Palette& palette);
	void update_canvas_texture(bool palette_changed);
	void update_floating_texture(bool palette_changed);
	bool handle_floating_input(const PaintCore::Vec2& img_pos,
	                           bool pressed, bool released,
	                           bool item_hovered);
	void draw_floating_region(ImDrawList* draw_list,
	                          ImGuiCanvasRenderer& renderer,
	                          const Palette& palette);
	void draw_marquee(ImDrawList* draw_list,
	                  const PaintCore::Rect& rect) const;
};

#endif
