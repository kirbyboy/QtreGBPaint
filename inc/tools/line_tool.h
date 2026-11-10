#ifndef QTREGBPAINT_LINE_TOOL_H
#define QTREGBPAINT_LINE_TOOL_H

#include <vector>

#include "core/color.h"
#include "core/vec2.h"
#include "tools/draw_tool.h"

// Straight line tool. It previews and commits the line from the press point to
// the current pointer position using the shared rasterize_line() helper. Five
// widths select a single pixel, a 2x2 square or a centered cross stamp.
class LineTool : public DrawTool
{
public:
	LineTool(PaintCore::ColorIndex foreground = 1,
	         PaintCore::ColorIndex background = 0);

	void on_mouse_press(PaintSession& canvas,
	                    const PaintCore::Vec2& pos, int button) override;

protected:
	void draw_preview(DrawBuffer& buffer, const PaintCore::Modifiers& modifiers) override;

private:
	int m_stroke_width = 1;
	std::vector<PaintCore::Vec2> m_stroke_offsets;
};

#endif
