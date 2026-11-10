#ifndef QTREGBPAINT_UNDO_HISTORY_H
#define QTREGBPAINT_UNDO_HISTORY_H

#include <cstddef>
#include <cstdint>
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

// Parameters needed to rebuild the post-resize state from the before
// snapshot. Resize is deterministic, so redo does not need to store the whole
// resulting image.
struct CanvasResize
{
	int width = 0;
	int height = 0;
	PaintCore::ColorIndex fill = 0;
};

struct UndoStep
{
	std::vector<PixelPatch> patches;

	// Snapshot steps change the canvas dimensions; they do not use patches.
	// Only the state before the resize is stored; redo rebuilds the result
	// with `canvas_resize`.
	std::optional<CanvasSnapshot> before_canvas;
	std::optional<CanvasResize> canvas_resize;

	// Identity of the state after this step. 0 is the initial state.
	std::uint64_t state_id = 0;

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

	// Identity of the current undo/redo state. 0 means "before the first
	// step". Saving records this value; undoing back to it makes the document
	// clean again.
	std::uint64_t current_state_id() const { return m_current_state_id; }

	void clear();
	void clear_redo();

	std::size_t bytes() const { return m_bytes; }
	std::size_t step_count() const { return m_undo.size() + m_redo.size(); }

private:
	std::vector<UndoStep> m_undo;
	std::vector<UndoStep> m_redo;
	std::size_t m_bytes = 0;
	HistoryLimits m_limits{};
	std::uint64_t m_next_state_id = 1;
	std::uint64_t m_current_state_id = 0;

	bool over_budget() const;
	void trim();
	void evict_oldest();
};

#endif
