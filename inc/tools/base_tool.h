#ifndef QTREGBPAINT_BASE_TOOL_H
#define QTREGBPAINT_BASE_TOOL_H

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/color.h"
#include "core/rect.h"
#include "core/vec2.h"
#include "tools/tool_option.h"


class PaintSession;
class DrawBuffer;

class ToolBase
{
	public:
		virtual ~ToolBase() = default;

		virtual void on_mouse_press(PaintSession& canvas, const PaintCore::Vec2& pos, int button) = 0;
		virtual void on_mouse_move(PaintSession& canvas, const PaintCore::Vec2& pos) = 0;
		virtual void on_mouse_release(PaintSession& canvas, const PaintCore::Vec2& pos, int button) = 0;

		virtual void on_cancel(PaintSession& canvas) = 0;

		static constexpr int kLeftButton = 1;
		static constexpr int kRightButton = 2;

		// Whether a mouse press with this button may start the tool. Drawing
		// tools accept both buttons; selection-like tools only the left one.
		virtual bool accepts_button(int button) const
		{
			return button == kLeftButton;
		}

		// Tools that paint can update the palette indices they use. The
		// default implementation ignores them (e.g. selection tools).
		virtual void set_colors(PaintCore::ColorIndex foreground,
		                        PaintCore::ColorIndex background)
		{
			(void)foreground;
			(void)background;
		}

		// Result of a color-picker click. `background` is true for the right
		// button; otherwise the color goes to the foreground slot.
		struct ColorPick
		{
			PaintCore::ColorIndex color = 0;
			bool background = false;
		};

		// Color-picker-like tools expose the last clicked color once. The
		// application consumes it after input routing to update the palette
		// and restore the previous editing tool.
		virtual bool take_color_pick(ColorPick& pick)
		{
			(void)pick;
			return false;
		}

		// Eyedropper tools expose the color under the pointer while the button
		// is held, so the frontend can fill the tool-options area with it.
		virtual bool color_pick_preview(PaintCore::ColorIndex& color) const
		{
			(void)color;
			return false;
		}

		// A tool with an uncommitted edit (for example a curve being shaped)
		// exposes its lifecycle so PaintSession can make it exclusive with the
		// floating selection and with undo/redo.
		virtual bool has_pending_edit() const { return false; }
		virtual bool commit_pending_edit(PaintSession& canvas)
		{
			(void)canvas;
			return false;
		}
		virtual void cancel_pending_edit(PaintSession& canvas)
		{
			(void)canvas;
		}

		struct EditGuide
		{
			PaintCore::Vec2 from{};
			PaintCore::Vec2 to{};
		};

		struct EditHandle
		{
			PaintCore::Vec2 pos{};
			bool draggable = true;
		};

		struct EditOverlay
		{
			std::vector<EditGuide> guides;
			std::vector<EditHandle> handles;
		};

		// Editable tools expose their handles in image coordinates. The view
		// draws them as an overlay and routes drags back by handle index.
		virtual bool edit_overlay(EditOverlay& overlay) const
		{
			(void)overlay;
			return false;
		}
		virtual void begin_edit_handle(int index) { (void)index; }
		virtual void move_edit_handle(int index,
		                              const PaintCore::Vec2& pos)
		{
			(void)index;
			(void)pos;
		}
		virtual void end_edit_handle() {}

		const std::vector<ToolOption>& options() const { return m_options; }

		bool set_option(const std::string& id, int value)
		{
			for (ToolOption& option : m_options)
			{
				if (option.id != id)
				{
					continue;
				}

				int new_value = value;
				if (option.type == ToolOptionType::Choice)
				{
					bool valid = false;
					for (const ToolOptionChoice& choice : option.choices)
					{
						if (choice.value == value)
						{
							valid = true;
							break;
						}
					}
					if (!valid)
					{
						return false;
					}
				}
				else
				{
					new_value = std::clamp(value, option.min_value, option.max_value);
				}

				if (option.value == new_value)
				{
					return false;
				}

				option.value = new_value;
				on_option_changed(id, new_value);
				return true;
			}
			return false;
		}

		int option_value(const std::string& id) const
		{
			for (const ToolOption& option : m_options)
			{
				if (option.id == id)
				{
					return option.value;
				}
			}
			return 0;
		}

		virtual DrawBuffer* get_draw_buffer() { return nullptr; }
		virtual bool is_drawing() const { return false; }

		// True while a mouse press is being tracked, even if the tool does not
		// paint. The eyedropper uses it to keep receiving moves and the release
		// while it previews the sampled color.
		virtual bool is_operating() const { return is_drawing(); }

		// Accepts an in-progress stroke when an external action (image
		// transform) must not discard it. The default cancels; drawing and
		// selection tools override it to commit.
		virtual void commit_active_stroke(PaintSession& canvas)
		{
			if (is_drawing())
			{
				on_cancel(canvas);
			}
		}

		// Fixed first point of a two-point operation (line, rect, circle,
		// selection and curve endpoints). While present, the status bar shows
		// the start point and the current pointer position.
		virtual bool drag_anchor(PaintCore::Vec2& anchor) const
		{
			(void)anchor;
			return false;
		}

		// Transient marquee drawn by selection-like tools while dragging.
		// Floating content itself lives in the editor, not in the tool.
		virtual bool has_marquee() const { return false; }
		virtual PaintCore::Rect marquee_rect() const { return {}; }

		// Selection-like tools can create a floating selection covering the
		// whole canvas, applying their current mode and hole background.
		virtual bool select_all(PaintSession& canvas)
		{
			(void)canvas;
			return false;
		}

		// Footprint preview under the pointer. The tool chooses the color and
		// whether the fill is opaque and needs a fixed 1-pixel screen border
		// (the eraser). nullptr offsets mean the tool has no preview.
		struct StampPreview
		{
			const std::vector<PaintCore::Vec2>* offsets = nullptr;
			PaintCore::ColorIndex color = 0;
			bool opaque = false;
			bool border = false;
		};

		virtual StampPreview stamp_preview(
			PaintCore::ColorIndex foreground,
			PaintCore::ColorIndex background) const
		{
			(void)foreground;
			(void)background;
			return {};
		}

		// Tools with live canvas state (e.g. selection transparency) can
		// update the active floating region. Called after colors/options change.
		virtual void sync_floating_region(PaintSession& canvas) { (void)canvas; }

	protected:
		void add_option(ToolOption option)
		{
			for (const ToolOption& existing : m_options)
			{
				if (existing.id == option.id)
				{
					return;
				}
			}

			if (option.type == ToolOptionType::Choice)
			{
				bool valid = false;
				for (const ToolOptionChoice& choice : option.choices)
				{
					if (choice.value == option.value)
					{
						valid = true;
						break;
					}
				}
				if (!valid && !option.choices.empty())
				{
					option.value = option.choices.front().value;
				}
			}
			else
			{
				option.value = std::clamp(option.value, option.min_value, option.max_value);
			}

			m_options.push_back(std::move(option));
		}

		virtual void on_option_changed(const std::string& id, int value)
		{
			(void)id;
			(void)value;
		}

	private:
		std::vector<ToolOption> m_options;
};

#endif