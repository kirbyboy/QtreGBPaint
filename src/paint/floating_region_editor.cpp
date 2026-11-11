#include "paint/floating_region_editor.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "paint/canvas_document.h"
#include "paint/image_transform.h"

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
}

FloatingRegionEditor::FloatingRegionEditor(CanvasDocument& document):
m_document(document)
{
}

bool FloatingRegionEditor::begin_selection(const PaintCore::Rect& rect,
                                           PaintCore::ColorIndex hole_fill,
                                           bool transparent_background)
{
	const PaintCore::Rect canvas_rect{
		0, 0, m_document.getWidth(), m_document.getHeight()};
	const PaintCore::Rect clipped = intersect(rect, canvas_rect);
	if (clipped.empty())
	{
		return false;
	}

	if (m_floating.active)
	{
		commit_floating_region();
	}

	const ImageBuffer& image = m_document.get_main_buffer();
	m_floating.set_content(image.sub_buffer(
		clipped.x, clipped.y, clipped.w, clipped.h));
	m_floating.rect = clipped;
	m_floating.hole = clipped;
	m_floating.hole_fill = hole_fill;
	m_floating.active = true;
	m_floating.transparent_background = transparent_background;
	m_floating.transparent_color = hole_fill;
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
	ImageBuffer content_copy(
		content.width(), content.height(),
		m_document.get_main_buffer().bitsPerPixel());
	for (int y = 0; y < content.height(); ++y)
	{
		for (int x = 0; x < content.width(); ++x)
		{
			content_copy.set_pixel(
				x, y, content.get_pixel(x, y));
		}
	}
	m_floating.set_content(std::move(content_copy));

	m_floating.rect = {pos.x, pos.y, content.width(), content.height()};
	m_floating.hole.reset();
	m_floating.hole_fill = 0;
	m_floating.active = true;
	m_floating.transparent_background = false;
	m_floating.transparent_color = 0;
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
	m_floating.invalidate();
	return true;
}

bool FloatingRegionEditor::commit_floating_region()
{
	if (!m_floating.active)
	{
		return false;
	}

	const ImageBuffer& image = m_document.get_main_buffer();
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

	// Build the final pixels without touching the document. The commit is a
	// single apply_pixel_patch, which also owns the before/after history.
	std::vector<PaintCore::ColorIndex> after = image.read_region(bounds);

	if (!hole_rect.empty())
	{
		for (int y = hole_rect.y; y < hole_rect.bottom(); ++y)
		{
			const std::size_t row =
				static_cast<std::size_t>(y - bounds.y) *
				static_cast<std::size_t>(bounds.w);
			for (int x = hole_rect.x; x < hole_rect.right(); ++x)
			{
				after[row + static_cast<std::size_t>(x - bounds.x)] =
					m_floating.hole_fill;
			}
		}
	}

	if (!content_rect.empty())
	{
		const int offset_x = content_rect.x - m_floating.rect.x;
		const int offset_y = content_rect.y - m_floating.rect.y;
		const ImageBuffer& content = m_floating.content();
		for (int y = 0; y < content_rect.h; ++y)
		{
			const std::size_t row =
				static_cast<std::size_t>(content_rect.y + y - bounds.y) *
				static_cast<std::size_t>(bounds.w);
			for (int x = 0; x < content_rect.w; ++x)
			{
				const PaintCore::ColorIndex color =
					content.get_pixel(offset_x + x, offset_y + y);
				const bool transparent =
					m_floating.transparent_background &&
					color == m_floating.transparent_color;
				if (transparent)
				{
					continue;
				}
				after[row + static_cast<std::size_t>(
					content_rect.x + x - bounds.x)] = color;
			}
		}
	}

	const bool changed =
		m_document.apply_pixel_patch(bounds, std::move(after));
	cancel_floating_region();
	return changed;
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
	const ImageBuffer& content = m_floating.content();
	if (!m_floating.active || content.width() <= 0 ||
	    content.height() <= 0)
	{
		return false;
	}

	// Copying does not confirm or modify the floating region and keeps raw
	// pixels; the transparency mask is applied later when drawing/pasting.
	m_clipboard = content;
	return true;
}

bool FloatingRegionEditor::cut_to_clipboard()
{
	if (!copy_to_clipboard())
	{
		return false;
	}

	remove_floating_region();
	return true;
}

bool FloatingRegionEditor::delete_floating_region()
{
	if (!m_floating.active)
	{
		return false;
	}

	remove_floating_region();
	return true;
}

void FloatingRegionEditor::remove_floating_region()
{
	// For a selection, delete means "remove the selected pixels and leave the
	// background". Paste regions have no hole, so deleting one just discards
	// the floating copy.
	const ImageBuffer& image = m_document.get_main_buffer();
	const PaintCore::Rect canvas_rect{
		0, 0, image.width(), image.height()};
	const PaintCore::Rect hole_rect =
		intersect(m_floating.hole, canvas_rect);
	if (!hole_rect.empty())
	{
		std::vector<PaintCore::ColorIndex> fill(
			static_cast<std::size_t>(hole_rect.w) *
			static_cast<std::size_t>(hole_rect.h),
			m_floating.hole_fill);
		m_document.apply_pixel_patch(hole_rect, std::move(fill));
	}

	cancel_floating_region();
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

bool FloatingRegionEditor::transform_floating_region(
	PaintCore::ImageTransform transform)
{
	const ImageBuffer& content = m_floating.content();
	if (!m_floating.active || content.width() <= 0 ||
	    content.height() <= 0)
	{
		return false;
	}

	ImageBuffer transformed = PaintCore::transform_image(content, transform);
	const bool swap_axes = PaintCore::image_transform_swaps_axes(transform);
	if (swap_axes)
	{
		const int new_width = transformed.width();
		const int new_height = transformed.height();

		// Keep the rotated region centered on the old one. Rect coordinates
		// are integers, so odd/even size mixes may shift by half a pixel.
		int new_x = m_floating.rect.x +
			(m_floating.rect.w - new_width) / 2;
		int new_y = m_floating.rect.y +
			(m_floating.rect.h - new_height) / 2;

		// As with moving, keep at least one pixel of the region on canvas.
		new_x = std::clamp(new_x, -(new_width - 1),
		                   m_document.getWidth() - 1);
		new_y = std::clamp(new_y, -(new_height - 1),
		                   m_document.getHeight() - 1);

		m_floating.rect.x = new_x;
		m_floating.rect.y = new_y;
		m_floating.rect.w = new_width;
		m_floating.rect.h = new_height;
	}

	// Replace-only content keeps the view texture in sync; the selection is
	// still active and is neither committed nor cancelled here.
	m_floating.set_content(std::move(transformed));
	return true;
}

bool FloatingRegionEditor::invert_floating_region_colors()
{
	const ImageBuffer& content = m_floating.content();
	if (!m_floating.active || content.width() <= 0 ||
	    content.height() <= 0)
	{
		return false;
	}

	// For 2bpp the four available indices are 0..3, so inversion is 3 - i.
	// Every palette index is a regular value now; transparency is a palette
	// property, not a pixel sentinel.
	const int bpp = content.bitsPerPixel();
	const int max_index = (1 << bpp) - 1;

	ImageBuffer inverted = content;
	bool changed = false;
	for (int y = 0; y < inverted.height(); ++y)
	{
		for (int x = 0; x < inverted.width(); ++x)
		{
			const PaintCore::ColorIndex index =
				inverted.get_pixel(x, y);
			if (static_cast<int>(index) > max_index)
			{
				continue;
			}

			const PaintCore::ColorIndex new_index =
				static_cast<PaintCore::ColorIndex>(max_index - index);
			if (new_index == index)
			{
				continue;
			}

			inverted.set_pixel(x, y, new_index);
			changed = true;
		}
	}

	if (!changed)
	{
		return false;
	}

	// Replace-only content keeps the view texture in sync.
	m_floating.set_content(std::move(inverted));
	return true;
}
