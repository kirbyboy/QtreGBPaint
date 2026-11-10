#include "tools/line_tool.h"

#include <algorithm>

#include "core/brush.h"
#include "core/line.h"
#include "core/line_stamp.h"
#include "core/shape_constraint.h"
#include "tools/line_width.h"

LineTool::LineTool(PaintCore::ColorIndex foreground,
                   PaintCore::ColorIndex background)
	: DrawTool(foreground, background),
	  m_stroke_offsets(PaintCore::make_line_stamp_offsets(m_stroke_width))
{
	add_option(LineWidth::make_option());
}

void LineTool::on_mouse_press(PaintSession& canvas,
                              const PaintCore::Vec2& pos, int button)
{
	DrawTool::on_mouse_press(canvas, pos, button);
	if (m_drawing)
	{
		// Freeze the width for the whole stroke, like the stroke colors.
		m_stroke_width = option_value("width");
		m_stroke_offsets =
			PaintCore::make_line_stamp_offsets(m_stroke_width);
	}
}

void LineTool::draw_preview(DrawBuffer& buffer,
                             const PaintCore::Modifiers& modifiers)
{
	const PaintCore::Vec2 end = modifiers.shift
		? PaintCore::snapped_line_end(m_start_pos, m_current_pos)
		: m_current_pos;

	const int margin = PaintCore::brush_margin(m_stroke_offsets);
	const int min_x = std::min(m_start_pos.x, end.x) - margin;
	const int min_y = std::min(m_start_pos.y, end.y) - margin;
	const int max_x = std::max(m_start_pos.x, end.x) + margin;
	const int max_y = std::max(m_start_pos.y, end.y) + margin;

	buffer.ensure_fit(min_x, min_y);
	buffer.ensure_fit(max_x, max_y);
	buffer.clear();

	const int start_x = m_start_pos.x - buffer.x();
	const int start_y = m_start_pos.y - buffer.y();
	const int end_x = end.x - buffer.x();
	const int end_y = end.y - buffer.y();

	PaintCore::rasterize_line(
		start_x, start_y, end_x, end_y,
		[&](int x, int y)
		{
			for (const PaintCore::Vec2& offset : m_stroke_offsets)
			{
				buffer.set_pixel(x + offset.x, y + offset.y,
				                 m_stroke_foreground);
			}
		});
}
