#ifndef QTREGBPAINT_UNDO_HISTORY_H
#define QTREGBPAINT_UNDO_HISTORY_H

#include <cstddef>
#include <vector>

#include "core/color.h"
#include "core/rect.h"

class ImageBuffer;

struct HistoryLimits
{
	std::size_t max_bytes = 64u * 1024u * 1024u;
	std::size_t max_steps = 200;
};

// Cambios de una región rectangular: píxeles antes y después del paso.
struct PixelPatch
{
	PaintCore::Rect rect;
	std::vector<PaintCore::ColorIndex> before;
	std::vector<PaintCore::ColorIndex> after;
};

struct UndoStep
{
	std::vector<PixelPatch> patches;

	std::size_t bytes() const;
};

// Pilas undo/redo. Los pasos deshechos se mueven a la pila de redo:
// su memoria solo se recicla cuando se descarta esa rama o se recorta
// el historial por límite de memoria/pasos.
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
