#include "paint/paint_frontend.h"

#include <algorithm>

void PaintFrontend::copy_selection()
{
	if (m_backend.session().has_floating_region())
	{
		m_backend.session().copy_to_clipboard();
	}
}

void PaintFrontend::cut_selection()
{
	if (m_backend.session().has_floating_region())
	{
		m_backend.session().cut_to_clipboard();
	}
}

void PaintFrontend::paste_selection()
{
	if (!m_backend.session().has_clipboard())
	{
		return;
	}

	// Paste enters selection mode so the pasted region can use and toggle the
	// current selection transparency option.
	select_tool("Selection");
	m_backend.session().paste_from_clipboard(
		m_canvas_area.visible_image_origin());
	if (ToolBase* tool = m_backend.session().active_tool())
	{
		tool->sync_floating_region(m_backend.session());
	}
}

void PaintFrontend::select_all()
{
	// The same path as pressing the Selection button and dragging over the
	// whole canvas: the tool applies its current Normal/Transparent mode.
	select_tool("Selection");
	if (ToolBase* tool = m_backend.session().active_tool())
	{
		tool->select_all(m_backend.session());
	}
}

void PaintFrontend::open_properties_dialog()
{
	m_canvas_size_width = m_backend.session().getWidth();
	m_canvas_size_height = m_backend.session().getHeight();
	m_canvas_size_bpp = m_backend.session().getBitsPerPixel();
	m_open_properties_popup = true;
}

void PaintFrontend::open_flip_rotate_dialog(FlipRotateTarget target)
{
	if (target == FlipRotateTarget::Selection &&
	    !m_backend.session().has_floating_region())
	{
		return;
	}

	m_flip_rotate_target = target;
	m_flip_rotate_mode = 0;
	m_flip_rotate_angle = 90;
	m_open_flip_rotate_popup = true;
}

void PaintFrontend::apply_flip_rotate()
{
	using Transform = PaintCore::ImageTransform;

	Transform transform = Transform::FlipHorizontal;
	if (m_flip_rotate_mode == 1)
	{
		transform = Transform::FlipVertical;
	}
	else if (m_flip_rotate_mode == 2)
	{
		switch (m_flip_rotate_angle)
		{
			case 180:
				transform = Transform::Rotate180;
				break;
			case 270:
				transform = Transform::Rotate270;
				break;
			default:
				transform = Transform::Rotate90;
				break;
		}
	}

	if (m_flip_rotate_target == FlipRotateTarget::Selection)
	{
		m_backend.session().transform_floating_region(transform);
	}
	else
	{
		// The session commits a floating region/pending edit first, then
		// records the canvas transform as a second history step.
		m_backend.transform_canvas(transform);
	}
}

void PaintFrontend::request_zoom_steps(int steps)
{
	m_zoom_key_steps += steps;
}

void PaintFrontend::apply_zoom_request()
{
	int target = m_zoom;
	bool has_target = false;
	ImVec2 anchor{};

	if (m_zoom_slider_target >= 0)
	{
		target = m_zoom_slider_target;
		anchor = m_canvas_area.visible_image_center();
		has_target = true;
	}
	else if (m_zoom_key_steps != 0 || m_zoom_wheel_steps != 0)
	{
		target = std::clamp(m_zoom + m_zoom_key_steps + m_zoom_wheel_steps,
		                    1, 64);
		if (m_zoom_wheel_steps != 0)
		{
			anchor = m_zoom_wheel_anchor;
		}
		else if (m_canvas_area.mouse_over_canvas() &&
		         m_canvas_area.pointer_over_image(ImGui::GetMousePos()))
		{
			anchor = ImGui::GetMousePos();
		}
		else
		{
			anchor = m_canvas_area.visible_image_center();
		}
		has_target = true;
	}

	if (has_target && target != m_zoom)
	{
		ImVec2 scroll{};
		if (m_canvas_area.scroll_for_zoom_anchor(
				anchor, static_cast<float>(target), scroll))
		{
			m_pending_canvas_scroll = scroll;
			m_apply_pending_canvas_scroll = true;
		}
		else
		{
			m_apply_pending_canvas_scroll = false;
		}
		m_zoom = target;
	}

	m_zoom_key_steps = 0;
	m_zoom_wheel_steps = 0;
	m_zoom_slider_target = -1;
}
