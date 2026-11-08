#include "paint/canvas_resize_gizmo.h"

#include <algorithm>
#include <cmath>

#include "core/ibackend.h"
#include "paint/paint_session.h"
#include "tools/base_tool.h"

namespace
{
constexpr float kCornerSize = 8.0f;
constexpr float kEdgeHandleWidth = 8.0f;
constexpr float kEdgeHandleLength = 16.0f;
constexpr float kHandleGap = 1.0f;
constexpr float kHitOutset = 2.0f;

constexpr ImU32 kHandleFill = IM_COL32(235, 235, 235, 255);
constexpr ImU32 kHandleBorder = IM_COL32(20, 20, 20, 255);
constexpr ImU32 kPreviewBorder = IM_COL32(90, 160, 255, 220);

bool hit_right(float x, float y, float x0, float y0, float x1, float y1)
{
	return x >= x0 && x <= x1 + kHitOutset &&
	       y >= y0 - kHitOutset && y <= y1 + kHitOutset;
}

bool hit_bottom(float x, float y, float x0, float y0, float x1, float y1)
{
	return x >= x0 - kHitOutset && x <= x1 + kHitOutset &&
	       y >= y0 && y <= y1 + kHitOutset;
}

bool hit_corner(float x, float y, float x0, float y0, float x1, float y1)
{
	return x >= x0 && x <= x1 + kHitOutset &&
	       y >= y0 && y <= y1 + kHitOutset;
}

struct HandleGeometry
{
	bool right_visible = false;
	ImVec2 right_min{};
	ImVec2 right_max{};

	bool bottom_visible = false;
	ImVec2 bottom_min{};
	ImVec2 bottom_max{};

	bool corner_visible = false;
	ImVec2 corner_min{};
	ImVec2 corner_max{};
};

// Start of a centered handle on the [span_min, span_max] visible span, rounded
// to whole screen pixels so the drawn rectangles stay crisp.
float centered_start(float span_min, float span_max, float length)
{
	return std::floor((span_min + span_max - length) * 0.5f + 0.5f);
}

HandleGeometry compute_geometry(const PaintCore::ViewTransform& view,
                                int width, int height,
                                const ImVec2& clip_min, const ImVec2& clip_max,
                                const ImVec2& offset)
{
	HandleGeometry geometry;

	const PaintCore::Vec2f origin = view.image_to_screen({0.0f, 0.0f});
	const PaintCore::Vec2f end = view.image_to_screen(
		{static_cast<float>(width), static_cast<float>(height)});

	// Visible span of the canvas inside the child content clip.
	const float visible_left = std::max(origin.x, clip_min.x);
	const float visible_top = std::max(origin.y, clip_min.y);
	const float visible_right = std::min(end.x, clip_max.x);
	const float visible_bottom = std::min(end.y, clip_max.y);
	if (visible_left >= visible_right || visible_top >= visible_bottom)
	{
		return geometry;
	}

	// Handles sit one pixel outside the real canvas edge, so they never mix
	// with the last grid line.
	const float handle_x = end.x + offset.x + kHandleGap;
	const float handle_y = end.y + offset.y + kHandleGap;

	// Right edge: only when the edge is visible and the whole handle fits in
	// the clip. It is centered on the visible vertical span, which is the
	// canvas itself for a small image or the viewport when the canvas is
	// scrolled. Too short a span leaves room only for the corner handle.
	if (handle_x + kEdgeHandleWidth <= clip_max.x &&
	    visible_bottom - visible_top >= kEdgeHandleLength)
	{
		const float y0 = centered_start(visible_top, visible_bottom,
		                                kEdgeHandleLength);
		geometry.right_visible = true;
		geometry.right_min = ImVec2(handle_x, y0);
		geometry.right_max = ImVec2(handle_x + kEdgeHandleWidth,
		                            y0 + kEdgeHandleLength);
	}

	// Bottom edge: same rule, centered on the visible horizontal span.
	if (handle_y + kEdgeHandleWidth <= clip_max.y &&
	    visible_right - visible_left >= kEdgeHandleLength)
	{
		const float x0 = centered_start(visible_left, visible_right,
		                                kEdgeHandleLength);
		geometry.bottom_visible = true;
		geometry.bottom_min = ImVec2(x0, handle_y);
		geometry.bottom_max = ImVec2(x0 + kEdgeHandleLength,
		                             handle_y + kEdgeHandleWidth);
	}

	// Corner: only when the actual corner has been reached. It does not
	// depend on the canvas spanning the handle length, so it is available for
	// canvases too small for the edge handles.
	if (handle_x + kCornerSize <= clip_max.x &&
	    handle_y + kCornerSize <= clip_max.y)
	{
		geometry.corner_visible = true;
		geometry.corner_min = ImVec2(handle_x, handle_y);
		geometry.corner_max = ImVec2(handle_x + kCornerSize,
		                             handle_y + kCornerSize);
	}

	return geometry;
}
}

CanvasResizeGizmo::Handle CanvasResizeGizmo::handle_at(
	const PaintCore::ViewTransform& view, const PaintSession& canvas,
	const ImVec2& mouse, const ImVec2& clip_min, const ImVec2& clip_max) const
{
	const HandleGeometry geometry = compute_geometry(
		view, canvas.getWidth(), canvas.getHeight(), clip_min, clip_max,
		m_handle_offset);

	// The corner wins over the overlapping edge handles. All hitboxes start
	// outside the canvas, so clicking the image edge still draws normally.
	if (geometry.corner_visible &&
	    hit_corner(mouse.x, mouse.y,
	               geometry.corner_min.x, geometry.corner_min.y,
	               geometry.corner_max.x, geometry.corner_max.y))
	{
		return Handle::Corner;
	}
	if (geometry.right_visible &&
	    hit_right(mouse.x, mouse.y,
	              geometry.right_min.x, geometry.right_min.y,
	              geometry.right_max.x, geometry.right_max.y))
	{
		return Handle::Right;
	}
	if (geometry.bottom_visible &&
	    hit_bottom(mouse.x, mouse.y,
	               geometry.bottom_min.x, geometry.bottom_min.y,
	               geometry.bottom_max.x, geometry.bottom_max.y))
	{
		return Handle::Bottom;
	}
	return Handle::None;
}

PaintCore::CursorType CanvasResizeGizmo::cursor_for(Handle handle)
{
	switch (handle)
	{
		case Handle::Right: return PaintCore::CursorType::ResizeEW;
		case Handle::Bottom: return PaintCore::CursorType::ResizeNS;
		case Handle::Corner: return PaintCore::CursorType::ResizeNWSE;
		case Handle::None: break;
	}
	return PaintCore::CursorType::Arrow;
}

bool CanvasResizeGizmo::handle_input(PaintSession& canvas, IGuiBackend* backend,
                                     const PaintCore::ViewTransform& view,
                                     const ImVec2& mouse, bool pressed,
                                     bool released, bool item_hovered,
                                     PaintCore::ColorIndex fill,
                                     const ImVec2& clip_min,
                                     const ImVec2& clip_max)
{
	const bool tool_drawing =
		canvas.active_tool() && canvas.active_tool()->is_drawing();

	if (!m_resizing && !tool_drawing && pressed && item_hovered)
	{
		const Handle handle = handle_at(view, canvas, mouse, clip_min, clip_max);
		if (handle != Handle::None)
		{
			m_resizing = true;
			m_active_handle = handle;
			m_start_width = canvas.getWidth();
			m_start_height = canvas.getHeight();
			m_preview_width = m_start_width;
			m_preview_height = m_start_height;
			m_start_mouse = mouse;
			if (backend)
			{
				backend->set_cursor(cursor_for(handle));
			}
			return true;
		}
	}

	if (!m_resizing)
	{
		if (item_hovered && !tool_drawing && backend)
		{
			const Handle handle = handle_at(view, canvas, mouse, clip_min, clip_max);
			if (handle != Handle::None)
			{
				backend->set_cursor(cursor_for(handle));
			}
		}
		return false;
	}

	const float zoom = view.zoom > 0.0f ? view.zoom : 1.0f;
	const int delta_x = static_cast<int>(
		std::lround((mouse.x - m_start_mouse.x) / zoom));
	const int delta_y = static_cast<int>(
		std::lround((mouse.y - m_start_mouse.y) / zoom));

	const bool changes_width =
		m_active_handle == Handle::Right || m_active_handle == Handle::Corner;
	const bool changes_height =
		m_active_handle == Handle::Bottom || m_active_handle == Handle::Corner;

	m_preview_width = std::max(1, m_start_width + (changes_width ? delta_x : 0));
	m_preview_height = std::max(1, m_start_height + (changes_height ? delta_y : 0));

	if (backend)
	{
		backend->set_cursor(cursor_for(m_active_handle));
	}

	if (released)
	{
		if (m_preview_width != canvas.getWidth() ||
		    m_preview_height != canvas.getHeight())
		{
			canvas.resize(m_preview_width, m_preview_height, fill);
		}
		reset();
		return true;
	}

	return true;
}

void CanvasResizeGizmo::draw(ImDrawList* draw_list,
                             const PaintCore::ViewTransform& view,
                             const PaintSession& canvas) const
{
	if (!draw_list)
	{
		return;
	}

	const int width = m_resizing ? m_preview_width : canvas.getWidth();
	const int height = m_resizing ? m_preview_height : canvas.getHeight();
	if (width <= 0 || height <= 0)
	{
		return;
	}

	const PaintCore::Vec2f origin = view.image_to_screen({0.0f, 0.0f});
	const PaintCore::Vec2f end = view.image_to_screen(
		{static_cast<float>(width), static_cast<float>(height)});
	const float right = end.x + m_handle_offset.x;
	const float bottom = end.y + m_handle_offset.y;

	if (m_resizing)
	{
		draw_list->AddRect(ImVec2(origin.x, origin.y), ImVec2(right, bottom),
		                   kPreviewBorder, 0.0f, 0, 1.0f);
	}

	const ImVec2 clip_min = draw_list->GetClipRectMin();
	const ImVec2 clip_max = draw_list->GetClipRectMax();
	const HandleGeometry geometry = compute_geometry(
		view, width, height, clip_min, clip_max, m_handle_offset);

	auto draw_handle = [&](const ImVec2& min, const ImVec2& max)
	{
		draw_list->AddRectFilled(min, max, kHandleFill);
		draw_list->AddRect(min, max, kHandleBorder);
	};

	if (geometry.right_visible)
	{
		draw_handle(geometry.right_min, geometry.right_max);
	}
	if (geometry.bottom_visible)
	{
		draw_handle(geometry.bottom_min, geometry.bottom_max);
	}
	if (geometry.corner_visible)
	{
		draw_handle(geometry.corner_min, geometry.corner_max);
	}
}

void CanvasResizeGizmo::reset()
{
	m_resizing = false;
	m_active_handle = Handle::None;
	m_start_width = 0;
	m_start_height = 0;
	m_preview_width = 0;
	m_preview_height = 0;
	m_start_mouse = ImVec2{};
}
