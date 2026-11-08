#include "paint/floating_region_editor.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "paint/canvas_document.h"

using namespace PaintCore;

namespace
{
PaintCore::Rect intersect(const PaintCore::Rect& a, const PaintCore::Rect& b)
{
	if (a.empty() || b.empty())
	{
		return {};
	}

	const int x0 = std::max(a.x, b.x);
	const int y0 = std::max(a.y, b.y);
	const int x1 = std::min(a.right(), b.right());
	const int y1 = std::min(a.bottom(), b.bottom());
	if (x0 >= x1 || y0 >= y1)
	{
		return {};
	}
	return {x0, y0, x1 - x0, y1 - y0};
}

bool image_contains_color(const ImageBuffer& image, PaintCore::ColorIndex color)
{
	for (int y = 0; y < image.height(); ++y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			if (image.get_pixel(x, y) == color)
			{
				return true;
			}
		}
	}
	return false;
}
}

FloatingRegionEditor::FloatingRegionEditor(CanvasDocument& document):
m_document(document)
{
}

bool FloatingRegionEditor::begin_selection(const PaintCore::Rect& rect,
                                           PaintCore::ColorIndex hole_fill,
                                           bool transparent_background)
{
	ImageBuffer& image = m_document.get_main_buffer();
	const PaintCore::Rect canvas_rect{
		0, 0, image.width(), image.height()};
	const PaintCore::Rect clipped = intersect(rect, canvas_rect);
	if (clipped.empty())
	{
		return false;
	}

	if (m_floating.active)
	{
		commit_floating_region();
	}

	m_floating.content = image.sub_buffer(
		clipped.x, clipped.y, clipped.w, clipped.h);
	m_floating.rect = clipped;
	m_floating.hole = clipped;
	m_floating.hole_fill = hole_fill;
	m_floating.active = true;
	m_floating.transparent_background = transparent_background;
	m_floating.transparent_color = hole_fill;
	m_floating.modified =
		transparent_background &&
		image_contains_color(m_floating.content, hole_fill);
	++m_floating.content_revision;
	return true;
}

bool FloatingRegionEditor::begin_paste(ImageBuffer content,
                                       const PaintCore::Vec2& pos)
{
	if (content.width() <= 0 || content.height() <= 0)
	{
		return false;
	}

	if (m_floating.active)
	{
		commit_floating_region();
	}

	// The floating buffer always uses the canvas format. Pixels are copied by
	// value, so a pasted image with a different bpp still works.
	m_floating.content = ImageBuffer(
		content.width(), content.height(),
		m_document.get_main_buffer().bitsPerPixel());
	for (int y = 0; y < content.height(); ++y)
	{
		for (int x = 0; x < content.width(); ++x)
		{
			m_floating.content.set_pixel(
				x, y, content.get_pixel(x, y));
		}
	}

	m_floating.rect = {pos.x, pos.y, content.width(), content.height()};
	m_floating.hole.reset();
	m_floating.hole_fill = 0;
	m_floating.active = true;
	m_floating.transparent_background = false;
	m_floating.transparent_color = 0;
	m_floating.modified = true;
	++m_floating.content_revision;
	return true;
}

bool FloatingRegionEditor::move_floating_region(int dx, int dy)
{
	if (!m_floating.active || m_floating.rect.empty() ||
	    m_document.getWidth() <= 0 || m_document.getHeight() <= 0)
	{
		return false;
	}

	// Keep at least one pixel of the region inside the canvas.
	const int min_x = -(m_floating.rect.w - 1);
	const int max_x = m_document.getWidth() - 1;
	const int min_y = -(m_floating.rect.h - 1);
	const int max_y = m_document.getHeight() - 1;

	const int new_x = std::clamp(m_floating.rect.x + dx, min_x, max_x);
	const int new_y = std::clamp(m_floating.rect.y + dy, min_y, max_y);
	if (new_x == m_floating.rect.x && new_y == m_floating.rect.y)
	{
		return false;
	}

	m_floating.rect.x = new_x;
	m_floating.rect.y = new_y;
	m_floating.modified = true;
	return true;
}

bool FloatingRegionEditor::set_floating_transparency(
	bool transparent_background, PaintCore::ColorIndex color)
{
	if (!m_floating.active)
	{
		return false;
	}

	const bool changed =
		m_floating.transparent_background != transparent_background ||
		m_floating.transparent_color != color;
	if (!changed)
	{
		return false;
	}

	m_floating.transparent_background = transparent_background;
	m_floating.transparent_color = color;

	if (transparent_background &&
	    image_contains_color(m_floating.content, color))
	{
		m_floating.modified = true;
	}

	++m_floating.content_revision;
	return true;
}

bool FloatingRegionEditor::commit_floating_region()
{
	if (!m_floating.active)
	{
		return false;
	}

	// A fresh selection that has not moved or been edited is not a change.
	if (!m_floating.modified)
	{
		cancel_floating_region();
		return false;
	}

	ImageBuffer& image = m_document.get_main_buffer();
	const PaintCore::Rect canvas_rect{
		0, 0, image.width(), image.height()};
	const PaintCore::Rect content_rect =
		intersect(m_floating.rect, canvas_rect);
	const PaintCore::Rect hole_rect =
		intersect(m_floating.hole, canvas_rect);

	PaintCore::Rect bounds = hole_rect;
	bounds.unite(content_rect);
	if (bounds.empty())
	{
		cancel_floating_region();
		return false;
	}

	std::vector<PaintCore::ColorIndex> before =
		image.read_region(bounds);

	if (!hole_rect.empty())
	{
		std::vector<PaintCore::ColorIndex> fill(
			static_cast<std::size_t>(hole_rect.w) *
			static_cast<std::size_t>(hole_rect.h),
			m_floating.hole_fill);
		image.write_region(hole_rect, fill.data());
	}

	if (!content_rect.empty())
	{
		const int offset_x = content_rect.x - m_floating.rect.x;
		const int offset_y = content_rect.y - m_floating.rect.y;
		for (int y = 0; y < content_rect.h; ++y)
		{
			for (int x = 0; x < content_rect.w; ++x)
			{
				const PaintCore::ColorIndex color =
					m_floating.content.get_pixel(
						offset_x + x, offset_y + y);
				const bool transparent =
					color == PaintCore::TRANSPARENT_INDEX ||
					(m_floating.transparent_background &&
					 color == m_floating.transparent_color);
				if (transparent)
				{
					continue;
				}
				image.set_pixel(
					content_rect.x + x, content_rect.y + y, color);
			}
		}
	}

	std::vector<PaintCore::ColorIndex> after =
		image.read_region(bounds);

	cancel_floating_region();
	return m_document.push_pixel_patch(
		bounds, std::move(before), std::move(after));
}

void FloatingRegionEditor::cancel_floating_region()
{
	if (m_floating.active)
	{
		m_floating.reset();
	}
}

bool FloatingRegionEditor::floating_region_contains(
	const PaintCore::Vec2& pos) const
{
	return m_floating.contains(pos.x, pos.y);
}

bool FloatingRegionEditor::copy_to_clipboard()
{
	if (!m_floating.active || m_floating.content.width() <= 0 ||
	    m_floating.content.height() <= 0)
	{
		return false;
	}

	// Copying does not confirm or modify the floating region and keeps raw
	// pixels; the transparency mask is applied later when drawing/pasting.
	m_clipboard = m_floating.content;
	return true;
}

bool FloatingRegionEditor::cut_to_clipboard()
{
	if (!copy_to_clipboard())
	{
		return false;
	}

	// For a selection, cut means "remove the selected pixels and leave the
	// background". Paste regions have no hole, so cutting one just discards
	// the floating copy.
	ImageBuffer& image = m_document.get_main_buffer();
	const PaintCore::Rect canvas_rect{
		0, 0, image.width(), image.height()};
	const PaintCore::Rect hole_rect =
		intersect(m_floating.hole, canvas_rect);
	if (!hole_rect.empty())
	{
		std::vector<PaintCore::ColorIndex> before =
			image.read_region(hole_rect);
		std::vector<PaintCore::ColorIndex> fill(
			static_cast<std::size_t>(hole_rect.w) *
			static_cast<std::size_t>(hole_rect.h),
			m_floating.hole_fill);
		image.write_region(hole_rect, fill.data());

		std::vector<PaintCore::ColorIndex> after =
			image.read_region(hole_rect);
		m_document.push_pixel_patch(
			hole_rect, std::move(before), std::move(after));
	}

	cancel_floating_region();
	return true;
}

bool FloatingRegionEditor::paste_from_clipboard(const PaintCore::Vec2& pos)
{
	if (!has_clipboard())
	{
		return false;
	}

	// begin_paste commits the current floating region, if any.
	return begin_paste(m_clipboard, pos);
}

bool FloatingRegionEditor::invert_floating_region_colors()
{
	if (!m_floating.active || m_floating.content.width() <= 0 ||
	    m_floating.content.height() <= 0)
	{
		return false;
	}

	// For 2bpp the four available indices are 0..3, so inversion is 3 - i.
	// TRANSPARENT_INDEX stays transparent (paste content may contain it).
	const int bpp = m_floating.content.bitsPerPixel();
	const int max_index = std::min(
		(1 << bpp) - 1,
		static_cast<int>(PaintCore::TRANSPARENT_INDEX) - 1);

	bool changed = false;
	for (int y = 0; y < m_floating.content.height(); ++y)
	{
		for (int x = 0; x < m_floating.content.width(); ++x)
		{
			const PaintCore::ColorIndex index =
				m_floating.content.get_pixel(x, y);
			if (index == PaintCore::TRANSPARENT_INDEX ||
			    static_cast<int>(index) > max_index)
			{
				continue;
			}

			const PaintCore::ColorIndex inverted =
				static_cast<PaintCore::ColorIndex>(max_index - index);
			if (inverted == index)
			{
				continue;
			}

			m_floating.content.set_pixel(x, y, inverted);
			changed = true;
		}
	}

	if (changed)
	{
		m_floating.modified = true;
		++m_floating.content_revision;
	}
	return changed;
}
