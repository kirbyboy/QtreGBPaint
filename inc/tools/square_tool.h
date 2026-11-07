#ifndef QTREGBPAINT_SQUARE_TOOL_H
#define QTREGBPAINT_SQUARE_TOOL_H

#include "core/vec2.h"
#include "paint/draw_buffer.h"
#include "tools/draw_tool.h"

class SquareTool : public DrawTool
{
	public:
		enum class Mode : int
		{
			Outline = 0,
			Fill = 1,
			Both = 2,
		};

		SquareTool(PaintCore::ColorIndex fg = 1, PaintCore::ColorIndex bg = 0);

	protected:
		void draw_preview(DrawBuffer& buffer) override;
};
#endif