#ifndef QTREGBPAINT_VEC2_H
#define QTREGBPAINT_VEC2_H

namespace PaintCore
{
	struct Vec2
	{
		int x = 0;
		int y = 0;

		Vec2() = default;
		Vec2(int _x, int _y) : x(_x), y(_y) {}
	};
}

#endif