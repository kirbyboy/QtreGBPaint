#ifndef QTREGBPAINT_FLOATING_REGION_H
#define QTREGBPAINT_FLOATING_REGION_H

#include <cstdint>
#include <utility>

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
	PaintCore::Rect rect{};
	PaintCore::Rect hole{};
	PaintCore::ColorIndex hole_fill = 0;
	bool active = false;
	// Live transparency mask: when enabled, content pixels equal to
	// `transparent_color` are drawn and committed as transparent without
	// modifying the content itself.
	bool transparent_background = false;
	PaintCore::ColorIndex transparent_color = 0;

	// Content is replace-only: every change bumps the revision, so the view
	// cannot accidentally keep a stale texture.
	const ImageBuffer& content() const { return m_content; }
	std::uint64_t revision() const { return m_revision; }

	void set_content(ImageBuffer content)
	{
		m_content = std::move(content);
		++m_revision;
	}

	// Changes that affect the floating preview without replacing its pixels
	// (e.g. toggling the transparency mask).
	void invalidate() { ++m_revision; }

	bool contains(int x, int y) const
	{
		return active && !rect.empty() &&
		       x >= rect.x && y >= rect.y &&
		       x < rect.right() && y < rect.bottom();
	}

	void reset()
	{
		set_content(ImageBuffer());
		rect.reset();
		hole.reset();
		hole_fill = 0;
		active = false;
		transparent_background = false;
		transparent_color = 0;
	}

private:
	ImageBuffer m_content;
	std::uint64_t m_revision = 0;
};

#endif
