#ifndef QTREGBPAINT_CANVAS_RENDERER_H
#define QTREGBPAINT_CANVAS_RENDERER_H

#include "core/color.h"

class ICanvasRenderer
{
public:
	virtual ~ICanvasRenderer() = default;

	// Minimal primitive for painting the canvas.
	virtual void draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color) = 0;
};

#endif