#ifndef QTREGBPAINT_CANVAS_AREA_H
#define QTREGBPAINT_CANVAS_AREA_H

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include "paint/paint_session.h"
#include "paint/canvas_renderer.h"
#include "paint/canvas_grid.h"
#include "paint/canvas_resize_gizmo.h"
#include "paint/canvas_tile_grid.h"
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
	CanvasArea(IGuiBackend* backend, PaintSession& session);
	~CanvasArea();

	CanvasArea(const CanvasArea&) = delete;
	CanvasArea& operator=(const CanvasArea&) = delete;

	// Called by the frontend once per frame, before input/render.
	void update_view(const ImVec2& canvas_pos,
					 const ImVec2& size,
					 const ImVec2& scroll_px,
					 float zoom);

	void render(ImDrawList* draw_list, const Palette& palette,
	            PaintCore::ColorIndex foreground,
	            PaintCore::ColorIndex background);

	PaintSession& session();
	const PaintSession& session() const;

	// True when the given screen position maps to an image pixel. The
	// frontend uses it to show the tool cursor only over the image.
	bool pointer_over_image(const ImVec2& mouse) const;

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
	PaintSession& m_session;
	CanvasResizeGizmo m_resize_gizmo;
	CanvasGrid m_grid;
	PaintCore::ColorIndex m_resize_fill = 0;
	bool m_grid_enabled = true;

	// Canvas image split into fixed-size tiles. Only the visible tiles (plus a
	// one-tile margin) are cached, so VRAM stays bounded on large canvases.
	struct CanvasTile
	{
		void* texture = nullptr;
		int width = 0;
		int height = 0;
	};
	std::unordered_map<std::uint64_t, CanvasTile> m_canvas_tiles;
	int m_tile_image_width = 0;
	int m_tile_image_height = 0;
	bool m_tiles_full_upload = false;

	std::array<PaintCore::Color, 256> m_lut{};
	bool m_lut_ready = false;
	std::vector<PaintCore::Color> m_upload_pixels;

	void* m_floating_texture = nullptr;
	int m_floating_texture_width = 0;
	int m_floating_texture_height = 0;
	std::uint64_t m_floating_texture_revision = 0;

	PaintCore::ViewTransform m_view{};
	bool m_hovered = false;
	bool m_hover_inside = false;
	PaintCore::Vec2 m_hover_image_pos{};
	bool m_active = false;

	bool m_left_was_down = false;
	bool m_right_was_down = false;
	int m_stroke_button = 0;
	bool m_suppress_stroke = false;
	bool m_floating_dragging = false;
	PaintCore::Vec2 m_floating_last_img{};
	int m_edit_handle = -1;

	bool update_palette_lut(const Palette& palette);
	bool update_canvas_tiles(bool palette_changed);
	void draw_canvas_tiles(ImDrawList* draw_list);
	void destroy_canvas_tiles();
	bool upload_canvas_tile(CanvasTile& tile, int tile_x, int tile_y,
	                        const ImageBuffer& image);
	void update_floating_texture(bool palette_changed);
	bool handle_floating_input(const PaintCore::Vec2& img_pos,
	                           bool pressed, bool released,
	                           bool item_hovered);
	void draw_floating_region(ImDrawList* draw_list,
	                          ImGuiCanvasRenderer& renderer,
	                          const Palette& palette);
	void draw_marquee(ImDrawList* draw_list,
	                  const PaintCore::Rect& rect) const;
	void draw_stamp_preview(ImDrawList* draw_list, const Palette& palette,
	                        PaintCore::ColorIndex foreground,
	                        PaintCore::ColorIndex background) const;
	int edit_handle_at(const ImVec2& mouse) const;
	void draw_edit_overlay(ImDrawList* draw_list);
};

#endif
