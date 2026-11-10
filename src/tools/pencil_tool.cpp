#include "tools/pencil_tool.h"

PencilTool::PencilTool(PaintCore::ColorIndex foreground,
                       PaintCore::ColorIndex background)
	: FreehandTool(foreground, background)
{
}

void PencilTool::stamp(DrawBuffer& buffer, const ImageBuffer& image,
                       int x, int y)
{
	(void)image;
	buffer.set_pixel(x - buffer.x(), y - buffer.y(), m_stroke_foreground);
}
