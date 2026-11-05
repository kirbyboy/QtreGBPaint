#ifndef QTREGBPAINT_VIEW_TRANSFORM_H
#define QTREGBPAINT_VIEW_TRANSFORM_H

#include "core/rect.h"
#include "core/vec2.h"

namespace PaintCore
{
// Transformación única entre coordenadas de imagen y de pantalla para un
// frame. `origin` es la posición en pantalla del píxel de imagen (0,0);
// `scroll_px` y `viewport_size` describen la ventana visible.
struct ViewTransform
{
	Vec2f origin{};         // imagen (0,0) en píxeles de pantalla
	Vec2f scroll_px{};      // scroll crudo de ImGui, en píxeles de pantalla
	Vec2f viewport_size{};  // tamaño visible, en píxeles de pantalla
	float zoom = 1.0f;

	static ViewTransform make(const Vec2f& canvas_pos,
							  const Vec2f& scroll_px,
							  const Vec2f& viewport_size,
							  float zoom);

	Vec2f image_to_screen(const Vec2f& image) const;
	Vec2f screen_to_image(const Vec2f& screen) const;
	Vec2 screen_to_image_pixel(const Vec2f& screen) const;

	// Parte del canvas que hay que recorrer para cubrir la ventana visible.
	Rect visible_image_rect(int image_width, int image_height) const;
};
}

#endif
