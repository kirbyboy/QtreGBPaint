#ifndef QTREGBPAINT_CANVAS_RESIZE_GIZMO_H
#define QTREGBPAINT_CANVAS_RESIZE_GIZMO_H

#include <imgui.h>

#include "core/color.h"
#include "core/cursor.h"
#include "core/view_transform.h"

class Canvas;
class IGuiBackend;

// Screen-space resize handles for the canvas. The handles are intentionally
// laid out from their own offset instead of the canvas image rect: the grid can
// make the canvas occupy one more screen pixel, and this gizmo can be shifted
// without adding grid conditionals.
class CanvasResizeGizmo
{
public:
	void set_handle_offset(const ImVec2& offset) { m_handle_offset = offset; }
	const ImVec2& handle_offset() const { return m_handle_offset; }

	bool is_resizing() const { return m_resizing; }
	int preview_width() const { return m_preview_width; }
	int preview_height() const { return m_preview_height; }

	// Returns true when the gizmo consumes the mouse input (press, drag or
	// release on a handle). `pressed` is the rising edge of the left button.
	bool handle_input(Canvas& canvas, IGuiBackend* backend,
	                  const PaintCore::ViewTransform& view,
	                  const ImVec2& mouse, bool pressed, bool released,
	                  bool item_hovered, PaintCore::ColorIndex fill,
	                  const ImVec2& clip_min, const ImVec2& clip_max);

	void draw(ImDrawList* draw_list, const PaintCore::ViewTransform& view,
	          const Canvas& canvas) const;

private:
	enum class Handle
	{
		None,
		Right,
		Bottom,
		Corner,
	};

	Handle handle_at(const PaintCore::ViewTransform& view,
	                 const Canvas& canvas, const ImVec2& mouse,
	                 const ImVec2& clip_min, const ImVec2& clip_max) const;
	static PaintCore::CursorType cursor_for(Handle handle);
	void reset();

	ImVec2 m_handle_offset{0.0f, 0.0f};
	bool m_resizing = false;
	Handle m_active_handle = Handle::None;
	int m_start_width = 0;
	int m_start_height = 0;
	int m_preview_width = 0;
	int m_preview_height = 0;
	ImVec2 m_start_mouse{};
};

#endif
