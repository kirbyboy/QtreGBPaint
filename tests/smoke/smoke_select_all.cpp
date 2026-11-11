// Smoke test: Edit > Select All (menu and Ctrl+A).
//
// Checks that Select All behaves like pressing the Selection button and
// dragging from corner to corner, including Normal/Transparent mode and the
// stored hole background.

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

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/rect.h"
#include "core/sdl_renderer_backend.h"
#include "paint/image_buffer.h"
#include "tools/fill_tool.h"
#include "tools/selection_tool.h"
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
#define REQUIRE(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
			          << ": " #expr "\n"; \
			return 1; \
		} \
	} while (0)

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};
};

void push_motion(SDL_Window* window, int x, int y)
{
	SDL_WarpMouseInWindow(window, x, y);

	SDL_Event event{};
	event.type = SDL_MOUSEMOTION;
	event.motion.timestamp = SDL_GetTicks();
	event.motion.windowID = SDL_GetWindowID(window);
	event.motion.which = 0;
	event.motion.state = 0;
	event.motion.x = x;
	event.motion.y = y;
	SDL_PushEvent(&event);
}

void push_button(SDL_Window* window, int x, int y, bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.timestamp = SDL_GetTicks();
	event.button.windowID = SDL_GetWindowID(window);
	event.button.which = 0;
	event.button.button = SDL_BUTTON_LEFT;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = x;
	event.button.y = y;
	SDL_PushEvent(&event);
}

void push_key(SDL_Window* window, SDL_Keycode key, bool down, Uint16 mod)
{
	SDL_Event event{};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.timestamp = SDL_GetTicks();
	event.key.windowID = SDL_GetWindowID(window);
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.repeat = 0;
	event.key.keysym.sym = key;
	event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
	event.key.keysym.mod = mod;
	SDL_PushEvent(&event);
}

void push_ctrl_combo(SDL_Window* window, SDL_Keycode key, int phase)
{
	switch (phase)
	{
		case 0:
			push_key(window, SDLK_LCTRL, true, KMOD_CTRL);
			break;
		case 1:
			push_key(window, key, true, KMOD_CTRL);
			break;
		case 2:
			push_key(window, key, false, KMOD_CTRL);
			break;
		case 3:
			push_key(window, SDLK_LCTRL, false, 0);
			break;
		default:
			break;
	}
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
	h.resources.addButton("Fill", std::make_shared<FillTool>(1, 2),
	                      PaintCore::CursorType::Fill,
	                      asset_base + "/fill_button.idx", gray);
	h.resources.addButton("Selection", std::make_shared<SelectionTool>(),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/selection_button.idx", gray);
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);

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

	PaintBackend& app = h.resources;
	PaintSession& session = app.session();
	std::string error;
	REQUIRE(app.new_document(6, 4, error));
	app.set_background_index(0);
	REQUIRE(session.document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 6, 4},
		{0, 1, 2, 3, 0, 1,
		 2, 3, 0, 1, 2, 3,
		 1, 0, 3, 2, 1, 0,
		 3, 2, 1, 0, 3, 2}));

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 1)
		{
			app.selectButton("Fill");
		}
		else if (frame >= 2 && frame <= 5)
		{
			push_ctrl_combo(window, SDLK_a, frame - 2);
		}
		else if (frame == 7)
		{
			save_frame(renderer, "temp/smoke_select_all.ppm");
			session.cancel_floating_region();
		}
		else if (frame == 8)
		{
			h.frontend.select_tool("Selection");
			ToolBase* tool = session.active_tool();
			REQUIRE(tool != nullptr);
			REQUIRE(tool->set_option(
				"mode", static_cast<int>(SelectionTool::Mode::Transparent)));
		}
		else if (frame >= 9 && frame <= 12)
		{
			push_ctrl_combo(window, SDLK_a, frame - 9);
		}
		else if (frame == 14)
		{
			save_frame(renderer, "temp/smoke_select_all_transparent.ppm");
			session.cancel_floating_region();
		}
		else if (frame == 15)
		{
			app.selectButton("Fill");
			push_motion(window, 56, 24);
		}
		else if (frame == 16)
		{
			push_button(window, 56, 24, true);
		}
		else if (frame == 17)
		{
			push_button(window, 56, 24, false);
		}
		else if (frame == 18)
		{
			// Edit > Select All.
			push_motion(window, 90, 97);
		}
		else if (frame == 19)
		{
			push_button(window, 90, 97, true);
		}
		else if (frame == 20)
		{
			push_button(window, 90, 97, false);
		}
		else if (frame == 22)
		{
			save_frame(renderer, "temp/smoke_select_all_menu.ppm");
			session.cancel_floating_region();
		}
		else if (frame == 24)
		{
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 6)
		{
			REQUIRE(session.has_floating_region());
			CHECK(app.activeButton() == "Selection");
			CHECK_EQ(session.floating_region().rect.x, 0);
			CHECK_EQ(session.floating_region().rect.y, 0);
			CHECK_EQ(session.floating_region().rect.w, 6);
			CHECK_EQ(session.floating_region().rect.h, 4);
			CHECK_EQ(session.floating_region().hole_fill, 0);
			CHECK_FALSE(session.floating_region().transparent_background);
			CHECK_EQ(session.floating_region().content().get_pixel(1, 0), 1);
			CHECK_EQ(session.floating_region().content().get_pixel(3, 3), 0);
			CHECK(app.is_modified());
		}
		else if (frame == 13)
		{
			REQUIRE(session.has_floating_region());
			CHECK(app.activeButton() == "Selection");
			CHECK(session.floating_region().transparent_background);
			CHECK_EQ(session.floating_region().transparent_color, 0);
			CHECK_EQ(session.floating_region().rect.w, 6);
			CHECK_EQ(session.floating_region().rect.h, 4);
		}
		else if (frame == 21)
		{
			REQUIRE(session.has_floating_region());
			CHECK(app.activeButton() == "Selection");
			CHECK_EQ(session.floating_region().rect.w, 6);
			CHECK_EQ(session.floating_region().rect.h, 4);
			CHECK(session.floating_region().transparent_background);
		}

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
