#include "paint/paint_session.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "paint/canvas_renderer.h"
#include "paint/draw_buffer.h"
#include "tools/base_tool.h"

PaintSession::PaintSession(int w, int h, HistoryLimits history_limits):
m_document(w, h, history_limits),
m_floating_editor(m_document)
{
}

void PaintSession::set_tool(ToolBase* tool)
{
	// Switching tools resolves pending edits instead of leaving two transient
	// zones alive: the floating selection and the tool edit are both committed.
	if (m_floating_editor.has_floating_region())
	{
		m_floating_editor.commit_floating_region();
	}
	if (m_active_tool)
	{
		if (m_active_tool->has_pending_edit())
		{
			m_active_tool->commit_pending_edit(*this);
		}
		m_active_tool->on_cancel(*this);
	}

	m_active_tool = tool;
}

ToolBase* PaintSession::active_tool() const
{
	return m_active_tool;
}

bool PaintSession::has_pending_edit() const
{
	return m_active_tool && m_active_tool->has_pending_edit();
}

bool PaintSession::commit_pending_edit()
{
	if (!m_active_tool || !m_active_tool->has_pending_edit())
	{
		return false;
	}
	return m_active_tool->commit_pending_edit(*this);
}

bool PaintSession::cancel_pending_edit()
{
	if (!m_active_tool || !m_active_tool->has_pending_edit())
	{
		return false;
	}
	m_active_tool->cancel_pending_edit(*this);
	return true;
}

void PaintSession::begin_edit_handle(int index)
{
	if (m_active_tool && m_active_tool->has_pending_edit())
	{
		m_active_tool->begin_edit_handle(index);
	}
}

void PaintSession::move_edit_handle(int index, const PaintCore::Vec2& pos)
{
	if (m_active_tool && m_active_tool->has_pending_edit())
	{
		m_active_tool->move_edit_handle(index, pos);
	}
}

void PaintSession::end_edit_handle()
{
	if (m_active_tool && m_active_tool->has_pending_edit())
	{
		m_active_tool->end_edit_handle();
	}
}

void PaintSession::handle_mouse_press(const PaintCore::Vec2& pos, int button)
{
	if (m_active_tool)
	{
		m_active_tool->on_mouse_press(*this, pos, button);
	}
}

void PaintSession::handle_mouse_move(const PaintCore::Vec2& pos)
{
	if (m_active_tool)
	{
		m_active_tool->on_mouse_move(*this, pos);
	}
}

void PaintSession::handle_mouse_release(const PaintCore::Vec2& pos, int button)
{
	if (m_active_tool)
	{
		m_active_tool->on_mouse_release(*this, pos, button);
	}
}

void PaintSession::commit_draw_buffer(DrawBuffer& buffer)
{
	m_document.commit_draw_buffer(buffer);
}

bool PaintSession::can_undo() const
{
	// Cancelling a floating region or a pending tool edit is an
	// undoable-looking operation from the user's point of view, even though it
	// never touched the history.
	return m_floating_editor.has_floating_region() ||
	       has_pending_edit() ||
	       m_document.can_undo();
}

bool PaintSession::can_redo() const
{
	return !m_floating_editor.has_floating_region() &&
	       !has_pending_edit() &&
	       m_document.can_redo();
}

void PaintSession::cancel_active_stroke()
{
	if (m_active_tool && m_active_tool->is_drawing())
	{
		m_active_tool->on_cancel(*this);
	}
}

bool PaintSession::undo()
{
	// A floating region is not in the history yet: undoing it is equivalent
	// to discarding it.
	if (m_floating_editor.has_floating_region())
	{
		m_floating_editor.cancel_floating_region();
		return true;
	}

	// A pending tool edit (e.g. a curve being shaped) is not in the history
	// either: undo cancels it before touching the document.
	if (has_pending_edit())
	{
		cancel_pending_edit();
		return true;
	}

	// An in-progress stroke is not in the history yet: undoing it is
	// equivalent to cancelling it.
	if (m_active_tool && m_active_tool->is_drawing())
	{
		cancel_active_stroke();
		return true;
	}

	return m_document.undo();
}

bool PaintSession::redo()
{
	if (m_floating_editor.has_floating_region() ||
	    has_pending_edit() ||
	    (m_active_tool && m_active_tool->is_drawing()))
	{
		return false;
	}

	return m_document.redo();
}

void PaintSession::clear_history()
{
	m_document.clear_history();
}

bool PaintSession::resize(int new_width, int new_height,
                          PaintCore::ColorIndex fill)
{
	if (new_width < 1 || new_height < 1)
	{
		return false;
	}

	// A floating selection/paste belongs to the current canvas size. Commit it
	// even if the requested size does not change (e.g. Canvas Size OK).
	m_floating_editor.commit_floating_region();
	if (has_pending_edit())
	{
		commit_pending_edit();
	}

	if (new_width == getWidth() && new_height == getHeight())
	{
		return false;
	}

	if (m_active_tool)
	{
		m_active_tool->on_cancel(*this);
	}

	return m_document.resize(new_width, new_height, fill);
}

bool PaintSession::begin_selection(const PaintCore::Rect& rect,
                                   PaintCore::ColorIndex hole_fill,
                                   bool transparent_background)
{
	if (has_pending_edit())
	{
		commit_pending_edit();
	}
	return m_floating_editor.begin_selection(
		rect, hole_fill, transparent_background);
}

bool PaintSession::begin_paste(ImageBuffer content, const PaintCore::Vec2& pos)
{
	if (has_pending_edit())
	{
		commit_pending_edit();
	}
	return m_floating_editor.begin_paste(std::move(content), pos);
}

bool PaintSession::move_floating_region(int dx, int dy)
{
	return m_floating_editor.move_floating_region(dx, dy);
}

bool PaintSession::set_floating_transparency(bool transparent_background,
                                             PaintCore::ColorIndex color)
{
	return m_floating_editor.set_floating_transparency(
		transparent_background, color);
}

bool PaintSession::commit_floating_region()
{
	return m_floating_editor.commit_floating_region();
}

void PaintSession::cancel_floating_region()
{
	m_floating_editor.cancel_floating_region();
}

bool PaintSession::floating_region_contains(const PaintCore::Vec2& pos) const
{
	return m_floating_editor.floating_region_contains(pos);
}

bool PaintSession::copy_to_clipboard()
{
	return m_floating_editor.copy_to_clipboard();
}

bool PaintSession::cut_to_clipboard()
{
	return m_floating_editor.cut_to_clipboard();
}

bool PaintSession::delete_floating_region()
{
	return m_floating_editor.delete_floating_region();
}

bool PaintSession::paste_from_clipboard(const PaintCore::Vec2& pos)
{
	return m_floating_editor.paste_from_clipboard(pos);
}

bool PaintSession::invert_floating_region_colors()
{
	return m_floating_editor.invert_floating_region_colors();
}

void PaintSession::set_history_limits(HistoryLimits limits)
{
	m_document.set_history_limits(limits);
}

HistoryLimits PaintSession::history_limits() const
{
	return m_document.history_limits();
}

int PaintSession::getWidth() const
{
	return m_document.getWidth();
}

int PaintSession::getHeight() const
{
	return m_document.getHeight();
}

int PaintSession::getBitsPerPixel() const
{
	return m_document.getBitsPerPixel();
}

std::uint64_t PaintSession::current_state_id() const
{
	return m_document.current_state_id();
}

void PaintSession::load_image(ImageBuffer image)
{
	if (m_active_tool)
	{
		if (m_active_tool->has_pending_edit())
		{
			m_active_tool->cancel_pending_edit(*this);
		}
		m_active_tool->on_cancel(*this);
	}
	if (m_floating_editor.has_floating_region())
	{
		m_floating_editor.cancel_floating_region();
	}
	m_document.replace_image(std::move(image));
}

const ImageBuffer& PaintSession::get_main_buffer() const
{
	return m_document.get_main_buffer();
}

PaintCore::Rect PaintSession::take_dirty_rect()
{
	return m_document.take_dirty_rect();
}

void PaintSession::render(ICanvasRenderer& renderer,
                          const PaintCore::ViewTransform& view,
                          const Palette& palette) const
{
	m_document.render(renderer, view, palette);
	render_preview(renderer, palette);
}

void PaintSession::render_preview(ICanvasRenderer& renderer,
                                  const Palette& palette) const
{
	if (!m_active_tool)
	{
		return;
	}

	DrawBuffer* draw_buffer = m_active_tool->get_draw_buffer();
	if (!draw_buffer || !draw_buffer->is_active())
	{
		return;
	}

	const int buf_x = draw_buffer->x();
	const int buf_y = draw_buffer->y();
	const int buf_w = draw_buffer->width();
	const int buf_h = draw_buffer->height();
	const std::vector<std::uint8_t>& data = draw_buffer->data();

	for (int y = 0; y < buf_h; ++y)
	{
		for (int x = 0; x < buf_w; ++x)
		{
			const std::uint8_t color_index =
				data[static_cast<std::size_t>(y) * static_cast<std::size_t>(buf_w) +
				     static_cast<std::size_t>(x)];
			if (color_index == PaintCore::TRANSPARENT_INDEX)
			{
				continue;
			}
			renderer.draw_rect_filled(
				buf_x + x, buf_y + y, 1, 1,
				palette.getColor(color_index));
		}
	}
}
