#ifndef QTREGBPAINT_LINE_WIDTH_H
#define QTREGBPAINT_LINE_WIDTH_H

#include <string>
#include <utility>
#include <vector>

#include "core/vec2.h"
#include "tools/tool_option.h"

// Shared width option for LineTool and CurveTool. The options panel shows five
// horizontal lines of 1..5 pixels of thickness; the stroke itself sweeps the
// footprint from PaintCore::make_line_stamp_offsets().
namespace LineWidth
{
constexpr int kMin = 1;
constexpr int kMax = 5;
constexpr int kPreviewLength = 24;

inline std::vector<PaintCore::Vec2> make_preview_offsets(int width)
{
	std::vector<PaintCore::Vec2> offsets;
	for (int y = 0; y < width; ++y)
	{
		for (int x = 0; x < kPreviewLength; ++x)
		{
			offsets.push_back({x, y});
		}
	}
	return offsets;
}

inline ToolOption make_option()
{
	std::vector<ToolOptionChoice> choices;
	for (int width = kMin; width <= kMax; ++width)
	{
		ToolOptionChoice choice;
		choice.value = width;
		choice.label = "Width " + std::to_string(width) + " px";
		choice.short_label = std::to_string(width);
		choice.preview = make_preview_offsets(width);
		choices.push_back(std::move(choice));
	}

	return ToolOption::choice("width", "Width", kMin, std::move(choices));
}
}

#endif
