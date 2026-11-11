// Smoke test: anchored zoom through the real frontend.
//
// Checks that Ctrl+Plus/Minus and Ctrl+wheel keep the image point under the
// pointer fixed, and that keyboard zoom without the pointer over the canvas
// anchors at the center of the visible image.

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

#define private public
#include "paint/paint_frontend.h"
#undef private

#include "core/imgui_manager.h"
#include "core/sdl_renderer_backend.h"

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
#define CHECK_NEAR(a, b) CHECK(std::fabs((a) - (b)) <= 0.25f)
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

void push_key(SDL_Window* window, SDL_Keycode key, bool down, Uint16 mod)
{
	SDL_Event event{};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.timestamp = SDL_GetTicks();
	event.key.windowID = SDL_GetWindowID(window);
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.repeat = 0;
	event.key.keysym.sym = key;
	SDL_Scancode scancode = SDL_GetScancodeFromKey(key);
	if (key == SDLK_EQUALS)
	{
		scancode = SDL_SCANCODE_EQUALS;
	}
	else if (key == SDLK_MINUS)
	{
		scancode = SDL_SCANCODE_MINUS;
	}
	event.key.keysym.scancode = scancode;
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

void push_wheel(SDL_Window* window, float amount)
{
	SDL_Event event{};
	event.type = SDL_MOUSEWHEEL;
	event.wheel.timestamp = SDL_GetTicks();
	event.wheel.windowID = SDL_GetWindowID(window);
	event.wheel.which = 0;
	event.wheel.x = 0;
	event.wheel.y = (amount > 0.0f) ? 1 : -1;
	event.wheel.preciseX = 0.0f;
	event.wheel.preciseY = amount;
	event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
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
	std::string error;
	REQUIRE(app.new_document(256, 256, error));

	// Large canvas at zoom 4: the image covers the viewport and scrolling is
	// possible in both axes.
	h.frontend.m_zoom = 4;

	PaintCore::Vec2f pointer_anchor{};
	PaintCore::Vec2f pointer_image_before{};
	PaintCore::Vec2f center_anchor{};
	PaintCore::Vec2f center_image_before{};

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 1)
		{
			ImGuiWindow* canvas_window = nullptr;
			for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
			{
				if (std::strstr(candidate->Name, "/Canvas_") != nullptr)
				{
					canvas_window = candidate;
					break;
				}
			}
			if (canvas_window)
			{
				ImGui::SetScrollX(canvas_window, 300.0f);
				ImGui::SetScrollY(canvas_window, 200.0f);
			}
		}
		else if (frame == 2)
		{
			const ImVec2 center = h.frontend.m_canvas_area.visible_image_center();
			push_motion(window, static_cast<int>(center.x),
			            static_cast<int>(center.y));
		}
		else if (frame == 3)
		{
			pointer_anchor = PaintCore::Vec2f{
				ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y};
			pointer_image_before =
				h.frontend.m_canvas_area.m_view.screen_to_image(pointer_anchor);
			save_frame(renderer, "temp/smoke_zoom_before.ppm");
		}
		// Ctrl+Plus: mouse-anchored zoom in.
		else if (frame >= 4 && frame <= 7)
		{
			push_ctrl_combo(window, SDLK_EQUALS, frame - 4);
		}
		// Ctrl+Minus: back to 4.
		else if (frame >= 9 && frame <= 12)
		{
			push_ctrl_combo(window, SDLK_MINUS, frame - 9);
		}
		// Ctrl+wheel: zoom in and back out.
		else if (frame == 15)
		{
			push_key(window, SDLK_LCTRL, true, KMOD_CTRL);
		}
		else if (frame == 16)
		{
			push_wheel(window, 1.0f);
		}
		else if (frame == 19)
		{
			push_wheel(window, -1.0f);
		}
		else if (frame == 22)
		{
			push_key(window, SDLK_LCTRL, false, 0);

			// Move the pointer outside the canvas for the center anchor.
			push_motion(window, 10, 590);
		}
		else if (frame == 24)
		{
			center_anchor = PaintCore::Vec2f{
				h.frontend.m_canvas_area.visible_image_center().x,
				h.frontend.m_canvas_area.visible_image_center().y};
			center_image_before =
				h.frontend.m_canvas_area.m_view.screen_to_image(center_anchor);
		}
		// Ctrl+Plus with the pointer outside: center-anchored zoom in.
		else if (frame >= 25 && frame <= 28)
		{
			push_ctrl_combo(window, SDLK_EQUALS, frame - 25);
		}
		else if (frame == 31)
		{
			save_frame(renderer, "temp/smoke_zoom_after.ppm");

			// Limits: zoom steps never leave 1..64.
			h.frontend.m_zoom = 64;
			h.frontend.request_zoom_steps(1);
			h.frontend.apply_zoom_request();
			CHECK_EQ(h.frontend.m_zoom, 64);
			h.frontend.m_zoom = 1;
			h.frontend.request_zoom_steps(-1);
			h.frontend.apply_zoom_request();
			CHECK_EQ(h.frontend.m_zoom, 1);
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		// The view is from the last completed frame. The zoom commit happens
		// at the end of the request frame, so check one frame later.
		if (frame == 7)
		{
			CHECK_EQ(h.frontend.m_zoom, 5);
			const PaintCore::Vec2f after =
				h.frontend.m_canvas_area.m_view.screen_to_image(pointer_anchor);
			CHECK_NEAR(after.x, pointer_image_before.x);
			CHECK_NEAR(after.y, pointer_image_before.y);
		}
		else if (frame == 13)
		{
			CHECK_EQ(h.frontend.m_zoom, 4);
			const PaintCore::Vec2f after =
				h.frontend.m_canvas_area.m_view.screen_to_image(pointer_anchor);
			CHECK_NEAR(after.x, pointer_image_before.x);
			CHECK_NEAR(after.y, pointer_image_before.y);
		}
		else if (frame == 18)
		{
			CHECK_EQ(h.frontend.m_zoom, 5);
			const PaintCore::Vec2f after =
				h.frontend.m_canvas_area.m_view.screen_to_image(pointer_anchor);
			CHECK_NEAR(after.x, pointer_image_before.x);
			CHECK_NEAR(after.y, pointer_image_before.y);
		}
		else if (frame == 21)
		{
			CHECK_EQ(h.frontend.m_zoom, 4);
			const PaintCore::Vec2f after =
				h.frontend.m_canvas_area.m_view.screen_to_image(pointer_anchor);
			CHECK_NEAR(after.x, pointer_image_before.x);
			CHECK_NEAR(after.y, pointer_image_before.y);
		}
		else if (frame == 29)
		{
			CHECK_EQ(h.frontend.m_zoom, 5);
			const PaintCore::Vec2f after =
				h.frontend.m_canvas_area.m_view.screen_to_image(center_anchor);
			CHECK_NEAR(after.x, center_image_before.x);
			CHECK_NEAR(after.y, center_image_before.y);
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
