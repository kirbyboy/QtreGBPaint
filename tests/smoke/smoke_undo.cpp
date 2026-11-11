// Smoke test with injected SDL events: exercises the frontend's real shortcuts
// without depending on XTest (which XWayland ignores in this session).
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_undo.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_undo

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"

// Test access to the frontend's internal Canvas (the project is not modified).
#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"
#include "tools/square_tool.h"

namespace
{
int g_failures = 0;

#define CHECK(expr) \
	do { \
		if (!(expr)) { \
			++g_failures; \
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
		} \
	} while (0)

void push_motion(SDL_Window* window, int x, int y, Uint32 state)
{
	SDL_Event event{};
	event.type = SDL_MOUSEMOTION;
	event.motion.timestamp = SDL_GetTicks();
	event.motion.windowID = SDL_GetWindowID(window);
	event.motion.which = 0;
	event.motion.state = state;
	event.motion.x = x;
	event.motion.y = y;
	if (SDL_PushEvent(&event) != 1)
	{
		std::cerr << "SDL_PushEvent(motion) failed\n";
	}
}

void push_button(SDL_Window* window, int x, int y, bool down)
{
	push_motion(window, x, y, down ? SDL_BUTTON_LMASK : 0);

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
	if (SDL_PushEvent(&event) != 1)
	{
		std::cerr << "SDL_PushEvent(button) failed\n";
	}
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
	if (SDL_PushEvent(&event) != 1)
	{
		std::cerr << "SDL_PushEvent(key) failed\n";
	}
}

void push_ctrl_combo(SDL_Window* window, SDL_Keycode key, int phase)
{
	// Cuatro frames: Ctrl down, tecla down, tecla up, Ctrl up.
	switch (phase)
	{
	case 0: push_key(window, SDLK_LCTRL, true, KMOD_CTRL); break;
	case 1: push_key(window, key, true, KMOD_CTRL); break;
	case 2: push_key(window, key, false, KMOD_CTRL); break;
	case 3: push_key(window, SDLK_LCTRL, false, 0); break;
	default: break;
	}
}

PaintCore::Rect nonzero_bbox(const ImageBuffer& image)
{
	int x0 = image.width();
	int y0 = image.height();
	int x1 = -1;
	int y1 = -1;
	for (int y = 0; y < image.height(); ++y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			if (image.data()[static_cast<std::size_t>(y) * image.width() + x] == 0)
			{
				continue;
			}
			x0 = std::min(x0, x);
			y0 = std::min(y0, y);
			x1 = std::max(x1, x);
			y1 = std::max(y1, y);
		}
	}
	if (x1 < 0)
	{
		return {};
	}
	return PaintCore::Rect{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

bool buffer_equal(const ImageBuffer& image, const std::vector<PaintCore::ColorIndex>& expected)
{
	return image.data() == expected;
}

void save_frame(SDL_Renderer* renderer, const char* path)
{
	int width = 0;
	int height = 0;
	SDL_GetRendererOutputSize(renderer, &width, &height);
	if (width <= 0 || height <= 0)
	{
		std::cerr << "invalid renderer size\n";
		return;
	}

	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
	if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGB24,
							 pixels.data(), width * 3) != 0)
	{
		std::cerr << "SDL_RenderReadPixels failed: " << SDL_GetError() << "\n";
		return;
	}

	FILE* file = std::fopen(path, "wb");
	if (!file)
	{
		std::cerr << "could not write " << path << "\n";
		return;
	}
	std::fprintf(file, "P6\n%d %d\n255\n", width, height);
	std::fwrite(pixels.data(), 1, pixels.size(), file);
	std::fclose(file);
}

struct Harness
{
	ImGuiManager imgui;
	SDLRendererBackend backend;
	PaintBackend resources{&backend};
	PaintFrontend frontend{resources};

	PaintSession& canvas() { return frontend.m_canvas_area.session(); }
};

void check_draw1_region(PaintSession& canvas)
{
	const ImageBuffer& image = canvas.get_main_buffer();
	CHECK(image.get_pixel(56, 49) == 3);    // foreground corner
	CHECK(image.get_pixel(106, 89) == 0);   // background fill
	CHECK(image.get_pixel(156, 129) == 3);  // opposite corner
}

void check_draw1_pixels(PaintSession& canvas)
{
	const ImageBuffer& image = canvas.get_main_buffer();
	const PaintCore::Rect bbox = nonzero_bbox(image);
	CHECK(bbox.x == 56);
	CHECK(bbox.y == 49);
	CHECK(bbox.w == 101);
	CHECK(bbox.h == 81);
	check_draw1_region(canvas);
}

void check_draw2(PaintSession& canvas)
{
	const ImageBuffer& image = canvas.get_main_buffer();
	const PaintCore::Rect bbox = nonzero_bbox(image);
	CHECK(bbox.w == 41);
	CHECK(bbox.h == 31);
	CHECK(canvas.can_undo());
}

void check_draw3(PaintSession& canvas)
{
	const ImageBuffer& image = canvas.get_main_buffer();
	// Third rectangle: corner (176,66), size 31x31, not overlapping the first.
	CHECK(image.get_pixel(176, 66) == 3);
	CHECK(image.get_pixel(191, 81) == 0);
	CHECK(image.get_pixel(206, 96) == 3);
	CHECK(canvas.can_undo());
}

int run()
{
	Harness h;
	h.imgui.init();
	h.backend.init();

	// Register the buttons exactly as main.cpp does.
	std::string asset_base = "assets";
	if (SDL_GetBasePath())
	{
		asset_base = std::string(SDL_GetBasePath()) + "../assets";
	}

	const Palette& gray = h.resources.iconPalette();
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
						  PaintCore::CursorType::Crosshair,
						  asset_base + "/rect_button.idx", gray);
	h.resources.addButton("Pencil", nullptr,
						  PaintCore::CursorType::Pencil,
						  asset_base + "/pencil_button.idx", gray);

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

	// The smoke checks pixel indices assuming the classic dark fg / light bg.
	h.resources.set_foreground_index(3);
	h.resources.set_background_index(0);

	std::vector<PaintCore::ColorIndex> after_rebranch;
	std::vector<PaintCore::ColorIndex> after_ctrl_y;

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		// --- Event script ---
		if (frame == 2)
		{
			push_motion(window, 32, 73, 0);
		}
		else if (frame == 3)
		{
			push_button(window, 32, 73, true);
		}
		else if (frame == 4)
		{
			push_button(window, 32, 73, false);
		}
		else if (frame == 5)
		{
			if (ToolBase* tool = h.frontend.m_canvas_area.session().active_tool())
			{
				tool->set_option("mode", static_cast<int>(SquareTool::Mode::Both));
			}
			push_motion(window, 168, 103, 0);
		}
		else if (frame == 6)
		{
			push_button(window, 168, 103, true);
		}
		else if (frame >= 7 && frame <= 17)
		{
			const int t = frame - 7;
			push_motion(window, 168 + 100 * t / 10, 103 + 80 * t / 10, SDL_BUTTON_LMASK);
		}
		else if (frame == 18)
		{
			push_button(window, 268, 183, false);
		}
		else if (frame >= 22 && frame <= 25)
		{
			push_ctrl_combo(window, SDLK_z, frame - 22);
		}
		else if (frame >= 29 && frame <= 32)
		{
			push_ctrl_combo(window, SDLK_y, frame - 29);
		}
		else if (frame == 36)
		{
			push_motion(window, 288, 220, 0);
		}
		else if (frame == 37)
		{
			push_button(window, 288, 220, true);
		}
		else if (frame >= 38 && frame <= 48)
		{
			const int t = frame - 38;
			push_motion(window, 288 + 40 * t / 10, 220 + 30 * t / 10, SDL_BUTTON_LMASK);
		}
		else if (frame == 49)
		{
			push_button(window, 328, 250, false);
		}
		else if (frame >= 53 && frame <= 56)
		{
			push_ctrl_combo(window, SDLK_z, frame - 53);
		}
		else if (frame == 58)
		{
			push_motion(window, 288, 120, 0);
		}
		else if (frame == 59)
		{
			push_button(window, 288, 120, true);
		}
		else if (frame >= 60 && frame <= 68)
		{
			const int t = frame - 60;
			push_motion(window, 288 + 30 * t / 8, 120 + 30 * t / 8, SDL_BUTTON_LMASK);
		}
		else if (frame == 69)
		{
			push_button(window, 318, 150, false);
		}
		else if (frame >= 75 && frame <= 78)
		{
			push_ctrl_combo(window, SDLK_y, frame - 75);
		}
		else if (frame >= 82 && frame <= 85)
		{
			push_ctrl_combo(window, SDLK_z, frame - 82);
		}
		else if (frame >= 88 && frame <= 91)
		{
			push_ctrl_combo(window, SDLK_z, frame - 88);
		}
		else if (frame >= 94 && frame <= 97)
		{
			push_ctrl_combo(window, SDLK_y, frame - 94);
		}
		else if (frame >= 102 && frame <= 105)
		{
			push_ctrl_combo(window, SDLK_y, frame - 102);
		}
		else if (frame == 110)
		{
			push_motion(window, 74, 27, 0);
		}
		else if (frame == 111)
		{
			push_button(window, 74, 27, true);
		}
		else if (frame == 112)
		{
			push_button(window, 74, 27, false);
		}
		else if (frame == 114)
		{
			save_frame(renderer, "temp/smoke_9_menu.ppm");
		}
		else if (frame == 116)
		{
			push_key(window, SDLK_ESCAPE, true, 0);
		}
		else if (frame == 117)
		{
			push_key(window, SDLK_ESCAPE, false, 0);
		}
		else if (frame == 120)
		{
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		// --- Checks after the frame ---
		PaintSession& canvas = h.canvas();
		if (frame == 1 || frame == 20 || frame == 21 || frame == 26 || frame == 27 ||
			frame == 33 || frame == 34 || frame == 52 || frame == 57 || frame == 71 ||
			frame == 80)
		{
			std::cout << "f" << frame << " undo=" << canvas.document().m_history.m_undo.size()
					  << " redo=" << canvas.document().m_history.m_redo.size()
					  << " canU=" << canvas.can_undo()
					  << " canR=" << canvas.can_redo() << std::endl;
		}
		if (frame == 20)
		{
			check_draw1_pixels(canvas);
			CHECK(canvas.can_undo());
			CHECK(!canvas.can_redo());
			save_frame(renderer, "temp/smoke_1_draw.ppm");
			std::cout << "draw1 bbox=" << nonzero_bbox(canvas.get_main_buffer()).x << ","
					  << nonzero_bbox(canvas.get_main_buffer()).y << " "
					  << nonzero_bbox(canvas.get_main_buffer()).w << "x"
					  << nonzero_bbox(canvas.get_main_buffer()).h << "\n";
		}
		else if (frame == 27)
		{
			CHECK(nonzero_bbox(canvas.get_main_buffer()).empty());
			CHECK(!canvas.can_undo());
			CHECK(canvas.can_redo());
			save_frame(renderer, "temp/smoke_2_undo.ppm");
		}
		else if (frame == 34)
		{
			check_draw1_pixels(canvas);
			CHECK(canvas.can_undo());
			CHECK(!canvas.can_redo());
			save_frame(renderer, "temp/smoke_3_redo.ppm");
		}
		else if (frame == 51)
		{
			CHECK(canvas.can_undo());
			CHECK(!canvas.can_redo());
			CHECK(nonzero_bbox(canvas.get_main_buffer()).w > 0);
			save_frame(renderer, "temp/smoke_4_second.ppm");
		}
		else if (frame == 58)
		{
			check_draw1_pixels(canvas);
			CHECK(canvas.can_undo());
			CHECK(canvas.can_redo());
		}
		else if (frame == 71)
		{
			check_draw3(canvas);
			after_rebranch = canvas.get_main_buffer().data();
		}
		else if (frame == 80)
		{
			CHECK(canvas.get_main_buffer().data() == after_rebranch);
			CHECK(!canvas.can_redo());
			after_ctrl_y = canvas.get_main_buffer().data();
			save_frame(renderer, "temp/smoke_6_ctrl_y_noop.ppm");
		}
		else if (frame == 86)
		{
			check_draw1_pixels(canvas);
			CHECK(canvas.can_undo());
			CHECK(canvas.can_redo());
		}
		else if (frame == 92)
		{
			CHECK(nonzero_bbox(canvas.get_main_buffer()).empty());
			CHECK(!canvas.can_undo());
			CHECK(canvas.can_redo());
		}
		else if (frame == 98)
		{
			// The first Ctrl+Y redoes draw1; draw3 remains in the redo stack.
			check_draw1_pixels(canvas);
			CHECK(canvas.can_undo());
			CHECK(canvas.can_redo());
			save_frame(renderer, "temp/smoke_7_first_redo.ppm");
		}
		else if (frame == 108)
		{
			// The second Ctrl+Y redoes draw3; the global bbox is now the union.
			check_draw1_region(canvas);
			check_draw3(canvas);
			CHECK(!canvas.can_redo());
			save_frame(renderer, "temp/smoke_8_final_redo.ppm");
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
