#include "paint/undo_history.h"

#include <utility>

#include "paint/image_buffer.h"
#include "paint/image_transform.h"

namespace
{
void apply_patch(ImageBuffer& image, const PixelPatch& patch, bool use_after)
{
	const std::vector<PaintCore::ColorIndex>& pixels = use_after ? patch.after : patch.before;
	image.write_region(patch.rect, pixels.data());
}

bool snapshot_valid(const CanvasSnapshot& snapshot)
{
	if (snapshot.width <= 0 || snapshot.height <= 0 ||
	    snapshot.bits_per_pixel < 1 || snapshot.bits_per_pixel > 8)
	{
		return false;
	}
	const std::size_t expected =
		static_cast<std::size_t>(snapshot.width) * static_cast<std::size_t>(snapshot.height);
	return snapshot.pixels.size() == expected;
}

void restore_snapshot(ImageBuffer& image, const CanvasSnapshot& snapshot)
{
	image = ImageBuffer(snapshot.width, snapshot.height, snapshot.bits_per_pixel);
	if (!snapshot.pixels.empty())
	{
		image.write_region(
			PaintCore::Rect{0, 0, snapshot.width, snapshot.height},
			snapshot.pixels.data());
	}
	image.mark_all_dirty();
}
}

std::size_t UndoStep::bytes() const
{
	std::size_t total = 0;
	for (const PixelPatch& patch : patches)
	{
		total += (patch.before.size() + patch.after.size()) * sizeof(PaintCore::ColorIndex);
	}
	if (before_canvas)
	{
		total += before_canvas->bytes();
	}
	if (canvas_bits_per_pixel)
	{
		total += canvas_bits_per_pixel->mapping.size() *
		         sizeof(PaintCore::ColorIndex);
	}
	return total;
}

UndoHistory::UndoHistory(HistoryLimits limits) : m_limits(limits)
{
}

void UndoHistory::set_limits(HistoryLimits limits)
{
	m_limits = limits;

	// A zero limit disables the history.
	if (m_limits.max_steps == 0 || m_limits.max_bytes == 0)
	{
		clear();
		return;
	}

	trim();
}

void UndoHistory::push(UndoStep step)
{
	const bool has_before_canvas = step.before_canvas.has_value();
	const bool has_canvas_resize = step.canvas_resize.has_value();
	const bool has_canvas_bits_per_pixel =
		step.canvas_bits_per_pixel.has_value();
	const bool has_canvas_transform = step.canvas_transform.has_value();

	if (has_before_canvas || has_canvas_resize || has_canvas_bits_per_pixel)
	{
		if (!has_before_canvas || has_canvas_transform ||
		    !step.patches.empty() || !snapshot_valid(*step.before_canvas))
		{
			return;
		}

		// Exactly one deterministic rebuild operation may accompany the before
		// snapshot.
		if (has_canvas_resize == has_canvas_bits_per_pixel)
		{
			return;
		}

		if (has_canvas_resize &&
		    (step.canvas_resize->width < 1 ||
		     step.canvas_resize->height < 1))
		{
			return;
		}

		if (has_canvas_bits_per_pixel)
		{
			const CanvasBitsPerPixel& change = *step.canvas_bits_per_pixel;
			if (change.bits_per_pixel < 1 || change.bits_per_pixel > 8 ||
			    change.bits_per_pixel ==
				    step.before_canvas->bits_per_pixel)
			{
				return;
			}
			const std::size_t expected = static_cast<std::size_t>(
				1u << step.before_canvas->bits_per_pixel);
			if (change.mapping.size() != expected)
			{
				return;
			}
			const int new_count = 1 << change.bits_per_pixel;
			for (PaintCore::ColorIndex value : change.mapping)
			{
				if (static_cast<int>(value) >= new_count)
				{
					return;
				}
			}
		}
	}
	else if (has_canvas_transform)
	{
		if (!step.patches.empty() ||
		    !PaintCore::is_valid_image_transform(*step.canvas_transform))
		{
			return;
		}
	}
	else
	{
		if (step.patches.empty())
		{
			return;
		}

		// Reject malformed patches before they can corrupt the image.
		for (const PixelPatch& patch : step.patches)
		{
			const std::size_t expected =
				static_cast<std::size_t>(patch.rect.w) * static_cast<std::size_t>(patch.rect.h);
			if (patch.rect.empty() || patch.rect.x < 0 || patch.rect.y < 0 ||
				patch.before.size() != expected || patch.after.size() != expected)
			{
				return;
			}
		}
	}

	if (m_limits.max_steps == 0 || m_limits.max_bytes == 0)
	{
		clear();
		return;
	}

	// Any new edit invalidates the redo branch.
	clear_redo();

	step.state_id = m_next_state_id++;
	m_bytes += step.bytes();
	m_undo.push_back(std::move(step));
	m_current_state_id = m_undo.back().state_id;
	trim();
}

bool UndoHistory::undo(ImageBuffer& image)
{
	if (m_undo.empty())
	{
		return false;
	}

	UndoStep step = std::move(m_undo.back());
	m_undo.pop_back();

	if (step.before_canvas)
	{
		restore_snapshot(image, *step.before_canvas);
	}
	else if (step.canvas_transform)
	{
		image = PaintCore::transform_image(
			image,
			PaintCore::inverse_image_transform(*step.canvas_transform));
		image.mark_all_dirty();
	}
	else
	{
		// If patches overlap, restoring in reverse order leaves the image in
		// the correct previous state.
		for (auto it = step.patches.rbegin(); it != step.patches.rend(); ++it)
		{
			apply_patch(image, *it, false);
		}
	}

	m_redo.push_back(std::move(step));
	m_current_state_id = m_undo.empty() ? 0 : m_undo.back().state_id;
	return true;
}

bool UndoHistory::redo(ImageBuffer& image)
{
	if (m_redo.empty())
	{
		return false;
	}

	UndoStep step = std::move(m_redo.back());
	m_redo.pop_back();

	if (step.before_canvas)
	{
		if (step.canvas_resize)
		{
			// Undo restored the before state, so applying the same resize
			// deterministically rebuilds the after state.
			image.resize(step.canvas_resize->width,
			             step.canvas_resize->height,
			             step.canvas_resize->fill);
		}
		else if (step.canvas_bits_per_pixel)
		{
			restore_snapshot(image, *step.before_canvas);
			image.remap_bits_per_pixel(
				step.canvas_bits_per_pixel->bits_per_pixel,
				step.canvas_bits_per_pixel->mapping);
		}
		image.mark_all_dirty();
	}
	else if (step.canvas_transform)
	{
		image = PaintCore::transform_image(image, *step.canvas_transform);
		image.mark_all_dirty();
	}
	else
	{
		for (const PixelPatch& patch : step.patches)
		{
			apply_patch(image, patch, true);
		}
	}

	m_undo.push_back(std::move(step));
	m_current_state_id = m_undo.back().state_id;
	return true;
}

void UndoHistory::clear()
{
	m_undo.clear();
	m_redo.clear();
	m_bytes = 0;
	// m_next_state_id stays monotonic: an old saved id must never be reused
	// after clearing the history.
	m_current_state_id = 0;
}

void UndoHistory::clear_redo()
{
	for (const UndoStep& step : m_redo)
	{
		m_bytes -= step.bytes();
	}
	m_redo.clear();
}

bool UndoHistory::over_budget() const
{
	return m_bytes > m_limits.max_bytes || step_count() > m_limits.max_steps;
}

void UndoHistory::evict_oldest()
{
	if (!m_undo.empty())
	{
		m_bytes -= m_undo.front().bytes();
		m_undo.erase(m_undo.begin());
	}
	else if (!m_redo.empty())
	{
		m_bytes -= m_redo.front().bytes();
		m_redo.erase(m_redo.begin());
	}
}

void UndoHistory::trim()
{
	// The most recent step is kept even if it alone exceeds the budget, so the
	// last action can always be undone (soft limit).
	while (over_budget() && step_count() > 1)
	{
		evict_oldest();
	}
}
