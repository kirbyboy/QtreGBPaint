#include "tools/square_tool.h"

SquareTool::SquareTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg): 
m_foreground(fg),
m_background(bg)
{}


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

	for (int y = top; y <= bottom; ++y)
	{
		for (int x = left; x <= right; ++x)
		{
			bool is_border = (x == left || x == right || y == top || y == bottom);
			buffer.set_pixel(x, y, is_border ? m_foreground : m_background);
		}
	}
}
