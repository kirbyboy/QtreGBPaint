#ifndef QTREGBPAINT_TOOL_OPTION_H
#define QTREGBPAINT_TOOL_OPTION_H

#include <string>
#include <utility>
#include <vector>

#include "core/vec2.h"

enum class ToolOptionType
{
	Choice,
	Integer,
};

struct ToolOptionChoice
{
	int value = 0;
	std::string label;
	std::string short_label;
	std::string icon;
	// Optional footprint preview for compact grids. Offsets are relative to
	// the hotspot. Empty means fall back to the icon or short label.
	std::vector<PaintCore::Vec2> preview;
};

struct ToolOption
{
	std::string id;
	std::string label;
	ToolOptionType type = ToolOptionType::Choice;
	bool visible_in_options_bar = true;
	// Choice layout: 1 keeps the full-width list; >1 renders a row-major grid
	// with that many columns (used by the brush palette).
	int grid_columns = 1;
	int value = 0;
	int min_value = 0;
	int max_value = 0;
	std::vector<ToolOptionChoice> choices;

	static ToolOption choice(std::string id, std::string label, int value,
	                         std::vector<ToolOptionChoice> choices,
	                         bool visible_in_options_bar = true)
	{
		ToolOption option;
		option.id = std::move(id);
		option.label = std::move(label);
		option.type = ToolOptionType::Choice;
		option.visible_in_options_bar = visible_in_options_bar;
		option.value = value;
		option.choices = std::move(choices);
		return option;
	}

	static ToolOption integer(std::string id, std::string label, int value,
	                          int min_value, int max_value,
	                          bool visible_in_options_bar = true)
	{
		ToolOption option;
		option.id = std::move(id);
		option.label = std::move(label);
		option.type = ToolOptionType::Integer;
		option.visible_in_options_bar = visible_in_options_bar;
		option.value = value;
		option.min_value = min_value;
		option.max_value = max_value;
		return option;
	}
};

#endif
