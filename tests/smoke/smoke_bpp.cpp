// Smoke test for changing bits per pixel from Image > Properties.
//
// Captures the warning state and verifies the document, palette and history
// after confirming the dialog.

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
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
	push_motion(window, x, y);

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
	PaintSession& canvas = app.session();
	std::string error;
	REQUIRE(app.new_document(4, 1, 2, error));
	REQUIRE(canvas.document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 4, 1}, {0, 1, 2, 3}));

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;
	int click_x = 0;
	int click_y = 0;

	while (running)
	{
		if (frame == 1)
		{
			h.frontend.m_canvas_size_width = 4;
			h.frontend.m_canvas_size_height = 1;
			h.frontend.m_canvas_size_bpp = 8;
			h.frontend.m_open_properties_popup = true;
		}
		else if (frame == 3)
		{
			save_frame(renderer, "temp/smoke_bpp_dialog.ppm");
		}
		else if (frame == 4)
		{
			ImGuiWindow* properties =
				ImGui::FindWindowByName("Properties");
			REQUIRE(properties != nullptr);
			const ImGuiStyle& style = ImGui::GetStyle();
			click_x = static_cast<int>(
				properties->Pos.x + style.WindowPadding.x + 40.0f);
			click_y = static_cast<int>(
				properties->Pos.y + properties->Size.y -
				style.WindowPadding.y - ImGui::GetFrameHeight() * 0.5f);
			push_motion(window, click_x, click_y);
		}
		else if (frame == 5)
		{
			push_button(window, click_x, click_y, true);
		}
		else if (frame == 6)
		{
			push_button(window, click_x, click_y, false);
		}
		else if (frame == 7)
		{
			CHECK_EQ(canvas.getBitsPerPixel(), 8);
			CHECK_EQ(app.palette().bitsPerPixel(), 8u);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 85);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(2, 0), 170);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(3, 0), 255);
			CHECK(app.is_modified());

			REQUIRE(app.undo());
			CHECK_EQ(canvas.getBitsPerPixel(), 2);
			CHECK_EQ(app.palette().bitsPerPixel(), 2u);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 1);

			REQUIRE(app.redo());
			CHECK_EQ(canvas.getBitsPerPixel(), 8);
			CHECK_EQ(canvas.get_main_buffer().get_pixel(1, 0), 85);
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
