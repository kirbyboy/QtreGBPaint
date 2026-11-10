#include "tools/square_tool.h"

#include "core/shape_constraint.h"

SquareTool::SquareTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg):
ShapeTool(fg, bg)
{
}


void SquareTool::draw_preview(DrawBuffer& buffer,
                              const PaintCore::Modifiers& modifiers)
{
	const PaintCore::Vec2 end = modifiers.shift
		? PaintCore::square_constrained_end(m_start_pos, m_current_pos)
		: m_current_pos;

	buffer.ensure_fit(m_start_pos.x, m_start_pos.y);
	buffer.ensure_fit(end.x, end.y);

	int rel_start_x = m_start_pos.x - buffer.x();
	int rel_start_y = m_start_pos.y - buffer.y();

	int rel_current_x = end.x - buffer.x();
	int rel_current_y = end.y - buffer.y();

	int left   = std::min(rel_start_x, rel_current_x);
	int top    = std::min(rel_start_y, rel_current_y);
	int right  = std::max(rel_start_x, rel_current_x);
	int bottom = std::max(rel_start_y, rel_current_y);

	buffer.clear();

	const Mode shape_mode = mode();

	for (int y = top; y <= bottom; ++y)
	{
		for (int x = left; x <= right; ++x)
		{
			bool is_border = (x == left || x == right || y == top || y == bottom);
			switch (shape_mode)
			{
				case Mode::Outline:
					if (is_border)
					{
						buffer.set_pixel(x, y, m_stroke_foreground);
					}
					break;
				case Mode::Fill:
					buffer.set_pixel(x, y, m_stroke_background);
					break;
				case Mode::Both:
					buffer.set_pixel(x, y, is_border ? m_stroke_foreground
					                                : m_stroke_background);
					break;
			}
		}
	}
}
