#ifndef QTREGBPAINT_SQUARE_TOOL_H
#define QTREGBPAINT_SQUARE_TOOL_H

#include "core/vec2.h"
#include "tools/draw_tool.h"

class SquareTool : public DrawTool
{
	public:
		SquareTool(PaintCore::ColorIndex fg, PaintCore::ColorIndex bg);

	protected:
		void generate_content(const PaintCore::Vec2& currentPos) override;

	private:
		PaintCore::ColorIndex m_foreground;
		PaintCore::ColorIndex m_background;
};

#endif