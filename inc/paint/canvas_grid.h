#ifndef QTREGBPAINT_CANVAS_GRID_H
#define QTREGBPAINT_CANVAS_GRID_H

#include <imgui.h>

#include "core/view_transform.h"

class IGuiBackend;

// Draws the mspaint-style checker grid over the canvas. Each line is a single
// textured quad; the alternating 1 px colors are baked into two tiny 1D
// textures, so the cost does not grow with the number of screen pixels.
class CanvasGrid
{
public:
	explicit CanvasGrid(IGuiBackend* backend = nullptr);
	~CanvasGrid();

	CanvasGrid(const CanvasGrid&) = delete;
	CanvasGrid& operator=(const CanvasGrid&) = delete;

	void render(ImDrawList* draw_list, const PaintCore::ViewTransform& view,
	            int canvas_width, int canvas_height, bool enabled);

private:
	IGuiBackend* m_backend = nullptr;
	void* m_vertical_texture = nullptr;
	int m_vertical_height = 0;
	int m_vertical_phase = 0;
	void* m_horizontal_texture = nullptr;
	int m_horizontal_width = 0;
	int m_horizontal_phase = 0;

	void ensure_textures(int vertical_height, int horizontal_width,
	                     int vertical_phase, int horizontal_phase);
	void destroy_textures();
};

#endif
