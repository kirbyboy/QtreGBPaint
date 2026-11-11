// Smoke test for the canvas resize handle placement rules.
//
// Build from the repository root:
//   g++ -std=c++17 -Iinc -Ilib/imgui/src -Ilib/imgui/src/backends \
//       $(sdl2-config --cflags) tests/smoke/smoke_resize_handles_edges.cpp \
//       $(find obj -name '*.o' ! -path '*/main.o') \
//       lib/imgui/libimgui.a lib/tinyfiledialogs/libtinyfiledialog.a \
//       $(sdl2-config --libs) -o temp/smoke_resize_handles_edges

#include <SDL.h>

#include <cmath>
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
			std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
		} \
	} while (0)

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

	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
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

ImGuiWindow* find_canvas_window()
{
	for (ImGuiWindow* candidate : ImGui::GetCurrentContext()->Windows)
	{
		if (std::strstr(candidate->Name, "/Canvas_") != nullptr)
		{
			return candidate;
		}
	}
	return nullptr;
}

void canvas_clip(ImVec2& clip_min, ImVec2& clip_max)
{
	ImGuiWindow* window = find_canvas_window();
	const ImGuiStyle& style = ImGui::GetStyle();
	clip_min = ImVec2(window->InnerRect.Min.x + style.WindowPadding.x,
	                  window->InnerRect.Min.y + style.WindowPadding.y);
	clip_max = ImVec2(window->InnerRect.Max.x - style.WindowPadding.x,
	                  window->InnerRect.Max.y - style.WindowPadding.y);
}

void set_scroll(float x, float y)
{
	if (ImGuiWindow* window = find_canvas_window())
	{
		ImGui::SetScrollX(window, x);
		ImGui::SetScrollY(window, y);
	}
}

void set_scroll_max(bool x, bool y)
{
	if (ImGuiWindow* window = find_canvas_window())
	{
		ImGui::SetScrollX(window, x ? window->ScrollMax.x : 0.0f);
		ImGui::SetScrollY(window, y ? window->ScrollMax.y : 0.0f);
	}
}

CanvasResizeGizmo::Handle handle_at(Harness& h, float x, float y)
{
	ImVec2 clip_min;
	ImVec2 clip_max;
	canvas_clip(clip_min, clip_max);
	return h.frontend.m_canvas_area.m_resize_gizmo.handle_at(
		h.frontend.m_canvas_area.m_view,
		h.frontend.m_canvas_area.session(),
		ImVec2(x, y), clip_min, clip_max);
}

struct EdgeGeometry
{
	float origin_x = 0.0f;
	float origin_y = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	float visible_top = 0.0f;
	float visible_bottom = 0.0f;
	float visible_left = 0.0f;
	float visible_right = 0.0f;
};

EdgeGeometry edge_geometry(Harness& h)
{
	ImVec2 clip_min;
	ImVec2 clip_max;
	canvas_clip(clip_min, clip_max);

	PaintSession& canvas = h.frontend.m_canvas_area.session();
	const PaintCore::ViewTransform& view = h.frontend.m_canvas_area.m_view;
	const PaintCore::Vec2f origin = view.image_to_screen({0.0f, 0.0f});
	const PaintCore::Vec2f end = view.image_to_screen(
		{static_cast<float>(canvas.getWidth()),
		 static_cast<float>(canvas.getHeight())});

	EdgeGeometry g;
	g.origin_x = origin.x;
	g.origin_y = origin.y;
	g.right = end.x;
	g.bottom = end.y;
	g.visible_top = std::max(origin.y, clip_min.y);
	g.visible_bottom = std::min(end.y, clip_max.y);
	g.visible_left = std::max(origin.x, clip_min.x);
	g.visible_right = std::min(end.x, clip_max.x);
	return g;
}

int run()
{
	Harness h;
	h.imgui.init();
	h.backend.init();

	h.frontend.m_zoom = 64;
	h.frontend.m_show_grid = true;

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
	SDL_SetWindowSize(window, 665, 360);

	bool running = true;
	int frame = 0;
	const Uint32 frame_ms = 1000 / 60;

	while (running)
	{
		if (frame == 2)
		{
			// No canvas edge is visible at scroll 0 with zoom 64, so no handle
			// should be reachable anywhere.
			ImVec2 clip_min;
			ImVec2 clip_max;
			canvas_clip(clip_min, clip_max);
			CHECK(handle_at(h, clip_max.x - 2.0f,
			                (clip_min.y + clip_max.y) * 0.5f) ==
			      CanvasResizeGizmo::Handle::None);
			CHECK(handle_at(h, (clip_min.x + clip_max.x) * 0.5f,
			                clip_max.y - 2.0f) ==
			      CanvasResizeGizmo::Handle::None);
			CHECK(handle_at(h, clip_max.x - 2.0f, clip_max.y - 2.0f) ==
			      CanvasResizeGizmo::Handle::None);
		}
		else if (frame == 3)
		{
			set_scroll_max(true, false);
		}
		else if (frame == 5)
		{
			// Right edge reached: only the right handle appears, centered on
			// the visible vertical span.
			const EdgeGeometry g = edge_geometry(h);
			const float center_y = (g.visible_top + g.visible_bottom) * 0.5f;
			CHECK(handle_at(h, g.right + 5.0f, center_y) ==
			      CanvasResizeGizmo::Handle::Right);
			CHECK(handle_at(h, (g.visible_left + g.visible_right) * 0.5f,
			                g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::None);
			CHECK(std::fabs(g.visible_bottom - g.visible_top) > 16.0f);

			ImVec2 clip_min;
			ImVec2 clip_max;
			canvas_clip(clip_min, clip_max);
			CHECK(std::fabs(g.right - (clip_max.x - 12.0f)) < 1.0f);
			save_frame(renderer, "temp/smoke_resize_edges_right.ppm");
		}
		else if (frame == 6)
		{
			set_scroll_max(true, true);
		}
		else if (frame == 8)
		{
			// Corner reached: right, bottom and corner handles are available.
			const EdgeGeometry g = edge_geometry(h);
			const float center_x = (g.visible_left + g.visible_right) * 0.5f;
			const float center_y = (g.visible_top + g.visible_bottom) * 0.5f;
			CHECK(handle_at(h, g.right + 5.0f, center_y) ==
			      CanvasResizeGizmo::Handle::Right);
			CHECK(handle_at(h, center_x, g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::Bottom);
			CHECK(handle_at(h, g.right + 5.0f, g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::Corner);

			// Edge handles are 1 px outside the image and centered.
			ImVec2 clip_min;
			ImVec2 clip_max;
			canvas_clip(clip_min, clip_max);
			const PaintCore::ViewTransform& view =
				h.frontend.m_canvas_area.m_view;
			const PaintCore::Vec2f end = view.image_to_screen(
				{static_cast<float>(h.frontend.m_canvas_area.session().getWidth()),
				 static_cast<float>(h.frontend.m_canvas_area.session().getHeight())});
			CHECK(std::fabs(end.x - (clip_max.x - 12.0f)) < 1.0f);
			CHECK(std::fabs(end.y - (clip_max.y - 12.0f)) < 1.0f);
			save_frame(renderer, "temp/smoke_resize_edges_corner.ppm");
		}
		else if (frame == 9)
		{
			h.frontend.m_canvas_area.session().resize(8, 8, 0);
			h.frontend.m_zoom = 1;
			set_scroll(0.0f, 0.0f);
		}
		else if (frame == 11)
		{
			// Canvas shorter/wider than the handle: edge handles are hidden,
			// the corner handle remains available.
			const EdgeGeometry g = edge_geometry(h);
			CHECK(handle_at(h, g.right + 5.0f,
			                (g.visible_top + g.visible_bottom) * 0.5f) ==
			      CanvasResizeGizmo::Handle::None);
			CHECK(handle_at(h, (g.visible_left + g.visible_right) * 0.5f,
			                g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::None);
			CHECK(handle_at(h, g.right + 5.0f, g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::Corner);
			save_frame(renderer, "temp/smoke_resize_edges_small.ppm");
		}
		else if (frame == 12)
		{
			h.frontend.m_canvas_area.session().resize(24, 24, 0);
			set_scroll(0.0f, 0.0f);
		}
		else if (frame == 14)
		{
			// Small but tall/wide enough for the edge handles: all three
			// handles are visible and the edge ones are centered.
			const EdgeGeometry g = edge_geometry(h);
			CHECK(handle_at(h, g.right + 5.0f,
			                (g.visible_top + g.visible_bottom) * 0.5f) ==
			      CanvasResizeGizmo::Handle::Right);
			CHECK(handle_at(h, (g.visible_left + g.visible_right) * 0.5f,
			                g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::Bottom);
			CHECK(handle_at(h, g.right + 5.0f, g.bottom + 5.0f) ==
			      CanvasResizeGizmo::Handle::Corner);
			save_frame(renderer, "temp/smoke_resize_edges_small_all.ppm");
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
}

int main()
{
	return run();
}
