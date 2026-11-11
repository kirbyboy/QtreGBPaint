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
#include "paint/checkerboard.h"
#include "core/view_transform.h"

class IGuiBackend;

// Implements ICanvasRenderer using ImDrawList internally.
class ImGuiCanvasRenderer : public ICanvasRenderer
{
public:
	ImDrawList* draw_list = nullptr;
	PaintCore::ViewTransform view{};

	void draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color) override;
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

	// Image pixel under a screen position. The result is unclamped: during a
	// drag it can fall outside the image, which the status bar shows as-is.
	PaintCore::Vec2 image_position_at(const ImVec2& mouse) const;

	// Image pixel and palette index under a screen position. Returns false
	// when the position is outside the image.
	bool image_pixel_at(const ImVec2& mouse, PaintCore::Vec2& pos,
	                    PaintCore::ColorIndex& color) const;

	// True when the pointer is over the canvas item (image area), not over the
	// palette, status bar or the child scrollbars.
	bool mouse_over_canvas() const { return m_mouse_over_canvas; }

	// Screen position of the center of the visible image area. Falls back to
	// the viewport center when no image pixel is visible. Used as the default
	// zoom anchor when the pointer is not over the canvas.
	ImVec2 visible_image_center() const;

	// Scroll that keeps `screen_anchor` over the same image point when the
	// zoom changes to `new_zoom`. Uses the last rendered view.
	bool scroll_for_zoom_anchor(const ImVec2& screen_anchor, float new_zoom,
	                            ImVec2& scroll) const;

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

	struct PreviewTile
	{
		void* texture = nullptr;
		int width = 0;
		int height = 0;
		std::uint64_t revision = 0;
	};
	std::unordered_map<std::uint64_t, PreviewTile> m_preview_tiles;
	std::vector<PaintCore::Color> m_preview_pixels;
	PaintUi::CheckerboardTexture m_checkerboard;

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
	bool m_mouse_over_canvas = false;
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
	bool draw_preview_tiles(ImDrawList* draw_list, const Palette& palette);
	void destroy_preview_tiles();
	void ensure_checkerboard_texture();
	void draw_checkerboard(ImDrawList* draw_list, const ImVec2& min,
	                       const ImVec2& max,
	                       const ImVec2& origin) const;
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
	// Checkerboard for preview pixels whose palette index is transparent; the
	// tile cache still holds the old opaque canvas under the active stroke.
	void draw_preview_transparency(ImDrawList* draw_list,
	                               const Palette& palette) const;
	int edit_handle_at(const ImVec2& mouse) const;
	void draw_edit_overlay(ImDrawList* draw_list);
};

#endif
