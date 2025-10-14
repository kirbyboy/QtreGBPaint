#ifndef QTREGBPAINT_PAINT_FRONTEND_H
#define QTREGBPAINT_PAINT_FRONTEND_H

#include "imgui.h"

class PaintFrontend
{
	public:
		PaintFrontend();
		void render(); // Main render method

	private:
		void renderMenuBar();
		void renderToolbar();
		void renderPalette();
		void renderCanvas();

	private:
		ImVec2 canvasSize = ImVec2(2000, 2000); // Larger than window to show scroll
};

#endif