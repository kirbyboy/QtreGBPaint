#ifndef QTREGBPAINT_SQUARE_TOOL_H
#define QTREGBPAINT_SQUARE_TOOL_H

#include "core/vec2.h"
#include "paint/draw_buffer.h"
#include "tools/shape_tool.h"

class SquareTool : public ShapeTool
{
	public:
		SquareTool(PaintCore::ColorIndex fg = 1, PaintCore::ColorIndex bg = 0);

	protected:
		void draw_preview(DrawBuffer& buffer, const PaintCore::Modifiers& modifiers) override;
};
#endif
