// Smoke test: built-in ImGui color themes through the real frontend.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ithird_party/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_theme.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       third_party/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_theme

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/rect.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
#include "tools/fill_tool.h"
#include "tools/square_tool.h"

namespace
{
int g_failures = 0;

#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
			          << ": " #expr "\n"; \
		} \
	} while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_FALSE(expr) CHECK(!(expr))

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

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

bool same_vec2(const ImVec2& a, const ImVec2& b)
{
	return a.x == b.x && a.y == b.y;
}

bool same_vec4(const ImVec4& a, const ImVec4& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
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

	const Palette& gray = h.resources.iconPalette();
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);
	h.resources.addButton("Fill", std::make_shared<FillTool>(1, 2),
	                      PaintCore::CursorType::Fill,
	                      asset_base + "/fill_button.idx", gray);

	h.resources.loadOptionIcon("rect.outline",
	                           asset_base + "/rect_options.idx", 0, 0, 60, 16);
	h.resources.loadOptionIcon("rect.fill",
	                           asset_base + "/rect_options.idx", 0, 16, 60, 16);
	h.resources.loadOptionIcon("rect.both",
	                           asset_base + "/rect_options.idx", 0, 32, 60, 16);
	h.resources.loadOptionIcon("fill.contiguous",
	                           asset_base + "/fill_options.idx", 0, 0, 60, 46);
	h.resources.loadOptionIcon("fill.global",
	                           asset_base + "/fill_options.idx", 0, 46, 60, 46);

	SDL_Window* window = nullptr;
	for (Uint32 id = 1; id <= 32 && !window; ++id)
	{
		SDL_Window* candidate = SDL_GetWindowFromID(id);
		if (candidate && std::strcmp(SDL_GetWindowTitle(candidate), "QtreGBPaint") == 0)
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

	PaintBackend& app = h.resources;
	PaintSession& canvas = app.session();
	app.set_foreground_index(3);
	app.set_background_index(1);

	ImageBuffer image(48, 48, 2);
	for (int y = 0; y < image.height(); ++y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			image.set_pixel(x, y,
				static_cast<std::uint8_t>(((x / 8) + (y / 8)) % 4));
		}
	}
	canvas.load_image(std::move(image));
	h.frontend.m_zoom = 4;

	ImVec2 default_window_padding{};
	ImVec2 default_item_spacing{};
	float default_frame_height = 0.0f;
	ImVec4 default_window_bg{};

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			h.frontend.select_tool("Rect");
		}
		else if (frame == 4)
		{
			CHECK(h.frontend.theme() == PaintFrontend::Theme::Light);
			const ImGuiStyle& style = ImGui::GetStyle();
			default_window_padding = style.WindowPadding;
			default_item_spacing = style.ItemSpacing;
			default_frame_height = ImGui::GetFrameHeight();
			default_window_bg = style.Colors[ImGuiCol_WindowBg];
			save_frame(renderer, "temp/smoke_theme_light.ppm");
		}
		else if (frame == 5)
		{
			h.frontend.set_theme(PaintFrontend::Theme::Dark);
		}
		else if (frame == 7)
		{
			CHECK(h.frontend.theme() == PaintFrontend::Theme::Dark);
			const ImGuiStyle& style = ImGui::GetStyle();
			// Built-in presets change colors only; layout metrics stay put.
			CHECK(same_vec2(style.WindowPadding, default_window_padding));
			CHECK(same_vec2(style.ItemSpacing, default_item_spacing));
			CHECK_EQ(ImGui::GetFrameHeight(), default_frame_height);
			CHECK_FALSE(same_vec4(style.Colors[ImGuiCol_WindowBg],
			                      default_window_bg));
			save_frame(renderer, "temp/smoke_theme_dark.ppm");
		}
		else if (frame == 8)
		{
			h.frontend.set_theme(PaintFrontend::Theme::Classic);
		}
		else if (frame == 10)
		{
			CHECK(h.frontend.theme() == PaintFrontend::Theme::Classic);
			const ImGuiStyle& style = ImGui::GetStyle();
			CHECK(same_vec2(style.WindowPadding, default_window_padding));
			CHECK(same_vec2(style.ItemSpacing, default_item_spacing));
			CHECK_EQ(ImGui::GetFrameHeight(), default_frame_height);
			save_frame(renderer, "temp/smoke_theme_classic.ppm");
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		SDL_Delay(frame_ms);
		++frame;
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
