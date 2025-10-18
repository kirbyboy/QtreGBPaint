#include "tools/square_tool.h"

SquareTool::SquareTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg): 
m_foreground(fg),
m_background(bg)
{}

void SquareTool::generate_content(const PaintCore::Vec2 &currentPos)
{
	int left = std::min(m_startPos.x, currentPos.x);
	int right = std::max(m_startPos.x, currentPos.x);
	int top = std::min(m_startPos.y, currentPos.y);
	int bottom = std::max(m_startPos.y, currentPos.y);

	for (int y = top; y <= bottom; ++y)
	{
		for (int x = left; x <= right; ++x)
		{
			// Bordes en foreground, relleno en background
			if (x == left || x == right || y == top || y == bottom)
			{
				m_temp_buffer.set_pixel(x - left, y - top, m_foreground);
			}
			else
			{
				m_temp_buffer.set_pixel(x - left, y - top, m_background);
			}
		}
	}
}
