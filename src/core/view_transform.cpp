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

	// `canvas_pos` already includes the window scroll (it is what
	// GetCursorScreenPos returns), so the image follows the scroll 1:1 and the
	// top/left edge can show a partially clipped pixel. Keep only the origin
	// aligned to whole screen pixels so zoomed pixels stay crisp.
	transform.origin.x = std::floor(canvas_pos.x + 0.5f);
	transform.origin.y = std::floor(canvas_pos.y + 0.5f);

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

Vec2f ViewTransform::scroll_for_zoom_anchor(const Vec2f& screen_anchor,
                                            float new_zoom) const
{
	if (zoom <= 0.0f || new_zoom <= 0.0f)
	{
		return scroll_px;
	}

	// origin = content_start - scroll, so keeping the same image point under
	// the anchor means:
	//   scroll' = scroll + (anchor - origin) * (new_zoom / zoom - 1)
	const float ratio = new_zoom / zoom;
	return Vec2f{
		scroll_px.x + (screen_anchor.x - origin.x) * (ratio - 1.0f),
		scroll_px.y + (screen_anchor.y - origin.y) * (ratio - 1.0f),
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
