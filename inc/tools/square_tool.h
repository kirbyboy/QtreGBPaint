#ifndef QTREGBPAINT_SQUARE_TOOL_H
#define QTREGBPAINT_SQUARE_TOOL_H

#include "core/vec2.h"
#include "paint/draw_buffer.h"
#include "tools/draw_tool.h"

class SquareTool : public DrawTool
{
	public:
		SquareTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg);

	protected:
		void draw_preview(DrawBuffer& buffer) override;

	private:
		PaintCore::ColorIndex m_foreground;
		PaintCore::ColorIndex m_background;
};
#endif