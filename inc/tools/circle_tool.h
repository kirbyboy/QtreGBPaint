#ifndef QTREGBPAINT_CIRCLE_TOOL_H
#define QTREGBPAINT_CIRCLE_TOOL_H

#include "core/vec2.h"
#include "paint/draw_buffer.h"
#include "tools/shape_tool.h"

// Ellipse tool. Without Shift the ellipse is inscribed in the drag bounding
// box; with Shift it uses the same square constraint as the rectangle and
// selection tools, so it becomes a perfect circle.
class CircleTool : public ShapeTool
{
	public:
		CircleTool(PaintCore::ColorIndex fg = 1,
		           PaintCore::ColorIndex bg = 0);

	protected:
		void draw_preview(DrawBuffer& buffer,
		                  const PaintCore::Modifiers& modifiers) override;
};

#endif
