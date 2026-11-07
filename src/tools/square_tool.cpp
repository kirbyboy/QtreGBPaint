#include "tools/square_tool.h"

SquareTool::SquareTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg):
DrawTool(fg, bg)
{
	add_option(ToolOption::choice(
		"mode", "Shape", static_cast<int>(Mode::Outline),
		{
			{static_cast<int>(Mode::Outline), "Outline", "O", "rect.outline"},
			{static_cast<int>(Mode::Fill), "Fill", "F", "rect.fill"},
			{static_cast<int>(Mode::Both), "Both", "B", "rect.both"},
		}));
}


void SquareTool::draw_preview(DrawBuffer &buffer)
{
	buffer.ensure_fit(m_current_pos.x, m_current_pos.y);

	int rel_start_x = m_start_pos.x - buffer.x();
	int rel_start_y = m_start_pos.y - buffer.y();

	int rel_current_x = m_current_pos.x - buffer.x();
	int rel_current_y = m_current_pos.y - buffer.y();

	int left   = std::min(rel_start_x, rel_current_x);
	int top    = std::min(rel_start_y, rel_current_y);
	int right  = std::max(rel_start_x, rel_current_x);
	int bottom = std::max(rel_start_y, rel_current_y);

	buffer.clear();

	const Mode mode = static_cast<Mode>(option_value("mode"));

	for (int y = top; y <= bottom; ++y)
	{
		for (int x = left; x <= right; ++x)
		{
			bool is_border = (x == left || x == right || y == top || y == bottom);
			switch (mode)
			{
				case Mode::Outline:
					if (is_border)
					{
						buffer.set_pixel(x, y, m_foreground);
					}
					break;
				case Mode::Fill:
					buffer.set_pixel(x, y, m_background);
					break;
				case Mode::Both:
					buffer.set_pixel(x, y, is_border ? m_foreground : m_background);
					break;
			}
		}
	}
}
