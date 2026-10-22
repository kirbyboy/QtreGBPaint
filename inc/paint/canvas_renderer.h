#ifndef QTREGBPAINT_CANVAS_RENDERER_H
#define QTREGBPAINT_CANVAS_RENDERER_H

#include <cstdint>

class ICanvasRenderer
{
public:
	virtual ~ICanvasRenderer() = default;

	// Primitivas mínimas para pintar el lienzo
	virtual void draw_rect_filled(int x, int y, int w, int h, std::uint32_t color) = 0;
	virtual void draw_rect(int x, int y, int w, int h, std::uint32_t color, int thickness = 1) = 0;

	// Líneas para rejilla/selección
	virtual void draw_line(int x0, int y0, int x1, int y1, std::uint32_t color, int thickness = 1) = 0;
};

#endif