#ifndef QTREGBPAINT_VIEWPORT_H
#define QTREGBPAINT_VIEWPORT_H

namespace PaintCore
{
	struct Viewport
	{
		// Scroll en coordenadas de imagen (no píxeles de pantalla)
		float scroll_x = 0.0f;
		float scroll_y = 0.0f;
		float zoom = 1.0f;

		int canvas_width_px = 0;   // tamaño visible en píxeles
		int canvas_height_px = 0;

		Viewport() = default;
		Viewport(float sx, float sy, float z, int w, int h)
			: scroll_x(sx), scroll_y(sy), zoom(z), canvas_width_px(w), canvas_height_px(h) {}
	};
}

#endif
