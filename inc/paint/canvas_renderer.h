#ifndef QTREGBPAINT_CANVAS_RENDERER_H
#define QTREGBPAINT_CANVAS_RENDERER_H

#include "core/color.h"

class ICanvasRenderer
{
public:
	virtual ~ICanvasRenderer() = default;

	// Minimal primitives for painting the canvas.
	virtual void draw_rect_filled(int x, int y, int w, int h, const PaintCore::Color& color) = 0;
	virtual void draw_rect(int x, int y, int w, int h, const PaintCore::Color& color, int thickness = 1) = 0;

	// Lines for the grid/selection.
	virtual void draw_line(int x0, int y0, int x1, int y1, const PaintCore::Color& color, int thickness = 1) = 0;
};

#endif