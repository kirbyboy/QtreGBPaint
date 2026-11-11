// Smoke test with injected SDL events for the palette/drawing connection.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_palette.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_palette

#include <SDL.h>

#include <algorithm>
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

// Test access to the frontend's internals (the project is not modified).
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

void push_button(SDL_Window* window, int x, int y, bool down,
                 Uint8 button = SDL_BUTTON_LEFT)
{
	push_motion(window, x, y, down ? SDL_BUTTON(button) : 0);

	SDL_Event event{};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.timestamp = SDL_GetTicks();
	event.button.windowID = SDL_GetWindowID(window);
	event.button.which = 0;
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = x;
	event.button.y = y;
	if (SDL_PushEvent(&event) != 1)
	{
		std::cerr << "SDL_PushEvent(button) failed\n";
	}
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

struct PaletteGeometry
{
	bool valid = false;
	ImVec2 cells[32]{};
};

PaletteGeometry palette_geometry(const PaintFrontend& frontend)
{
	PaletteGeometry result;

	ImGuiWindow* window = nullptr;
	for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
	{
		if (std::strstr(candidate->Name, "/Palette") != nullptr)
		{
			window = candidate;
			break;
		}
	}
	if (!window)
	{
		return result;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const int count = frontend.m_backend.palette_visual_count();
	const int rows = count <= 16 ? 1 : 2;
	const int columns = (count + rows - 1) / rows;

	const float gridWidth =
		10.0f * 50.0f + 9.0f * style.ItemSpacing.x;
	const float cellWidth =
		(gridWidth - (columns - 1) * style.ItemSpacing.x) / columns;
	const float cellHeight = std::max(
		1.0f, (50.0f - (rows - 1) * style.ItemSpacing.y) / rows);

	const ImVec2 contentStart(
		window->Pos.x + style.WindowPadding.x,
		window->Pos.y + style.WindowPadding.y);
	const ImVec2 gridPos(
		contentStart.x + 50.0f + style.ItemSpacing.x,
		contentStart.y);

	for (int i = 0; i < count; ++i)
	{
		const int column = i % columns;
		const int row = i / columns;
		result.cells[i] = ImVec2(
			gridPos.x + column * (cellWidth + style.ItemSpacing.x) + cellWidth * 0.5f,
			gridPos.y + row * (cellHeight + style.ItemSpacing.y) + cellHeight * 0.5f);
	}

	result.valid = true;
	return result;
}

bool same_color(const PaintCore::Color& a, const PaintCore::Color& b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int run()
{
	Harness h;
	h.imgui.init();
	h.backend.init();

	std::string asset_base = "assets";
	if (char* basePath = SDL_GetBasePath())
	{
		asset_base = std::string(basePath) + "../assets";
		SDL_free(basePath);
	}

	const Palette& gray = h.resources.iconPalette();
	h.resources.addButton("Rect", std::make_shared<SquareTool>(1, 2),
	                      PaintCore::CursorType::Crosshair,
	                      asset_base + "/rect_button.idx", gray);

	const std::string rectOptions = asset_base + "/rect_options.idx";
	h.resources.loadOptionIcon("rect.outline", rectOptions, 0, 0, 60, 16);
	h.resources.loadOptionIcon("rect.fill", rectOptions, 0, 16, 60, 16);
	h.resources.loadOptionIcon("rect.both", rectOptions, 0, 32, 60, 16);

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

	const Palette dmg = Palette::dmg();
	for (int i = 0; i < 4; ++i)
	{
		CHECK(same_color(h.resources.palette().getColor(
			static_cast<PaintCore::ColorIndex>(i)),
			dmg.getColor(static_cast<PaintCore::ColorIndex>(i))));
	}
	CHECK(h.resources.palette_visual_count() == 4);
	CHECK(h.resources.foreground_index() == 3);
	CHECK(h.resources.background_index() == 0);

	PaletteGeometry geometry{};
	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 1)
		{
			geometry = palette_geometry(h.frontend);
			CHECK(geometry.valid);
			CHECK(geometry.cells[0].x > 0.0f);
			CHECK(geometry.cells[0].y > 0.0f);
		}
		else if (frame == 2)
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
			CHECK(h.resources.activeButton() == "Rect");
			CHECK(h.frontend.m_canvas_area.session().active_tool() != nullptr);
			CHECK(h.resources.foreground_index() == 3);
			CHECK(h.resources.background_index() == 0);
			push_motion(window, 55, 136, 0);
		}
		else if (frame == 6)
		{
			save_frame(renderer, "temp/smoke_options.ppm");
			push_motion(window, static_cast<int>(geometry.cells[1].x),
			            static_cast<int>(geometry.cells[1].y), 0);
		}
		else if (frame == 7)
		{
			push_button(window, static_cast<int>(geometry.cells[1].x),
			            static_cast<int>(geometry.cells[1].y), true);
		}
		else if (frame == 8)
		{
			push_button(window, static_cast<int>(geometry.cells[1].x),
			            static_cast<int>(geometry.cells[1].y), false);
		}
		else if (frame == 9)
		{
			CHECK(h.resources.foreground_index() == 1);
		}
		else if (frame == 10)
		{
			push_motion(window, static_cast<int>(geometry.cells[2].x),
			            static_cast<int>(geometry.cells[2].y), 0);
		}
		else if (frame == 11)
		{
			push_button(window, static_cast<int>(geometry.cells[2].x),
			            static_cast<int>(geometry.cells[2].y), true, SDL_BUTTON_RIGHT);
		}
		else if (frame == 12)
		{
			push_button(window, static_cast<int>(geometry.cells[2].x),
			            static_cast<int>(geometry.cells[2].y), false, SDL_BUTTON_RIGHT);
		}
		else if (frame == 13)
		{
			CHECK(h.resources.background_index() == 2);
			if (ToolBase* tool = h.frontend.m_canvas_area.session().active_tool())
			{
				tool->set_option("mode", static_cast<int>(SquareTool::Mode::Both));
			}
		}
		else if (frame == 14)
		{
			push_motion(window, 168, 103, 0);
		}
		else if (frame == 15)
		{
			push_button(window, 168, 103, true);
		}
		else if (frame >= 16 && frame <= 26)
		{
			const int t = frame - 16;
			push_motion(window, 168 + 100 * t / 10, 103 + 80 * t / 10,
			            SDL_BUTTON_LMASK);
		}
		else if (frame == 27)
		{
			push_button(window, 268, 183, false);
		}
		else if (frame == 29)
		{
			const ImageBuffer& image = h.canvas().get_main_buffer();
			CHECK(image.get_pixel(56, 49) == 1);   // foreground border
			CHECK(image.get_pixel(106, 89) == 2);  // background interior
			CHECK(image.get_pixel(156, 129) == 1); // foreground border
		}
		else if (frame == 30)
		{
			save_frame(renderer, "temp/smoke_palette_1.ppm");
		}
		else if (frame == 32)
		{
			// Switch to a generated tone. With the current 2bpp document this
			// still shows four colors, lightest first and darkest last.
			h.resources.select_palette(PaletteMode::Green);

			CHECK(h.resources.palette_mode() == PaletteMode::Green);
			CHECK(h.resources.palette_visual_count() == 4);
			CHECK(h.resources.foreground_index() == 1);
			CHECK(h.resources.background_index() == 2);
			CHECK(h.resources.palette().getColor(0).g > 200);
			CHECK(h.resources.palette().getColor(3).g == 0);
		}
		else if (frame == 34)
		{
			geometry = palette_geometry(h.frontend);
			CHECK(geometry.valid);
			// Four colors fit in a single row.
			CHECK(geometry.cells[1].y == geometry.cells[0].y);
			CHECK(geometry.cells[1].x > geometry.cells[0].x);
		}
		else if (frame == 35)
		{
			push_motion(window, static_cast<int>(geometry.cells[1].x),
			            static_cast<int>(geometry.cells[1].y), 0);
		}
		else if (frame == 36)
		{
			push_button(window, static_cast<int>(geometry.cells[1].x),
			            static_cast<int>(geometry.cells[1].y), true);
		}
		else if (frame == 37)
		{
			push_button(window, static_cast<int>(geometry.cells[1].x),
			            static_cast<int>(geometry.cells[1].y), false);
		}
		else if (frame == 38)
		{
			CHECK(h.resources.foreground_index() == 1);
		}
		else if (frame == 39)
		{
			save_frame(renderer, "temp/smoke_palette_2.ppm");
		}
		else if (frame == 40)
		{
			h.resources.toggle_palette_inverted();

			CHECK(h.resources.palette_inverted());
			CHECK(h.resources.palette().getColor(0).g == 0);
			CHECK(h.resources.palette().getColor(3).g > 200);
			CHECK(h.resources.foreground_index() == 1);
			CHECK(h.resources.background_index() == 2);
		}
		else if (frame == 41)
		{
			save_frame(renderer, "temp/smoke_palette_inverted.ppm");
		}
		else if (frame == 42)
		{
			// Choosing a tone after inverting shows it light-to-dark again.
			h.resources.select_palette(PaletteMode::Blue);
			CHECK(!h.resources.palette_inverted());
			CHECK(h.resources.palette().getColor(0).r == 255);
			CHECK(h.resources.palette().getColor(0).g == 255);
			CHECK(h.resources.palette().getColor(0).b == 255);
			CHECK(h.resources.palette().getColor(3).b == 0);
			CHECK(h.resources.foreground_index() == 1);
			CHECK(h.resources.background_index() == 2);
		}
		else if (frame == 44)
		{
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
