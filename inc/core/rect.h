#ifndef QTREGBPAINT_CORE_RECT_H
#define QTREGBPAINT_CORE_RECT_H

#include <algorithm>

namespace PaintCore
{
	struct Rect
	{
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;

		bool empty() const { return w <= 0 || h <= 0; }
		int right() const { return x + w; }
		int bottom() const { return y + h; }

		void reset() { x = y = w = h = 0; }

		void unite(const Rect& other)
		{
			if (other.empty())
			{
				return;
			}
			if (empty())
			{
				*this = other;
				return;
			}

			const int x0 = std::min(x, other.x);
			const int y0 = std::min(y, other.y);
			const int x1 = std::max(right(), other.right());
			const int y1 = std::max(bottom(), other.bottom());

			x = x0;
			y = y0;
			w = x1 - x0;
			h = y1 - y0;
		}
	};
}

#endif
