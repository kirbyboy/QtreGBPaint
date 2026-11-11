// Smoke test: Selection > Flip/Rotate modal.
//
// Opens the modal from the menu and from Ctrl+R, checks that Cancel leaves the
// floating selection untouched and that OK applies the selected transform
// without committing or cancelling the selection.

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

bool popup_open_by_name(const char* name)
{
	for (const ImGuiPopupData& popup :
	     ImGui::GetCurrentContext()->OpenPopupStack)
	{
		if (popup.Window && popup.Window->Name &&
		    std::strcmp(popup.Window->Name, name) == 0)
		{
			return true;
		}
	}
	return false;
}

ImVec2 modal_button_position(const char* name, int button_index)
{
	ImGuiWindow* window = ImGui::FindWindowByName(name);
	if (!window)
	{
		return ImVec2(-1.0f, -1.0f);
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float frame_height = ImGui::GetFrameHeight();
	const float x = window->Pos.x + style.WindowPadding.x +
		(button_index == 0 ? 40.0f : 80.0f + style.ItemSpacing.x + 40.0f);
	const float y = window->Pos.y + window->Size.y -
		style.WindowPadding.y - frame_height * 0.5f;
	return ImVec2(x, y);
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
	REQUIRE(app.new_document(6, 5, error));
	REQUIRE(session.document().apply_pixel_patch(
		PaintCore::Rect{1, 1, 3, 2},
		{0, 1, 2, 3, 4, 5}));
	app.selectButton("Fill");
	REQUIRE(session.begin_selection(PaintCore::Rect{1, 1, 3, 2}, 0));

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			push_motion(window, 146, 24);
		}
		else if (frame == 3)
		{
			push_button(window, 146, 24, true);
		}
		else if (frame == 4)
		{
			push_button(window, 146, 24, false);
		}
		else if (frame == 5)
		{
			push_motion(window, 180, 124);
		}
		else if (frame == 6)
		{
			push_button(window, 180, 124, true);
		}
		else if (frame == 7)
		{
			push_button(window, 180, 124, false);
		}
		else if (frame == 9)
		{
			save_frame(renderer, "temp/smoke_flip_rotate_modal.ppm");
			const ImVec2 cancel = modal_button_position("Flip/Rotate", 1);
			push_motion(window, static_cast<int>(cancel.x),
			            static_cast<int>(cancel.y));
		}
		else if (frame == 10)
		{
			const ImVec2 cancel = modal_button_position("Flip/Rotate", 1);
			push_button(window, static_cast<int>(cancel.x),
			            static_cast<int>(cancel.y), true);
		}
		else if (frame == 11)
		{
			const ImVec2 cancel = modal_button_position("Flip/Rotate", 1);
			push_button(window, static_cast<int>(cancel.x),
			            static_cast<int>(cancel.y), false);
		}
		else if (frame == 13)
		{
			push_key(window, SDLK_LCTRL, true, KMOD_CTRL);
		}
		else if (frame == 14)
		{
			push_key(window, SDLK_r, true, KMOD_CTRL);
		}
		else if (frame == 15)
		{
			push_key(window, SDLK_r, false, KMOD_CTRL);
		}
		else if (frame == 16)
		{
			push_key(window, SDLK_LCTRL, false, 0);
		}
		else if (frame == 18)
		{
			h.frontend.m_flip_rotate_mode = 2;
			h.frontend.m_flip_rotate_angle = 90;
		}
		else if (frame == 20)
		{
			// The modal needs a frame to auto-resize after the Rotate radios
			// appear at frame 18.
			save_frame(renderer, "temp/smoke_flip_rotate_rotate.ppm");
		}
		else if (frame == 21)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate", 0);
			push_motion(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y));
		}
		else if (frame == 22)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate", 0);
			push_button(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y), true);
		}
		else if (frame == 23)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate", 0);
			push_button(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y), false);
		}
		else if (frame == 25)
		{
			save_frame(renderer, "temp/smoke_flip_rotate_result.ppm");
			session.cancel_floating_region();
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 8)
		{
			CHECK(popup_open_by_name("Flip/Rotate"));
		}
		else if (frame == 12)
		{
			CHECK_FALSE(popup_open_by_name("Flip/Rotate"));
			REQUIRE(session.has_floating_region());
			CHECK_EQ(session.floating_region().rect.w, 3);
			CHECK_EQ(session.floating_region().rect.h, 2);
			CHECK_EQ(session.floating_region().content().get_pixel(0, 0), 0);
			CHECK_EQ(session.floating_region().content().get_pixel(2, 1), 5);
		}
		else if (frame == 17)
		{
			CHECK(popup_open_by_name("Flip/Rotate"));
		}
		else if (frame == 24)
		{
			CHECK_FALSE(popup_open_by_name("Flip/Rotate"));
			REQUIRE(session.has_floating_region());
			CHECK_EQ(session.floating_region().rect.w, 2);
			CHECK_EQ(session.floating_region().rect.h, 3);
			CHECK_EQ(session.floating_region().content().width(), 2);
			CHECK_EQ(session.floating_region().content().height(), 3);
			CHECK_EQ(session.floating_region().content().get_pixel(0, 0), 3);
			CHECK_EQ(session.floating_region().content().get_pixel(1, 0), 0);
			CHECK_EQ(session.floating_region().content().get_pixel(0, 1), 4);
			CHECK_EQ(session.floating_region().content().get_pixel(1, 1), 1);
			CHECK_EQ(session.floating_region().content().get_pixel(0, 2), 5);
			CHECK_EQ(session.floating_region().content().get_pixel(1, 2), 2);
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
