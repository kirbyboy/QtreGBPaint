// Smoke test: Selection delete through the real frontend. The first deletion
// uses the same session command as Selection > Delete; the second uses the
// Delete key accelerator.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       -Ithird_party/ImGuiFileDialog $(sdl2-config --cflags) tests/smoke/smoke_selection_delete.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       third_party/ImGuiFileDialog/libImGuiFileDialog.a lib/imgui/libimgui.a \
//       lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -pthread -o temp/smoke_selection_delete

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
#include "tools/selection_tool.h"

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
#define REQUIRE(expr) CHECK(expr)

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

void push_delete(SDL_Window* window, bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.timestamp = SDL_GetTicks();
	event.key.windowID = SDL_GetWindowID(window);
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.repeat = 0;
	event.key.keysym.sym = SDLK_DELETE;
	event.key.keysym.scancode = SDL_SCANCODE_DELETE;
	SDL_PushEvent(&event);
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
	h.resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/selection_button.idx", gray);

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

	SDL_SetWindowInputFocus(window);
	SDL_RaiseWindow(window);

	PaintBackend& app = h.resources;
	PaintSession& canvas = app.session();
	app.set_foreground_index(3);
	app.set_background_index(0);

	ImageBuffer image(16, 16, 2);
	image.fill(0);
	for (int y = 4; y < 8; ++y)
	{
		for (int x = 4; x < 8; ++x)
		{
			image.set_pixel(x, y, 2);
		}
	}
	image.set_pixel(8, 8, 1);
	canvas.load_image(std::move(image));

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			h.frontend.select_tool("Selection");
			REQUIRE(canvas.begin_selection(PaintCore::Rect{4, 4, 4, 4}, 0));
			REQUIRE(canvas.move_floating_region(4, 4));
			REQUIRE(canvas.delete_floating_region());
		}
		else if (frame == 4)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK_FALSE(canvas.has_clipboard());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(5, 5), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(8, 8), 1);
			CHECK(canvas.can_undo());
			save_frame(renderer, "temp/smoke_selection_delete.ppm");

			REQUIRE(canvas.undo());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 2);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(8, 8), 1);

			REQUIRE(canvas.begin_selection(PaintCore::Rect{4, 4, 2, 2}, 0));
			REQUIRE(canvas.move_floating_region(-4, -4));
			push_delete(window, true);
		}
		else if (frame == 5)
		{
			push_delete(window, false);
		}
		else if (frame == 7)
		{
			CHECK_FALSE(canvas.has_floating_region());
			CHECK_EQ(canvas.get_main_buffer().get_pixel(0, 0), 0);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(4, 4), 0);
			CHECK(canvas.can_undo());
			save_frame(renderer, "temp/smoke_selection_delete_key.ppm");
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
