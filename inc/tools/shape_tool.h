#ifndef QTREGBPAINT_SHAPE_TOOL_H
#define QTREGBPAINT_SHAPE_TOOL_H

#include "core/color.h"
#include "tools/draw_tool.h"

// Shared base for closed shape tools (rectangle, circle). It owns the
// Outline/Fill/Both option: Outline paints only the border with the
// foreground color, Fill paints the interior with the background color, and
// Both paints the fill and then the outline on top.
class ShapeTool : public DrawTool
{
	public:
		enum class Mode : int
		{
			Outline = 0,
			Fill = 1,
			Both = 2,
		};

		ShapeTool(PaintCore::ColorIndex foreground = 1,
		          PaintCore::ColorIndex background = 0);

	protected:
		Mode mode() const;
};

#endif
