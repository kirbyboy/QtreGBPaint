#include "paint/canvas_document.h"

#include <algorithm>
#include <utility>

#include "paint/canvas_renderer.h"
#include "paint/draw_buffer.h"

using namespace PaintCore;
using namespace std;

namespace
{
CanvasSnapshot make_canvas_snapshot(const ImageBuffer& image)
{
	CanvasSnapshot snapshot;
	snapshot.width = image.width();
	snapshot.height = image.height();
	snapshot.bits_per_pixel = image.bitsPerPixel();
	snapshot.pixels = image.data();
	return snapshot;
}
}

CanvasDocument::CanvasDocument(int w, int h, HistoryLimits history_limits):
m_image_buffer(w, h, 2),
m_history(history_limits)
{
	m_image_buffer.fill(0);
}

int CanvasDocument::getWidth() const
{
	return m_image_buffer.width();
}

int CanvasDocument::getHeight() const
{
	return m_image_buffer.height();
}

int CanvasDocument::getBitsPerPixel() const
{
	return m_image_buffer.bitsPerPixel();
}

void CanvasDocument::render(ICanvasRenderer& renderer, const ViewTransform& view,
                            const Palette& palette) const
{
	render_image_buffer(renderer, view, palette);
}

void CanvasDocument::render_image_buffer(
	ICanvasRenderer& renderer, const ViewTransform& view,
	const Palette& palette) const
{
	const Rect visible = view.visible_image_rect(
		m_image_buffer.width(), m_image_buffer.height());

	for (int y = visible.y; y < visible.bottom(); ++y)
	{
		for (int x = visible.x; x < visible.right(); ++x)
		{
			ColorIndex c = m_image_buffer.data()[
				static_cast<std::size_t>(y) * static_cast<std::size_t>(m_image_buffer.width()) +
				static_cast<std::size_t>(x)];
			if (c == PaintCore::TRANSPARENT_INDEX)
			{
				continue;
			}
			// IMAGE coordinates; the renderer applies the transformation.
			renderer.draw_rect_filled(x, y, 1, 1, palette.getColor(c));
		}
	}
}

void CanvasDocument::commit_draw_buffer(DrawBuffer& buffer)
{
	if (!buffer.is_active())
	{
		buffer.reset();
		return;
	}

	const int x0 = std::max(0, buffer.x());
	const int y0 = std::max(0, buffer.y());
	const int x1 = std::min(m_image_buffer.width(), buffer.x() + buffer.width());
	const int y1 = std::min(m_image_buffer.height(), buffer.y() + buffer.height());

	if (x0 >= x1 || y0 >= y1)
	{
		buffer.reset();
		return;
	}

	const PaintCore::Rect rect{x0, y0, x1 - x0, y1 - y0};
	std::vector<PaintCore::ColorIndex> before = m_image_buffer.read_region(rect);

	buffer.blend_onto(m_image_buffer);
	buffer.reset();

	std::vector<PaintCore::ColorIndex> after = m_image_buffer.read_region(rect);
	record_patch(rect, std::move(before), std::move(after));
}

bool CanvasDocument::apply_pixel_patch(
	const PaintCore::Rect& rect,
	std::vector<PaintCore::ColorIndex> after)
{
	if (rect.empty() || rect.x < 0 || rect.y < 0 ||
	    rect.right() > m_image_buffer.width() ||
	    rect.bottom() > m_image_buffer.height() ||
	    after.size() != static_cast<std::size_t>(rect.w) *
	                    static_cast<std::size_t>(rect.h))
	{
		return false;
	}

	std::vector<PaintCore::ColorIndex> before =
		m_image_buffer.read_region(rect);
	if (before == after)
	{
		return false;
	}

	// Push before writing: if the history allocation fails, the image is
	// still untouched and the caller can retry.
	record_patch(rect, std::move(before), after);
	m_image_buffer.write_region(rect, after.data());
	return true;
}

bool CanvasDocument::record_patch(
	const PaintCore::Rect& rect,
	std::vector<PaintCore::ColorIndex> before,
	std::vector<PaintCore::ColorIndex> after)
{
	if (rect.empty() || before == after)
	{
		return false;
	}

	UndoStep step;
	step.patches.push_back(PixelPatch{
		rect, std::move(before), std::move(after)});
	m_history.push(std::move(step));
	return true;
}

bool CanvasDocument::can_undo() const
{
	return m_history.can_undo();
}

bool CanvasDocument::can_redo() const
{
	return m_history.can_redo();
}

bool CanvasDocument::undo()
{
	return m_history.undo(m_image_buffer);
}

bool CanvasDocument::redo()
{
	return m_history.redo(m_image_buffer);
}

void CanvasDocument::clear_history()
{
	m_history.clear();
}

std::uint64_t CanvasDocument::current_state_id() const
{
	return m_history.current_state_id();
}

void CanvasDocument::replace_image(ImageBuffer image)
{
	m_image_buffer = std::move(image);
	m_image_buffer.mark_all_dirty();
	m_history.clear();
}

bool CanvasDocument::resize(int new_width, int new_height,
                            PaintCore::ColorIndex fill)
{
	if (new_width < 1 || new_height < 1)
	{
		return false;
	}
	if (new_width == m_image_buffer.width() &&
	    new_height == m_image_buffer.height())
	{
		return false;
	}

	CanvasSnapshot before = make_canvas_snapshot(m_image_buffer);
	m_image_buffer.resize(new_width, new_height, fill);

	UndoStep step;
	step.before_canvas = std::move(before);
	step.canvas_resize = CanvasResize{new_width, new_height, fill};
	m_history.push(std::move(step));
	return true;
}

void CanvasDocument::set_history_limits(HistoryLimits limits)
{
	m_history.set_limits(limits);
}

HistoryLimits CanvasDocument::history_limits() const
{
	return m_history.limits();
}

const ImageBuffer& CanvasDocument::get_main_buffer() const
{
	return m_image_buffer;
}

PaintCore::Rect CanvasDocument::take_dirty_rect()
{
	const PaintCore::Rect dirty = m_image_buffer.dirty_rect();
	m_image_buffer.clear_dirty_rect();
	return dirty;
}
