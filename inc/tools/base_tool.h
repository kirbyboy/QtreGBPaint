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

		// Tools that paint can update the palette indices they use. The
		// default implementation ignores them (e.g. selection tools).
		virtual void set_colors(PaintCore::ColorIndex foreground,
		                        PaintCore::ColorIndex background)
		{
			(void)foreground;
			(void)background;
		}

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

		// Transient marquee drawn by selection-like tools while dragging.
		// Floating content itself lives in the editor, not in the tool.
		virtual bool has_marquee() const { return false; }
		virtual PaintCore::Rect marquee_rect() const { return {}; }

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