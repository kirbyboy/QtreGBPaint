#ifndef QTREGBPAINT_VIEW_TRANSFORM_H
#define QTREGBPAINT_VIEW_TRANSFORM_H

#include "core/rect.h"
#include "core/vec2.h"

namespace PaintCore
{
// Single transformation between image and screen coordinates for one frame.
// `origin` is the screen position of image pixel (0,0); `scroll_px` and
// `viewport_size` describe the visible window.
struct ViewTransform
{
	Vec2f origin{};         // image (0,0) in screen pixels
	Vec2f scroll_px{};      // raw ImGui scroll, in screen pixels
	Vec2f viewport_size{};  // visible size, in screen pixels
	float zoom = 1.0f;

	static ViewTransform make(const Vec2f& canvas_pos,
							  const Vec2f& scroll_px,
							  const Vec2f& viewport_size,
							  float zoom);

	Vec2f image_to_screen(const Vec2f& image) const;
	Vec2f screen_to_image(const Vec2f& screen) const;
	Vec2 screen_to_image_pixel(const Vec2f& screen) const;

	// Part of the canvas that must be traversed to cover the visible window.
	Rect visible_image_rect(int image_width, int image_height) const;
};
}

#endif
