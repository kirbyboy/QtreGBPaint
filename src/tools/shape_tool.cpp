#include "tools/shape_tool.h"

ShapeTool::ShapeTool(PaintCore::ColorIndex foreground,
                     PaintCore::ColorIndex background)
	: DrawTool(foreground, background)
{
	add_option(ToolOption::choice(
		"mode", "Shape", static_cast<int>(Mode::Outline),
		{
			{static_cast<int>(Mode::Outline), "Outline", "O", "rect.outline", {}},
			{static_cast<int>(Mode::Fill), "Fill", "F", "rect.fill", {}},
			{static_cast<int>(Mode::Both), "Both", "B", "rect.both", {}},
		}));
}

ShapeTool::Mode ShapeTool::mode() const
{
	return static_cast<Mode>(option_value("mode"));
}
