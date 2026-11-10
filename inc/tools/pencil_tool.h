#ifndef QTREGBPAINT_PENCIL_TOOL_H
#define QTREGBPAINT_PENCIL_TOOL_H

#include "core/color.h"
#include "tools/freehand_tool.h"

// One-pixel freehand pencil. It inherits the segment interpolation from
// FreehandTool, so a fast mouse move is drawn as a continuous line.
class PencilTool : public FreehandTool
{
public:
	PencilTool(PaintCore::ColorIndex foreground = 1,
	           PaintCore::ColorIndex background = 0);

protected:
	void stamp(DrawBuffer& buffer, const ImageBuffer& image,
	           int x, int y) override;
};

#endif
