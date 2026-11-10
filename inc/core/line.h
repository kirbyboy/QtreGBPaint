#ifndef QTREGBPAINT_LINE_H
#define QTREGBPAINT_LINE_H

#include <cstdlib>

namespace PaintCore
{
// Calls plot(x, y) for every pixel on the integer Bresenham line from (x0, y0)
// to (x1, y1), both endpoints included. The callback receives coordinates in
// the same integer space as the endpoints.
//
// This is the common rasterizer for the line tool and for freehand tools:
// pencil, brush and eraser can connect their previous and current pointer
// positions with it so fast mouse movement does not leave gaps.
template <typename Plot>
void rasterize_line(int x0, int y0, int x1, int y1, Plot&& plot)
{
	const int dx = std::abs(x1 - x0);
	const int sx = x0 < x1 ? 1 : -1;
	const int dy = -std::abs(y1 - y0);
	const int sy = y0 < y1 ? 1 : -1;
	int error = dx + dy;

	while (true)
	{
		plot(x0, y0);
		if (x0 == x1 && y0 == y1)
		{
			break;
		}

		const int doubled = error * 2;
		if (doubled >= dy)
		{
			error += dy;
			x0 += sx;
		}
		if (doubled <= dx)
		{
			error += dx;
			y0 += sy;
		}
	}
}
}

#endif
