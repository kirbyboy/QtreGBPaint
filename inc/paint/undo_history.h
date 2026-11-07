#ifndef QTREGBPAINT_UNDO_HISTORY_H
#define QTREGBPAINT_UNDO_HISTORY_H

#include <cstddef>
#include <optional>
#include <vector>

#include "core/color.h"
#include "core/rect.h"

class ImageBuffer;

struct HistoryLimits
{
	std::size_t max_bytes = 64u * 1024u * 1024u;
	std::size_t max_steps = 200;
};

// Changes in a rectangular region: pixels before and after the step.
struct PixelPatch
{
	PaintCore::Rect rect;
	std::vector<PaintCore::ColorIndex> before;
	std::vector<PaintCore::ColorIndex> after;
};

// Full image state, used by steps that change the canvas dimensions.
struct CanvasSnapshot
{
	int width = 0;
	int height = 0;
	int bits_per_pixel = 0;
	std::vector<PaintCore::ColorIndex> pixels;

	std::size_t bytes() const
	{
		return pixels.size() * sizeof(PaintCore::ColorIndex);
	}
};

struct UndoStep
{
	std::vector<PixelPatch> patches;

	// Snapshot steps change the canvas dimensions; they do not use patches.
	std::optional<CanvasSnapshot> before_canvas;
	std::optional<CanvasSnapshot> after_canvas;

	std::size_t bytes() const;
};

// Undo/redo stacks. Undone steps move to the redo stack: their memory is
// only reclaimed when that branch is discarded or the history is trimmed
// by the memory/step limits.
class UndoHistory
{
public:
	UndoHistory() = default;
	explicit UndoHistory(HistoryLimits limits);

	void set_limits(HistoryLimits limits);
	HistoryLimits limits() const { return m_limits; }

	void push(UndoStep step);
	bool undo(ImageBuffer& image);
	bool redo(ImageBuffer& image);

	bool can_undo() const { return !m_undo.empty(); }
	bool can_redo() const { return !m_redo.empty(); }

	void clear();
	void clear_redo();

	std::size_t bytes() const { return m_bytes; }
	std::size_t step_count() const { return m_undo.size() + m_redo.size(); }

private:
	std::vector<UndoStep> m_undo;
	std::vector<UndoStep> m_redo;
	std::size_t m_bytes = 0;
	HistoryLimits m_limits{};

	bool over_budget() const;
	void trim();
	void evict_oldest();
};

#endif
