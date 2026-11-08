#ifndef QTREGBPAINT_FLOATING_REGION_H
#define QTREGBPAINT_FLOATING_REGION_H

#include <cstdint>

#include "core/color.h"
#include "core/rect.h"
#include "paint/image_buffer.h"

// Pixels that can be moved over the canvas without being part of it yet.
// Selection and paste share this model: the tool only creates the marquee,
// while the floating region editor owns the movable content and lifecycle.
//
// A selection leaves a `hole` at its original position; that area is filled
// with `hole_fill` when the region is committed. Paste regions have no hole.
struct FloatingRegion
{
	ImageBuffer content;
	PaintCore::Rect rect{};
	PaintCore::Rect hole{};
	PaintCore::ColorIndex hole_fill = 0;
	bool active = false;
	// Live transparency mask: when enabled, content pixels equal to
	// `transparent_color` are drawn and committed as transparent without
	// modifying `content` itself.
	bool transparent_background = false;
	PaintCore::ColorIndex transparent_color = 0;
	// True once the content or its position differs from the original
	// selection. Inverting colors or enabling a matching transparency mask
	// without moving the rect must count too.
	bool modified = false;
	std::uint64_t content_revision = 0;

	bool contains(int x, int y) const
	{
		return active && !rect.empty() &&
		       x >= rect.x && y >= rect.y &&
		       x < rect.right() && y < rect.bottom();
	}

	void reset()
	{
		content = ImageBuffer();
		rect.reset();
		hole.reset();
		hole_fill = 0;
		active = false;
		transparent_background = false;
		transparent_color = 0;
		modified = false;
		++content_revision;
	}
};

#endif
