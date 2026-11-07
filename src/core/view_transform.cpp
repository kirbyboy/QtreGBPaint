#include "core/view_transform.h"

#include <algorithm>
#include <cmath>

using namespace PaintCore;

ViewTransform ViewTransform::make(const Vec2f& canvas_pos,
								  const Vec2f& scroll_px,
								  const Vec2f& viewport_size,
								  float zoom)
{
	ViewTransform transform;
	transform.scroll_px = scroll_px;
	transform.viewport_size = viewport_size;
	transform.zoom = (zoom > 0.0f) ? zoom : 1.0f;

	// Align the origin to whole pixels and keep only the fractional part of
	// the scroll, which is what ImGui does not round.
	const float snapped_x = std::floor(scroll_px.x / transform.zoom) * transform.zoom;
	const float snapped_y = std::floor(scroll_px.y / transform.zoom) * transform.zoom;

	transform.origin.x = std::floor(canvas_pos.x + (scroll_px.x - snapped_x) + 0.5f);
	transform.origin.y = std::floor(canvas_pos.y + (scroll_px.y - snapped_y) + 0.5f);

	return transform;
}

Vec2f ViewTransform::image_to_screen(const Vec2f& image) const
{
	return Vec2f{
		origin.x + image.x * zoom,
		origin.y + image.y * zoom,
	};
}

Vec2f ViewTransform::screen_to_image(const Vec2f& screen) const
{
	return Vec2f{
		(screen.x - origin.x) / zoom,
		(screen.y - origin.y) / zoom,
	};
}

Vec2 ViewTransform::screen_to_image_pixel(const Vec2f& screen) const
{
	const Vec2f image = screen_to_image(screen);
	return Vec2{
		static_cast<int>(std::floor(image.x)),
		static_cast<int>(std::floor(image.y)),
	};
}

Rect ViewTransform::visible_image_rect(int image_width, int image_height) const
{
	if (image_width <= 0 || image_height <= 0 || zoom <= 0.0f)
	{
		return {};
	}

	int start_x = static_cast<int>(std::floor(scroll_px.x / zoom));
	int start_y = static_cast<int>(std::floor(scroll_px.y / zoom));
	const int visible_w = static_cast<int>(std::ceil(viewport_size.x / zoom));
	const int visible_h = static_cast<int>(std::ceil(viewport_size.y / zoom));

	start_x = std::clamp(start_x, 0, image_width);
	start_y = std::clamp(start_y, 0, image_height);

	const int end_x = std::min(image_width, start_x + visible_w);
	const int end_y = std::min(image_height, start_y + visible_h);

	if (start_x >= end_x || start_y >= end_y)
	{
		return {};
	}

	return Rect{start_x, start_y, end_x - start_x, end_y - start_y};
}
