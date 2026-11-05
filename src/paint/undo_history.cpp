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
}

std::size_t UndoStep::bytes() const
{
	std::size_t total = 0;
	for (const PixelPatch& patch : patches)
	{
		total += (patch.before.size() + patch.after.size()) * sizeof(PaintCore::ColorIndex);
	}
	return total;
}

UndoHistory::UndoHistory(HistoryLimits limits) : m_limits(limits)
{
}

void UndoHistory::set_limits(HistoryLimits limits)
{
	m_limits = limits;

	// Un límite a cero desactiva el historial.
	if (m_limits.max_steps == 0 || m_limits.max_bytes == 0)
	{
		clear();
		return;
	}

	trim();
}

void UndoHistory::push(UndoStep step)
{
	if (step.patches.empty())
	{
		return;
	}

	// Rechazar parches mal formados antes de que puedan corromper la imagen.
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

	if (m_limits.max_steps == 0 || m_limits.max_bytes == 0)
	{
		clear();
		return;
	}

	// Cualquier edición nueva invalida la rama de redo.
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

	// Si los parches se solapan, restaurar en orden inverso deja la imagen
	// en el estado anterior correcto.
	for (auto it = step.patches.rbegin(); it != step.patches.rend(); ++it)
	{
		apply_patch(image, *it, false);
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

	for (const PixelPatch& patch : step.patches)
	{
		apply_patch(image, patch, true);
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
	// El paso más reciente se conserva aunque él solo supere el presupuesto;
	// así la última acción siempre se puede deshacer (límite blando).
	while (over_budget() && step_count() > 1)
	{
		evict_oldest();
	}
}
