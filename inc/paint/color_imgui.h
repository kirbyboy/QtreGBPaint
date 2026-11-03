#ifndef QTREGBPAINT_COLOR_IMGUI_H
#define QTREGBPAINT_COLOR_IMGUI_H

#include "imgui.h"
#include "core/color.h"

inline ImU32 toImU32(const PaintCore::Color& color)
{
	return IM_COL32(color.r, color.g, color.b, color.a);
}

inline ImVec4 toImVec4(const PaintCore::Color& color)
{
	return ImVec4(color.r / 255.0f, color.g / 255.0f,
				  color.b / 255.0f, color.a / 255.0f);
}

#endif