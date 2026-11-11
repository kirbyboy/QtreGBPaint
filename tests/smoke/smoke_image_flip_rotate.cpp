// Smoke test: Image > Flip/Rotate.
//
// Checks the whole-canvas transform modal, Cancel, undo/redo of the transform
// step, and that a floating selection is accepted first (one history step)
// before the image transform (a second history step).

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
#include "core/image_transform.h"
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
	const float x = window->Pos.x + window->WindowPadding.x +
		(button_index == 0 ? 40.0f : 80.0f + style.ItemSpacing.x + 40.0f);
	const float y = window->Pos.y + window->Size.y -
		window->WindowPadding.y - frame_height * 0.5f;
	return ImVec2(x, y);
}

void open_image_flip_rotate(SDL_Window* window, int phase)
{
	switch (phase)
	{
		case 0:
			push_motion(window, 235, 24);
			break;
		case 1:
			push_button(window, 235, 24, true);
			break;
		case 2:
			push_button(window, 235, 24, false);
			break;
		case 3:
			push_motion(window, 260, 52);
			break;
		case 4:
			push_button(window, 260, 52, true);
			break;
		case 5:
			push_button(window, 260, 52, false);
			break;
		default:
			break;
	}
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
	const std::vector<PaintCore::ColorIndex> pattern{
		1, 2, 3, 4,
		5, 6, 7, 8,
	};
	REQUIRE(app.new_document(4, 2, error));
	REQUIRE(session.document().apply_pixel_patch(
		PaintCore::Rect{0, 0, 4, 2}, pattern));
	session.clear_history();

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		// --- Cancel path ---
		if (frame >= 2 && frame <= 7)
		{
			open_image_flip_rotate(window, frame - 2);
		}
		else if (frame == 8 || frame == 9)
		{
			// Let the always-auto-resize modal settle before measuring its
			// buttons.
		}
		else if (frame == 10)
		{
			save_frame(renderer, "temp/smoke_image_flip_rotate_modal.ppm");
			const ImVec2 cancel = modal_button_position("Flip/Rotate###flip_rotate", 1);
			push_motion(window, static_cast<int>(cancel.x),
			            static_cast<int>(cancel.y));
		}
		else if (frame == 11)
		{
			const ImVec2 cancel = modal_button_position("Flip/Rotate###flip_rotate", 1);
			push_button(window, static_cast<int>(cancel.x),
			            static_cast<int>(cancel.y), true);
		}
		else if (frame == 12)
		{
			const ImVec2 cancel = modal_button_position("Flip/Rotate###flip_rotate", 1);
			push_button(window, static_cast<int>(cancel.x),
			            static_cast<int>(cancel.y), false);
		}
		// --- OK path with Rotate 90 ---
		else if (frame >= 14 && frame <= 19)
		{
			open_image_flip_rotate(window, frame - 14);
		}
		else if (frame == 20)
		{
			h.frontend.m_flip_rotate_mode = 2;
			h.frontend.m_flip_rotate_angle = 90;
		}
		else if (frame == 21 || frame == 22)
		{
			// Wait for the rotate radio row to resize the modal.
		}
		else if (frame == 23)
		{
			save_frame(renderer, "temp/smoke_image_flip_rotate_rotate.ppm");
			const ImVec2 ok = modal_button_position("Flip/Rotate###flip_rotate", 0);
			push_motion(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y));
		}
		else if (frame == 24)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate###flip_rotate", 0);
			push_button(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y), true);
		}
		else if (frame == 25)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate###flip_rotate", 0);
			push_button(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y), false);
		}
		// --- Pending selection is accepted before the image transform ---
		else if (frame == 27)
		{
			save_frame(renderer, "temp/smoke_image_flip_rotate_result.ppm");
			REQUIRE(app.new_document(4, 2, error));
			REQUIRE(session.document().apply_pixel_patch(
				PaintCore::Rect{0, 0, 4, 2},
				{1, 2, 0, 0,
				 0, 0, 0, 0}));
			session.clear_history();
			REQUIRE(session.begin_selection(
				PaintCore::Rect{0, 0, 2, 2}, 0));
			REQUIRE(session.move_floating_region(1, 0));
		}
		else if (frame >= 29 && frame <= 34)
		{
			open_image_flip_rotate(window, frame - 29);
		}
		else if (frame == 35 || frame == 36)
		{
			// Wait for the default modal to settle.
		}
		else if (frame == 37)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate###flip_rotate", 0);
			push_motion(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y));
		}
		else if (frame == 38)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate###flip_rotate", 0);
			push_button(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y), true);
		}
		else if (frame == 39)
		{
			const ImVec2 ok = modal_button_position("Flip/Rotate###flip_rotate", 0);
			push_button(window, static_cast<int>(ok.x),
			            static_cast<int>(ok.y), false);
		}
		else if (frame == 41)
		{
			save_frame(renderer, "temp/smoke_image_flip_rotate_pending.ppm");
			running = false;
		}

		h.backend.poll_events();
		h.imgui.begin_frame(h.backend);
		h.frontend.render();
		h.backend.render_imgui(h.imgui.end_frame());

		if (frame == 10)
		{
			CHECK(popup_open_by_name("Flip/Rotate###flip_rotate"));
			CHECK(h.frontend.m_flip_rotate_target ==
			      PaintFrontend::FlipRotateTarget::Canvas);
		}
		else if (frame == 13)
		{
			CHECK_FALSE(popup_open_by_name("Flip/Rotate###flip_rotate"));
			CHECK_EQ(session.getWidth(), 4);
			CHECK_EQ(session.getHeight(), 2);
			CHECK(session.get_main_buffer().data() == pattern);
			CHECK_FALSE(session.can_undo());
		}
		else if (frame == 26)
		{
			CHECK_FALSE(popup_open_by_name("Flip/Rotate###flip_rotate"));
			CHECK_EQ(session.getWidth(), 2);
			CHECK_EQ(session.getHeight(), 4);
			CHECK(session.get_main_buffer().data() ==
			      std::vector<PaintCore::ColorIndex>(
			              {5, 1, 6, 2, 7, 3, 8, 4}));

			REQUIRE(session.undo());
			CHECK_EQ(session.getWidth(), 4);
			CHECK_EQ(session.getHeight(), 2);
			CHECK(session.get_main_buffer().data() == pattern);
			REQUIRE(session.redo());
			CHECK_EQ(session.getWidth(), 2);
			CHECK_EQ(session.getHeight(), 4);
			CHECK(session.get_main_buffer().data() ==
			      std::vector<PaintCore::ColorIndex>(
			              {5, 1, 6, 2, 7, 3, 8, 4}));
			// Back to the 4x2 pattern for the pending-selection scenario.
			REQUIRE(session.undo());
		}
		else if (frame == 35)
		{
			CHECK(popup_open_by_name("Flip/Rotate###flip_rotate"));
			CHECK(h.frontend.m_flip_rotate_target ==
			      PaintFrontend::FlipRotateTarget::Canvas);
		}
		else if (frame == 40)
		{
			CHECK_FALSE(popup_open_by_name("Flip/Rotate###flip_rotate"));
			CHECK_FALSE(session.has_floating_region());
			CHECK_EQ(session.getWidth(), 4);
			CHECK_EQ(session.getHeight(), 2);
			CHECK(session.get_main_buffer().data() ==
			      std::vector<PaintCore::ColorIndex>(
			              {0, 2, 1, 0,
			               0, 0, 0, 0}));

			// First undo removes the image flip; the second removes the
			// selection commit.
			REQUIRE(session.undo());
			CHECK(session.get_main_buffer().data() ==
			      std::vector<PaintCore::ColorIndex>(
			              {0, 1, 2, 0,
			               0, 0, 0, 0}));
			REQUIRE(session.undo());
			CHECK(session.get_main_buffer().data() ==
			      std::vector<PaintCore::ColorIndex>(
			              {1, 2, 0, 0,
			               0, 0, 0, 0}));
			CHECK_FALSE(session.can_undo());
			REQUIRE(session.redo());
			REQUIRE(session.redo());
			CHECK(session.get_main_buffer().data() ==
			      std::vector<PaintCore::ColorIndex>(
			              {0, 2, 1, 0,
			               0, 0, 0, 0}));
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
