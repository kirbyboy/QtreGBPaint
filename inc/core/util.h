#ifndef QTREGBPAINT_UTIL_H
#define QTREGBPAINT_UTIL_H

#include "imgui.h"

namespace PaintCore
{
	inline ImVec2 Vec2Max(const ImVec2& a, const ImVec2& b)
	{
		return ImVec2((a.x > b.x) ? a.x : b.x, (a.y > b.y) ? a.y : b.y);
	}

	inline ImVec2 Vec2Min(const ImVec2& a, const ImVec2& b)
	{
		return ImVec2((a.x < b.x) ? a.x : b.x, (a.y < b.y) ? a.y : b.y);
	}
}

#endif