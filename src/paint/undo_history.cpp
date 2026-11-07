#include "paint/undo_history.h"

#include <utility>

#include "paint/image_buffer.h"

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
	if (after_canvas)
	{
		total += after_canvas->bytes();
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
	const bool has_after_canvas = step.after_canvas.has_value();
	if (has_before_canvas != has_after_canvas)
	{
		return;
	}

	if (has_before_canvas)
	{
		if (!step.patches.empty() ||
		    !snapshot_valid(*step.before_canvas) ||
		    !snapshot_valid(*step.after_canvas))
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

	m_bytes += step.bytes();
	m_undo.push_back(std::move(step));
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

	if (step.after_canvas)
	{
		restore_snapshot(image, *step.after_canvas);
	}
	else
	{
		for (const PixelPatch& patch : step.patches)
		{
			apply_patch(image, patch, true);
		}
	}

	m_undo.push_back(std::move(step));
	return true;
}

void UndoHistory::clear()
{
	m_undo.clear();
	m_redo.clear();
	m_bytes = 0;
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
