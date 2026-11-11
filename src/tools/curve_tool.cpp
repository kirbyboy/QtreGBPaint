#include "tools/curve_tool.h"

#include <algorithm>

#include "core/bezier.h"
#include "core/brush.h"
#include "core/line.h"
#include "core/line_stamp.h"
#include "core/shape_constraint.h"
#include "paint/image_buffer.h"
#include "paint/paint_session.h"
#include "tools/line_width.h"

CurveTool::CurveTool(PaintCore::ColorIndex foreground,
                     PaintCore::ColorIndex background)
	: DrawTool(foreground, background),
	  m_stroke_offsets(PaintCore::make_line_stamp_offsets(m_stroke_width))
{
	add_option(LineWidth::make_option());
}

void CurveTool::on_mouse_press(PaintSession& canvas,
                               const PaintCore::Vec2& pos, int button)
{
	if (m_phase != Phase::Idle)
	{
		return;
	}

	DrawTool::on_mouse_press(canvas, pos, button);
	if (!m_drawing)
	{
		return;
	}

	// Freeze the width for the whole curve, including the pending handles
	// phase, like the stroke colors.
	m_stroke_width = option_value("width");
	m_stroke_offsets =
		PaintCore::make_line_stamp_offsets(m_stroke_width);

	m_phase = Phase::Endpoints;
	m_a = pos;
	m_b = pos;
	m_p1 = pos;
	m_p2 = pos;
	m_bounds_width = canvas.getWidth();
	m_bounds_height = canvas.getHeight();
	rebuild_preview();
}

void CurveTool::on_mouse_move(PaintSession& canvas,
                              const PaintCore::Vec2& pos)
{
	if (m_phase != Phase::Endpoints || !m_drawing)
	{
		return;
	}

	m_b = canvas.modifiers().shift
		? PaintCore::snapped_line_end(m_a, pos)
		: pos;
	rebuild_preview();
}

void CurveTool::on_mouse_release(PaintSession& canvas,
                                 const PaintCore::Vec2& pos, int button)
{
	if (m_phase != Phase::Endpoints || !m_drawing ||
	    button != m_active_button)
	{
		return;
	}

	m_b = canvas.modifiers().shift
		? PaintCore::snapped_line_end(m_a, pos)
		: pos;
	m_drawing = false;
	m_active_button = 0;
	initialize_controls();
	m_phase = Phase::Handles;
	rebuild_preview();
}

void CurveTool::on_cancel(PaintSession& canvas)
{
	(void)canvas;
	reset_state();
}

bool CurveTool::drag_anchor(PaintCore::Vec2& anchor) const
{
	// Only the initial endpoint drag has two coordinates. Moving the control
	// handles is a single-point operation for the status bar.
	if (m_phase != Phase::Endpoints || !m_drawing)
	{
		return false;
	}
	anchor = m_a;
	return true;
}

void CurveTool::set_colors(PaintCore::ColorIndex foreground,
                           PaintCore::ColorIndex background)
{
	// The pending curve is a continuation of the initial stroke: keep its
	// colors frozen until it is committed or cancelled.
	if (m_phase == Phase::Handles)
	{
		m_foreground = foreground;
		m_background = background;
		return;
	}
	DrawTool::set_colors(foreground, background);
}

bool CurveTool::has_pending_edit() const
{
	return m_phase == Phase::Handles;
}

bool CurveTool::commit_pending_edit(PaintSession& canvas)
{
	if (m_phase != Phase::Handles)
	{
		return false;
	}

	canvas.commit_draw_buffer(m_preview_buffer);
	reset_state();
	return true;
}

void CurveTool::cancel_pending_edit(PaintSession& canvas)
{
	(void)canvas;
	reset_state();
}

bool CurveTool::edit_overlay(EditOverlay& overlay) const
{
	if (m_phase != Phase::Handles)
	{
		return false;
	}

	overlay.guides.clear();
	overlay.handles.clear();

	overlay.guides.push_back(EditGuide{m_a, m_p1});
	overlay.guides.push_back(EditGuide{m_p1, m_p2});
	overlay.guides.push_back(EditGuide{m_p2, m_b});

	overlay.handles.push_back(EditHandle{m_a, false});
	overlay.handles.push_back(EditHandle{m_p1, true});
	overlay.handles.push_back(EditHandle{m_p2, true});
	overlay.handles.push_back(EditHandle{m_b, false});
	return true;
}

void CurveTool::move_edit_handle(int index, const PaintCore::Vec2& pos)
{
	if (m_phase != Phase::Handles)
	{
		return;
	}

	if (index == 1)
	{
		m_p1 = clamp_to_canvas(pos);
		rebuild_preview();
	}
	else if (index == 2)
	{
		m_p2 = clamp_to_canvas(pos);
		rebuild_preview();
	}
}

void CurveTool::draw_preview(DrawBuffer& buffer,
                             const PaintCore::Modifiers& modifiers)
{
	(void)buffer;
	(void)modifiers;
	// DrawTool's preview is rebuilt explicitly while the curve is edited.
}

void CurveTool::reset_state()
{
	m_phase = Phase::Idle;
	m_drawing = false;
	m_active_button = 0;
	m_preview_buffer.reset();
}

void CurveTool::initialize_controls()
{
	const int dx = m_b.x - m_a.x;
	const int dy = m_b.y - m_a.y;
	m_p1 = clamp_to_canvas(
		PaintCore::Vec2{m_a.x + dx / 3, m_a.y + dy / 3});
	m_p2 = clamp_to_canvas(
		PaintCore::Vec2{m_a.x + (dx * 2) / 3, m_a.y + (dy * 2) / 3});
}

PaintCore::Vec2 CurveTool::clamp_to_canvas(
	const PaintCore::Vec2& pos) const
{
	if (m_bounds_width <= 0 || m_bounds_height <= 0)
	{
		return pos;
	}
	return PaintCore::Vec2{
		std::clamp(pos.x, 0, m_bounds_width - 1),
		std::clamp(pos.y, 0, m_bounds_height - 1),
	};
}

void CurveTool::rebuild_preview()
{
	m_preview_buffer.set_bounds(m_bounds_width, m_bounds_height);

	const int min_x = std::min(std::min(m_a.x, m_b.x),
	                           std::min(m_p1.x, m_p2.x));
	const int max_x = std::max(std::max(m_a.x, m_b.x),
	                           std::max(m_p1.x, m_p2.x));
	const int min_y = std::min(std::min(m_a.y, m_b.y),
	                           std::min(m_p1.y, m_p2.y));
	const int max_y = std::max(std::max(m_a.y, m_b.y),
	                           std::max(m_p1.y, m_p2.y));
	const int margin = PaintCore::brush_margin(m_stroke_offsets);

	m_preview_buffer.clear();
	m_preview_buffer.ensure_fit(min_x - margin, min_y - margin);
	m_preview_buffer.ensure_fit(max_x + margin, max_y + margin);

	const int buffer_x = m_preview_buffer.x();
	const int buffer_y = m_preview_buffer.y();
	const auto plot = [&](int x, int y)
	{
		for (const PaintCore::Vec2& offset : m_stroke_offsets)
		{
			m_preview_buffer.set_pixel(
				x + offset.x - buffer_x,
				y + offset.y - buffer_y,
				m_stroke_foreground);
		}
	};

	if (m_phase == Phase::Endpoints)
	{
		PaintCore::rasterize_line(m_a.x, m_a.y, m_b.x, m_b.y, plot);
	}
	else
	{
		PaintCore::rasterize_cubic_bezier(
			PaintCore::CubicBezier{m_a, m_p1, m_p2, m_b}, plot);
	}
}
