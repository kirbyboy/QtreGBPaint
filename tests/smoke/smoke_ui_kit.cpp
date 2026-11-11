// Smoke test: visual gallery for the PaintUi design system.
//
// Renders every design-system piece in Light/Dark/Classic and English/Spanish/
// pseudo-locale, saving one screenshot per combination in temp/.

#include <SDL.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "paint/checkerboard.h"
#include "paint/color_imgui.h"
#include "paint/i18n.h"
#include "paint/ui_roles.h"
#include "paint/ui_tokens.h"
#include "paint/ui_widgets.h"

namespace
{
int g_failures = 0;
int g_theme = 0;
int g_locale = 0;
int g_selected_grid = 0;
int g_selected_list = 1;

#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
			          << ": " #expr "\n"; \
		} \
	} while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

const char* kThemeNames[] = {"light", "dark", "classic"};
const char* kLocaleSuffixes[] = {"en", "es", "xa"};
const char* kLocaleCodes[] = {"en", "es", "en_XA"};

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
};

struct Variant
{
	int theme = 0;
	int locale = 0;
};

void apply_theme(int theme)
{
	g_theme = theme;
	switch (theme)
	{
		case 1:
			ImGui::StyleColorsDark();
			break;
		case 2:
			ImGui::StyleColorsClassic();
			break;
		default:
			ImGui::StyleColorsLight();
			break;
	}
}

void apply_locale(int locale)
{
	g_locale = locale;
	i18n::set_locale(kLocaleCodes[locale]);
}

void apply_variant(const Variant& variant)
{
	apply_theme(variant.theme);
	apply_locale(variant.locale);
}

void save_frame(SDL_Renderer* renderer, const char* path)
{
	int width = 0;
	int height = 0;
	SDL_GetRendererOutputSize(renderer, &width, &height);
	if (width <= 0 || height <= 0)
	{
		return;
	}

	std::vector<std::uint8_t> pixels(
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
	if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24,
	                         pixels.data(), width * 3) != 0)
	{
		return;
	}

	FILE* file = std::fopen(path, "wb");
	if (!file)
	{
		return;
	}
	std::fprintf(file, "P6\n%d %d\n255\n", width, height);
	std::fwrite(pixels.data(), 1, pixels.size(), file);
	std::fclose(file);
}

void state_chip(const char* id, const char* label, ImU32 fill,
                ImU32 text_color, const ImVec2& pos)
{
	const ImVec2 size(174.0f, 24.0f);
	ImGui::SetCursorScreenPos(pos);
	ImGui::PushID(id);
	ImGui::InvisibleButton("##chip", size);
	ImGui::PopID();

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const ImVec2 end(pos.x + size.x, pos.y + size.y);
	draw_list->AddRectFilled(pos, end, fill);
	draw_list->AddRect(pos, end, PaintUi::Roles::border());

	const ImVec2 text_size = ImGui::CalcTextSize(label);
	draw_list->AddText(
		ImVec2(pos.x + (size.x - text_size.x) * 0.5f,
		       pos.y + (size.y - text_size.y) * 0.5f),
		text_color, label);
}

void role_line(const char* label, ImU32 color)
{
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	ImGui::Dummy(ImVec2(14.0f, 14.0f));

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const ImVec2 end(pos.x + 14.0f, pos.y + 14.0f);
	draw_list->AddRectFilled(pos, end, color);
	draw_list->AddRect(pos, end, PaintUi::Roles::border());
	ImGui::SameLine();
	ImGui::TextUnformatted(label);
}

void draw_header()
{
	ImGui::TextUnformatted("QtreGBPaint UI Kit");
	ImGui::SameLine();
	ImGui::TextDisabled("%s", i18n::tr("design tokens, roles and widgets"));
	ImGui::Separator();

	ImGui::TextUnformatted(i18n::tr("Theme"));
	ImGui::SameLine();
	if (ImGui::RadioButton(i18n::tr("Light"), g_theme == 0))
	{
		apply_theme(0);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton(i18n::tr("Dark"), g_theme == 1))
	{
		apply_theme(1);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton(i18n::tr("Classic"), g_theme == 2))
	{
		apply_theme(2);
	}

	ImGui::TextUnformatted(i18n::tr("Language"));
	ImGui::SameLine();
	if (ImGui::RadioButton("English", g_locale == 0))
	{
		apply_locale(0);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Español", g_locale == 1))
	{
		apply_locale(1);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Pseudo (en_XA)", g_locale == 2))
	{
		apply_locale(2);
	}
	ImGui::Separator();
}

void draw_toolbar(ImTextureID icon)
{
	ImGui::TextUnformatted(i18n::tr("Toolbar"));
	PaintUi::tool_button("Pencil", icon, true, i18n::tr("Pencil"));
	ImGui::SameLine();
	PaintUi::tool_button("Brush", icon, false, i18n::tr("Brush"));
	ImGui::SameLine();
	PaintUi::tool_button("Line", 0, false, i18n::tr("Line"));
	ImGui::SameLine();
	ImGui::BeginDisabled();
	PaintUi::tool_button("Eraser", icon, false, i18n::tr("Eraser"));
	ImGui::EndDisabled();

	ImGui::PushStyleColor(ImGuiCol_Text,
	                      ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("%s",
		i18n::tr("Active / inactive / text fallback / disabled"));
	ImGui::PopStyleColor();
}

void draw_options()
{
	ImGui::TextUnformatted(i18n::tr("Options"));

	static const char* grid_labels[] = {
		"Contiguous", "Global", "Normal", "Transparent",
		"Outline", "Fill", "Both", "Area"};
	const ImVec2 grid_start = ImGui::GetCursorScreenPos();
	const float cell_width = 174.0f;
	const float cell_height = 24.0f;
	const float gap = PaintUi::Space::kXs;

	for (int index = 0; index < 8; ++index)
	{
		const ImVec2 cell_min(
			grid_start.x + (index % 2) * (cell_width + gap),
			grid_start.y + (index / 2) * (cell_height + gap));
		ImGui::PushID(index);
		const PaintUi::OptionCellState state = PaintUi::option_cell(
			"##grid", cell_min, ImVec2(cell_width, cell_height),
			index == g_selected_grid);
		if (state.clicked)
		{
			g_selected_grid = index;
		}

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const char* label = i18n::tr(grid_labels[index]);
		const ImVec2 text_size = ImGui::CalcTextSize(label);
		draw_list->AddText(
			ImVec2(cell_min.x + (cell_width - text_size.x) * 0.5f,
			       cell_min.y + (cell_height - text_size.y) * 0.5f),
			PaintUi::Roles::text(), label);
		ImGui::PopID();
	}

	ImGui::SetCursorScreenPos(ImVec2(
		grid_start.x,
		grid_start.y + 4.0f * (cell_height + gap) + PaintUi::Space::kXs));

	static const char* list_labels[] = {
		"Width 1 px", "Width 2 px", "Width 3 px"};
	for (int index = 0; index < 3; ++index)
	{
		const ImVec2 row_pos = ImGui::GetCursorScreenPos();
		const ImVec2 row_size(ImGui::GetContentRegionAvail().x, cell_height);
		ImGui::PushID(index);
		const PaintUi::OptionCellState state = PaintUi::option_cell(
			"##list", row_pos, row_size, index == g_selected_list);
		if (state.clicked)
		{
			g_selected_list = index;
		}

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const char* label = i18n::tr(list_labels[index]);
		draw_list->AddText(
			ImVec2(row_pos.x + PaintUi::Space::kSm,
			       row_pos.y + (row_size.y - ImGui::GetTextLineHeight()) * 0.5f),
			PaintUi::Roles::text(), label);
		ImGui::PopID();
	}
}

void draw_states()
{
	ImGui::TextUnformatted(i18n::tr("States"));

	struct Chip
	{
		const char* id;
		const char* label;
		ImU32 fill;
		ImU32 text;
	};

	const ImU32 disabled_text = ImGui::GetColorU32(ImGuiCol_Text, 0.4f);
	const Chip chips[] = {
		{"default", i18n::tr("Default"), IM_COL32(0, 0, 0, 0),
		 PaintUi::Roles::text()},
		{"hover", i18n::tr("Hover"), PaintUi::Roles::hover_overlay(),
		 PaintUi::Roles::text()},
		{"pressed", i18n::tr("Pressed"), PaintUi::Roles::pressed_overlay(),
		 PaintUi::Roles::text()},
		{"selected", i18n::tr("Selected"), PaintUi::Roles::selected_fill(),
		 PaintUi::Roles::text()},
		{"accent", i18n::tr("Accent"), PaintUi::Roles::accent(),
		 PaintUi::Roles::text()},
		{"disabled", i18n::tr("Disabled"), IM_COL32(0, 0, 0, 0),
		 disabled_text},
	};

	const ImVec2 start = ImGui::GetCursorScreenPos();
	const float width = 174.0f;
	const float height = 24.0f;
	const float gap = PaintUi::Space::kXs;
	for (int index = 0; index < 6; ++index)
	{
		const ImVec2 pos(
			start.x + (index % 2) * (width + gap),
			start.y + (index / 2) * (height + gap));
		state_chip(chips[index].id, chips[index].label, chips[index].fill,
		           chips[index].text, pos);
	}
	ImGui::SetCursorScreenPos(
		ImVec2(start.x, start.y + 3.0f * (height + gap)));
	ImGui::Dummy(ImVec2(1.0f, 1.0f));
}

void draw_palette()
{
	ImGui::TextUnformatted(i18n::tr("Palette"));

	const ImVec2 start = ImGui::GetCursorScreenPos();
	const float big = PaintUi::Size::kPaletteColor;
	const ImVec2 big_end(start.x + big, start.y + big);
	ImGui::SetCursorScreenPos(start);
	ImGui::InvisibleButton("##big_swatch", ImVec2(big, big));

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(start, big_end, PaintUi::Roles::surface());
	PaintUi::swatch_frame(draw_list, start, big_end);

	const float small = PaintUi::Size::kSwatch;
	const float inset = PaintUi::Size::kSwatchGap;
	const ImVec2 fg(start.x + inset, start.y + inset);
	const ImVec2 bg(start.x + big - inset - small,
	                start.y + big - inset - small);
	const ImVec2 fg_end(fg.x + small, fg.y + small);
	const ImVec2 bg_end(bg.x + small, bg.y + small);
	draw_list->AddRectFilled(fg, fg_end, IM_COL32(230, 230, 190, 255));
	PaintUi::swatch_frame(draw_list, fg, fg_end);
	draw_list->AddRectFilled(bg, bg_end, IM_COL32(30, 50, 40, 255));
	PaintUi::swatch_frame(draw_list, bg, bg_end);

	const std::array<PaintCore::Color, 6> colors = {{
		{230, 230, 190, 255},
		{140, 190, 90, 255},
		{70, 120, 60, 255},
		{30, 50, 40, 255},
		{200, 120, 80, 255},
		{80, 120, 200, 255},
	}};
	const float x0 = start.x + big + PaintUi::Space::kSm;
	for (int index = 0; index < 6; ++index)
	{
		const ImVec2 pos(
			x0 + index * (small + PaintUi::Space::kXs), start.y);
		const ImVec2 end(pos.x + small, pos.y + small);
		ImGui::PushID(index);
		ImGui::SetCursorScreenPos(pos);
		ImGui::InvisibleButton("##small_swatch", ImVec2(small, small));
		ImGui::PopID();
		if (index == 5)
		{
			PaintUi::draw_checkerboard(
				draw_list, pos, end, PaintUi::Space::kXs, pos);
		}
		else
		{
			draw_list->AddRectFilled(pos, end, toImU32(colors[index]));
		}
		PaintUi::swatch_frame(draw_list, pos, end);
	}

	ImGui::SetCursorScreenPos(
		ImVec2(start.x, start.y + big + PaintUi::Space::kSm));
	ImGui::PushStyleColor(ImGuiCol_Text,
	                      ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("%s",
		i18n::tr("Opaque and transparent swatches"));
	ImGui::PopStyleColor();
}

void draw_dialog()
{
	ImGui::TextUnformatted(i18n::tr("Dialog"));
	ImGui::BeginChild("##dialog_card", ImVec2(0.0f, 96.0f), true);
	ImGui::TextUnformatted(i18n::tr("New Image"));
	ImGui::PushStyleColor(ImGuiCol_Text,
	                      ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("%s",
		i18n::tr("New area uses the current background color."));
	ImGui::PopStyleColor();
	ImGui::Separator();
	PaintUi::dialog_button(i18n::tr("OK"));
	ImGui::SameLine();
	PaintUi::dialog_button(i18n::tr("Cancel"));
	ImGui::SameLine();
	ImGui::BeginDisabled();
	PaintUi::dialog_button(i18n::tr("Discard"));
	ImGui::EndDisabled();
	ImGui::EndChild();
}

void draw_text_demo()
{
	ImGui::TextUnformatted(i18n::tr("Long text"));
	ImGui::BeginChild("##long_text", ImVec2(0.0f, 68.0f), true);
	ImGui::TextWrapped("%s", i18n::tr(
		"Plain decimal text, one value per pixel. Easy to inspect, "
		"but the largest format."));
	ImGui::EndChild();
}

void draw_roles()
{
	ImGui::TextUnformatted(i18n::tr("Roles"));
	role_line(i18n::tr("Accent"), PaintUi::Roles::accent());
	role_line(i18n::tr("Selection"), PaintUi::Roles::selection());
	role_line(i18n::tr("Text"), PaintUi::Roles::text());
	role_line(i18n::tr("Muted"), PaintUi::Roles::text_muted());
}

void draw_gallery(ImTextureID icon)
{
	const std::string title = std::string(i18n::tr("UI Kit")) + "###ui_kit";
	ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(780.0f, 580.0f), ImGuiCond_Always);
	ImGui::Begin(
		title.c_str(), nullptr,
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

	draw_header();

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
	ImGui::BeginChild("##left", ImVec2(370.0f, 0.0f), true,
	                  ImGuiWindowFlags_NoScrollbar |
	                      ImGuiWindowFlags_NoScrollWithMouse);
	draw_toolbar(icon);
	ImGui::Separator();
	draw_options();
	ImGui::Separator();
	draw_states();
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##right", ImVec2(0.0f, 0.0f), true,
	                  ImGuiWindowFlags_NoScrollbar |
	                      ImGuiWindowFlags_NoScrollWithMouse);
	draw_palette();
	ImGui::Separator();
	draw_dialog();
	ImGui::Separator();
	draw_text_demo();
	ImGui::Separator();
	draw_roles();
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::End();
}

int run()
{
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

	Harness h;
	h.imgui.init();
	h.backend.init();

	std::string asset_base = "assets";
	if (char* base_path = SDL_GetBasePath())
	{
		asset_base = std::string(base_path) + "../assets";
		SDL_free(base_path);
	}
	i18n::set_language_directory(asset_base + "/../lang");

	SDL_Window* window = nullptr;
	for (Uint32 id = 1; id <= 32 && !window; ++id)
	{
		SDL_Window* candidate = SDL_GetWindowFromID(id);
		if (candidate &&
		    std::strcmp(SDL_GetWindowTitle(candidate), "QtreGBPaint") == 0)
		{
			window = candidate;
		}
	}
	if (!window)
	{
		std::cerr << "SDL window not found\n";
		return 2;
	}
	SDL_Renderer* renderer = SDL_GetRenderer(window);
	if (!renderer)
	{
		std::cerr << "SDL renderer not found\n";
		return 2;
	}

	std::vector<PaintCore::Color> icon_pixels(32 * 32);
	for (int y = 0; y < 32; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			const bool light = ((x / 8 + y / 8) & 1) == 0;
			icon_pixels[static_cast<std::size_t>(y * 32 + x)] =
				light ? PaintCore::Color{235, 235, 235, 255}
				      : PaintCore::Color{60, 60, 60, 255};
		}
	}
	void* icon_texture = h.backend.createTexture(32, 32);
	CHECK(icon_texture != nullptr);
	if (icon_texture)
	{
		h.backend.updateTexture(
			icon_texture, icon_pixels.data(), 32, 32);
	}
	const ImTextureID icon = reinterpret_cast<ImTextureID>(icon_texture);

	const Variant variants[] = {
		{0, 0}, {0, 1}, {0, 2},
		{1, 0}, {1, 1}, {1, 2},
		{2, 0}, {2, 1}, {2, 2},
	};
	const int variant_count =
		static_cast<int>(sizeof(variants) / sizeof(variants[0]));

	int variant_index = 0;
	int phase = 0;
	apply_variant(variants[variant_index]);

	while (true)
	{
		if (phase == 2)
		{
			++variant_index;
			if (variant_index >= variant_count)
			{
				break;
			}
			apply_variant(variants[variant_index]);
			phase = 0;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		draw_gallery(icon);
		h.backend.render_imgui(h.imgui.end_frame());

		if (phase == 1)
		{
			const Variant& variant = variants[variant_index];
			const std::string path =
				std::string("temp/smoke_ui_kit_") +
				kThemeNames[variant.theme] + "_" +
				kLocaleSuffixes[variant.locale] + ".ppm";
			save_frame(renderer, path.c_str());
			CHECK_EQ(i18n::resolved_locale(),
			         kLocaleCodes[variant.locale]);
			if (variant.locale == 2)
			{
				CHECK(std::string(i18n::tr("Open")).rfind("[!! ", 0) == 0);
			}
			if (variant_index == 0)
			{
				CHECK(ImGui::FindWindowByName("###ui_kit") != nullptr);
			}
		}

		++phase;
		SDL_Delay(16);
	}

	if (icon_texture)
	{
		h.backend.destroyTexture(icon_texture);
	}
	h.backend.shutdown();
	h.imgui.shutdown();

	if (g_failures == 0)
	{
		std::cout << "SMOKE OK\n";
		return 0;
	}
	std::cerr << "SMOKE FAIL: " << g_failures << " failures\n";
	return 1;
}
} // namespace

int main()
{
	return run();
}
