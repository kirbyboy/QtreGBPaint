#ifndef QTREGBPAINT_PAINT_FRONTEND_H
#define QTREGBPAINT_PAINT_FRONTEND_H

#include "imgui.h"
#include "paint/canvas_area.h"
#include "paint/palette.h"
#include "paint/paint_backend.h"

class PaintFrontend
{
	public:
		PaintFrontend(PaintBackend& backend);
		void select_tool(const std::string& name);
		void render(); // Main render method

	private:
		void renderMenuBar();
		void renderToolbar();
		void renderPalette();
		void renderCanvas();

		

	private:
		PaintBackend& m_backend;
		CanvasArea m_canvas_area;
		int m_zoom = 1;
		Palette m_palette;
};

#endif