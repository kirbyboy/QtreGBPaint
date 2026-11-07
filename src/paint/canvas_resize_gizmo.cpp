#include "paint/canvas_resize_gizmo.h"

#include <algorithm>
#include <cmath>

#include "core/ibackend.h"
#include "paint/canvas.h"

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

HandleGeometry compute_geometry(const PaintCore::ViewTransform& view,
                                int width, int height,
                                const ImVec2& clip_min, const ImVec2& clip_max,
                                const ImVec2& offset)
{
	HandleGeometry geometry;

	const PaintCore::Vec2f origin = view.image_to_screen({0.0f, 0.0f});
	const PaintCore::Vec2f end = view.image_to_screen(
		{static_cast<float>(width), static_cast<float>(height)});
	const float right = end.x + offset.x;
	const float bottom = end.y + offset.y;
	const float visible_left = std::max(origin.x, clip_min.x);
	const float visible_top = std::max(origin.y, clip_min.y);
	const float visible_right = std::min(end.x, clip_max.x);
	const float visible_bottom = std::min(end.y, clip_max.y);
	const float half_length = kEdgeHandleLength * 0.5f;
	const float zoom = view.zoom > 0.0f ? view.zoom : 1.0f;

	// Reserve room for the handle plus its outward hit area. Then snap down to
	// the last whole canvas pixel that fits. This keeps the handles at the
	// maximum visible pixel extent instead of depending on the partial pixel
	// left by scroll snapping.
	constexpr float kHandleSpace =
		kHandleGap + kEdgeHandleWidth + kHitOutset;
	const float max_right = std::min(right, clip_max.x - kHandleSpace);
	const float max_bottom = std::min(bottom, clip_max.y - kHandleSpace);
	const int columns = static_cast<int>(
		std::floor((max_right - origin.x) / zoom));
	const int rows = static_cast<int>(
		std::floor((max_bottom - origin.y) / zoom));
	const float effective_right =
		origin.x + std::max(0, columns) * zoom;
	const float effective_bottom =
		origin.y + std::max(0, rows) * zoom;

	const float right_x0 = effective_right + kHandleGap;
	const float bottom_y0 = effective_bottom + kHandleGap;

	if (visible_left < visible_right && visible_top < visible_bottom &&
	    effective_right >= visible_left && right_x0 < clip_max.x)
	{
		geometry.right_visible = true;
		const float y0 = std::max(clip_min.y, effective_bottom - half_length);
		geometry.right_min = ImVec2(right_x0, y0);
		geometry.right_max = ImVec2(right_x0 + kEdgeHandleWidth,
		                            effective_bottom);
	}

	if (visible_left < visible_right && visible_top < visible_bottom &&
	    effective_bottom >= visible_top && bottom_y0 < clip_max.y)
	{
		geometry.bottom_visible = true;
		const float x0 = std::max(clip_min.x, effective_right - half_length);
		geometry.bottom_min = ImVec2(x0, bottom_y0);
		geometry.bottom_max = ImVec2(effective_right,
		                             bottom_y0 + kEdgeHandleWidth);
	}

	if (visible_left < visible_right && visible_top < visible_bottom &&
	    effective_right >= visible_left && effective_bottom >= visible_top &&
	    right_x0 < clip_max.x && bottom_y0 < clip_max.y)
	{
		geometry.corner_visible = true;
		geometry.corner_min = ImVec2(right_x0, bottom_y0);
		geometry.corner_max = ImVec2(right_x0 + kCornerSize,
		                             bottom_y0 + kCornerSize);
	}

	return geometry;
}
}

CanvasResizeGizmo::Handle CanvasResizeGizmo::handle_at(
	const PaintCore::ViewTransform& view, const Canvas& canvas,
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

bool CanvasResizeGizmo::handle_input(Canvas& canvas, IGuiBackend* backend,
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
                             const Canvas& canvas) const
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
