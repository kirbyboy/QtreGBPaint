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

	struct Vec2f
	{
		float x = 0.0f;
		float y = 0.0f;

		Vec2f() = default;
		Vec2f(float _x, float _y) : x(_x), y(_y) {}
	};
}

#endif