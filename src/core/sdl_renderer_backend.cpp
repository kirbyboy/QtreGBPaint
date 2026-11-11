#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "core/sdl_renderer_backend.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

namespace
{
bool position_is_visible(int x, int y, int width, int height)
{
	const int display_count = SDL_GetNumVideoDisplays();
	if (display_count <= 0)
	{
		// Headless or unusual backend: let SDL/window manager decide.
		return true;
	}

	SDL_Rect window_rect{x, y, width, height};
	for (int display = 0; display < display_count; ++display)
	{
		SDL_Rect bounds{};
		if (SDL_GetDisplayBounds(display, &bounds) != 0)
		{
			continue;
		}
		SDL_Rect overlap{};
		if (SDL_IntersectRect(&window_rect, &bounds, &overlap) &&
		    overlap.w >= 32 && overlap.h >= 32)
		{
			return true;
		}
	}
	return false;
}

SDL_Cursor* create_color_cursor(const PaintCore::CursorImage& image)
{
	if (image.width <= 0 || image.height <= 0)
	{
		return nullptr;
	}
	const std::size_t expected =
		static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
	if (image.pixels.size() != expected)
	{
		return nullptr;
	}
	if (image.hotspot_x < 0 || image.hotspot_x >= image.width ||
	    image.hotspot_y < 0 || image.hotspot_y >= image.height)
	{
		return nullptr;
	}

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
		0, image.width, image.height, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!surface)
	{
		return nullptr;
	}
	if (SDL_LockSurface(surface) != 0)
	{
		SDL_FreeSurface(surface);
		return nullptr;
	}

	for (int y = 0; y < image.height; ++y)
	{
		auto* row = reinterpret_cast<std::uint32_t*>(
			static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch);
		for (int x = 0; x < image.width; ++x)
		{
			const PaintCore::Color& color =
				image.pixels[static_cast<std::size_t>(y) * image.width + x];
			row[x] = (static_cast<std::uint32_t>(color.a) << 24) |
			         (static_cast<std::uint32_t>(color.r) << 16) |
			         (static_cast<std::uint32_t>(color.g) << 8) |
			         static_cast<std::uint32_t>(color.b);
		}
	}

	SDL_UnlockSurface(surface);
	SDL_Cursor* cursor =
		SDL_CreateColorCursor(surface, image.hotspot_x, image.hotspot_y);
	SDL_FreeSurface(surface);
	return cursor;
}
}

SDLRendererBackend::SDLRendererBackend()
{}

void SDLRendererBackend::init()
{
	init_with_geometry(PaintCore::WindowGeometry{});
}

void SDLRendererBackend::init_with_geometry(
	const PaintCore::WindowGeometry& geometry)
{
	if(!ImGui::GetCurrentContext())
	{
		throw std::runtime_error("ImGui context required before backend init");
	}

	// Wayland uses the application id to find the desktop entry; X11 uses
	// WM_CLASS. SDL reads these environment variables for the class name.
	SDL_setenv("SDL_VIDEO_X11_WMCLASS",
	           "io.github.kirbyboy.QtreGBPaint", 1);
	SDL_setenv("SDL_VIDEO_WAYLAND_WMCLASS",
	           "io.github.kirbyboy.QtreGBPaint", 1);
	SDL_SetHint(SDL_HINT_APP_NAME, "QtreGBPaint");

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		throw std::runtime_error("SDL could not init");
	}

	const int width = std::max(1, geometry.width);
	const int height = std::max(1, geometry.height);
	int x = SDL_WINDOWPOS_CENTERED;
	int y = SDL_WINDOWPOS_CENTERED;
	if (geometry.has_position &&
	    position_is_visible(geometry.x, geometry.y, width, height))
	{
		x = geometry.x;
		y = geometry.y;
	}

	// Start hidden so the stored geometry is applied before the first frame.
	m_window = SDL_CreateWindow("QtreGBPaint", x, y, width, height,
	                            SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
	if(!m_window)
	{
		throw std::runtime_error("SDL could not create window");
	}
	m_minimum_width = 0;
	m_minimum_height = 0;
	m_normal_geometry = geometry;
	m_normal_geometry.width = width;
	m_normal_geometry.height = height;
	m_normal_geometry.maximized = false;
	m_normal_geometry.has_position =
		geometry.has_position && x == geometry.x && y == geometry.y;

	m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!m_renderer)
	{
		throw std::runtime_error("SDL could not create renderer");
	}

	if (!ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer))
	{
		throw std::runtime_error("ImGui SDL2 backend could not init");
	}
	if (!ImGui_ImplSDLRenderer2_Init(m_renderer))
	{
		throw std::runtime_error("ImGui SDL renderer backend could not init");
	}
	m_imgui_ready = true;

	// The backend owns the OS cursor from now on. This keeps ImGui from
	// overwriting the tool cursors set by the paint frontend.
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	create_cursors();

	if (geometry.maximized)
	{
		SDL_MaximizeWindow(m_window);
	}
	SDL_ShowWindow(m_window);

	// Query the final state. When not maximized, this also records the actual
	// centered position for the first time the app runs.
	update_normal_geometry();
}

PaintCore::WindowGeometry SDLRendererBackend::window_geometry() const
{
	PaintCore::WindowGeometry result = m_normal_geometry;
	result.maximized = m_window_maximized;
	return result;
}

void SDLRendererBackend::update_normal_geometry()
{
	if (!m_window)
	{
		return;
	}

	const Uint32 flags = SDL_GetWindowFlags(m_window);
	m_window_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
	if (m_window_maximized || (flags & SDL_WINDOW_MINIMIZED) ||
	    (flags & SDL_WINDOW_HIDDEN))
	{
		return;
	}

	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	SDL_GetWindowPosition(m_window, &x, &y);
	SDL_GetWindowSize(m_window, &width, &height);
	if (width <= 0 || height <= 0)
	{
		return;
	}

	m_normal_geometry.x = x;
	m_normal_geometry.y = y;
	m_normal_geometry.width = width;
	m_normal_geometry.height = height;
	m_normal_geometry.maximized = false;
	m_normal_geometry.has_position = true;
}

void SDLRendererBackend::begin_imgui_frame()
{
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	update_cursor();
}

void SDLRendererBackend::render_imgui(ImDrawData* drawData)
{
	// Apply the cursor chosen by the widgets rendered this frame. Doing it
	// here also catches ImGui's own cursor changes (text input, sliders, ...).
	update_cursor();
	ImGui_ImplSDLRenderer2_RenderDrawData(drawData, m_renderer);
	SDL_RenderPresent(m_renderer);
}

void SDLRendererBackend::set_minimum_window_size(int width, int height)
{
	if (!m_window || width <= 0 || height <= 0)
	{
		return;
	}
	if (width == m_minimum_width && height == m_minimum_height)
	{
		return;
	}

	m_minimum_width = width;
	m_minimum_height = height;
	SDL_SetWindowMinimumSize(m_window, width, height);

	int current_width = 0;
	int current_height = 0;
	SDL_GetWindowSize(m_window, &current_width, &current_height);
	if (current_width < width || current_height < height)
	{
		SDL_SetWindowSize(m_window,
			current_width < width ? width : current_width,
			current_height < height ? height : current_height);
	}
}

void SDLRendererBackend::set_cursor(PaintCore::CursorType cursor)
{
	if (m_requested_cursor == cursor)
	{
		return;
	}

	m_requested_cursor = cursor;
	update_cursor();
}

bool SDLRendererBackend::set_cursor_image(PaintCore::CursorType cursor,
                                          const PaintCore::CursorImage& image)
{
	const auto index = static_cast<std::size_t>(cursor);
	if (index >= m_tool_cursors.size())
	{
		return false;
	}

	SDL_Cursor* replacement = create_color_cursor(image);
	if (!replacement)
	{
		return false;
	}

	SDL_Cursor* previous = m_tool_cursors[index];
	m_tool_cursors[index] = replacement;
	if (m_last_cursor == previous)
	{
		m_last_cursor = nullptr;
		update_cursor();
	}
	if (previous)
	{
		SDL_FreeCursor(previous);
	}
	return true;
}

void SDLRendererBackend::set_window_icon(const PaintCore::Color* pixels,
                                         int width, int height)
{
	if (!m_window || !pixels || width <= 0 || height <= 0)
	{
		return;
	}

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
		0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
	if (!surface)
	{
		return;
	}

	if (SDL_ConvertPixels(width, height, SDL_PIXELFORMAT_RGBA32, pixels,
	                      width * static_cast<int>(sizeof(PaintCore::Color)),
	                      SDL_PIXELFORMAT_RGBA32, surface->pixels,
	                      surface->pitch) != 0)
	{
		SDL_FreeSurface(surface);
		return;
	}

	SDL_SetWindowIcon(m_window, surface);
	if (m_window_icon)
	{
		SDL_FreeSurface(m_window_icon);
	}
	m_window_icon = surface;
}

void SDLRendererBackend::poll_events()
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		if(event.type == SDL_QUIT)
		{
			m_quit_requested = true;
		}
		else if(event.type == SDL_WINDOWEVENT)
		{
			// The window manager may reset the cursor when the pointer or
			// focus enters the window. Forget the cache so update_cursor()
			// applies the requested cursor again.
			if(event.window.event == SDL_WINDOWEVENT_ENTER ||
			   event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
			   event.window.event == SDL_WINDOWEVENT_SHOWN)
			{
				m_last_cursor = nullptr;
			}
		}
	}

	// Keep the last non-maximized geometry for AppConfig. This runs once per
	// frame after the window-manager events have been processed.
	update_normal_geometry();
}

void SDLRendererBackend::shutdown()
{
	destroy_cursors();

	if(m_imgui_ready)
	{
		ImGui_ImplSDLRenderer2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		m_imgui_ready = false;
	}

	if(m_renderer)
	{
		for (auto& entry : m_textures)
		{
			SDL_DestroyTexture(entry.second);
		}
		m_textures.clear();

		SDL_DestroyRenderer(m_renderer);
		m_renderer = nullptr;
	}
	if(m_window)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}
	if(m_window_icon)
	{
		SDL_FreeSurface(m_window_icon);
		m_window_icon = nullptr;
	}
	SDL_Quit();
}

void *SDLRendererBackend::createTexture(int width, int height)
{
	SDL_Texture* texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!texture)
	{
		return nullptr;
	}
	if (SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) != 0 ||
	    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0)
	{
		SDL_DestroyTexture(texture);
		return nullptr;
	}
	void* id = reinterpret_cast<void*>(texture);
	m_textures[id] = texture;
	return id;
}

void SDLRendererBackend::updateTexture(void *textureId, const PaintCore::Color *pixels, int width, int height)
{
	updateTextureRegion(textureId, pixels, 0, 0, width, height);
}

void SDLRendererBackend::updateTextureRegion(void *textureId, const PaintCore::Color *pixels,
                                             int x, int y, int width, int height)
{
	if(!pixels || width <= 0 || height <= 0)
	{
		return;
	}

	auto it = m_textures.find(textureId);
	if(it == m_textures.end())
	{
		return;
	}

	SDL_Texture* texture = it->second;
	SDL_Rect region{x, y, width, height};
	void* dst;
	int pitch;
	if(SDL_LockTexture(texture, &region, &dst, &pitch) != 0)
	{
		return;
	}

	// SDL_PIXELFORMAT_RGBA32 means the bytes in memory are R,G,B,A.
	// We write channel by channel instead of memcpy so the pitch and the
	// endianness never matter.
	uint8_t* bytes = static_cast<uint8_t*>(dst);
	for(int row = 0; row < height; ++row)
	{
		uint8_t* row_ptr = bytes + static_cast<size_t>(row) * static_cast<size_t>(pitch);
		for(int col = 0; col < width; ++col)
		{
			const PaintCore::Color& color = pixels[static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col)];
			row_ptr[col * 4 + 0] = color.r;
			row_ptr[col * 4 + 1] = color.g;
			row_ptr[col * 4 + 2] = color.b;
			row_ptr[col * 4 + 3] = color.a;
		}
	}

	SDL_UnlockTexture(texture);
}

void SDLRendererBackend::destroyTexture(void *textureId)
{
	auto it = m_textures.find(textureId);
	if(it != m_textures.end())
	{
		SDL_DestroyTexture(it->second);
		m_textures.erase(it);
	}
}

void SDLRendererBackend::create_cursors()
{
	destroy_cursors();

	auto create_tool_cursor = [this](PaintCore::CursorType type, SDL_SystemCursor shape)
	{
		m_tool_cursors[static_cast<std::size_t>(type)] = SDL_CreateSystemCursor(shape);
	};

	create_tool_cursor(PaintCore::CursorType::Arrow, SDL_SYSTEM_CURSOR_ARROW);
	create_tool_cursor(PaintCore::CursorType::Crosshair, SDL_SYSTEM_CURSOR_CROSSHAIR);
	// Drawing tools fall back to the default system cursor until a valid .ipx
	// replaces the slot. Crosshair is the shared precision cursor.
	create_tool_cursor(PaintCore::CursorType::Pencil, SDL_SYSTEM_CURSOR_ARROW);
	create_tool_cursor(PaintCore::CursorType::Brush, SDL_SYSTEM_CURSOR_CROSSHAIR);
	create_tool_cursor(PaintCore::CursorType::Eraser, SDL_SYSTEM_CURSOR_ARROW);
	create_tool_cursor(PaintCore::CursorType::Pick, SDL_SYSTEM_CURSOR_CROSSHAIR);
	create_tool_cursor(PaintCore::CursorType::Fill, SDL_SYSTEM_CURSOR_ARROW);
	create_tool_cursor(PaintCore::CursorType::ResizeEW, SDL_SYSTEM_CURSOR_SIZEWE);
	create_tool_cursor(PaintCore::CursorType::ResizeNS, SDL_SYSTEM_CURSOR_SIZENS);
	create_tool_cursor(PaintCore::CursorType::ResizeNWSE, SDL_SYSTEM_CURSOR_SIZENWSE);
	create_tool_cursor(PaintCore::CursorType::Move, SDL_SYSTEM_CURSOR_SIZEALL);

	m_imgui_cursors.assign(ImGuiMouseCursor_COUNT, nullptr);
	auto create_imgui_cursor = [this](ImGuiMouseCursor type, SDL_SystemCursor shape)
	{
		m_imgui_cursors[static_cast<std::size_t>(type)] = SDL_CreateSystemCursor(shape);
	};

	create_imgui_cursor(ImGuiMouseCursor_Arrow, SDL_SYSTEM_CURSOR_ARROW);
	create_imgui_cursor(ImGuiMouseCursor_TextInput, SDL_SYSTEM_CURSOR_IBEAM);
	create_imgui_cursor(ImGuiMouseCursor_ResizeAll, SDL_SYSTEM_CURSOR_SIZEALL);
	create_imgui_cursor(ImGuiMouseCursor_ResizeNS, SDL_SYSTEM_CURSOR_SIZENS);
	create_imgui_cursor(ImGuiMouseCursor_ResizeEW, SDL_SYSTEM_CURSOR_SIZEWE);
	create_imgui_cursor(ImGuiMouseCursor_ResizeNESW, SDL_SYSTEM_CURSOR_SIZENESW);
	create_imgui_cursor(ImGuiMouseCursor_ResizeNWSE, SDL_SYSTEM_CURSOR_SIZENWSE);
	create_imgui_cursor(ImGuiMouseCursor_Hand, SDL_SYSTEM_CURSOR_HAND);
	create_imgui_cursor(ImGuiMouseCursor_Wait, SDL_SYSTEM_CURSOR_WAIT);
	create_imgui_cursor(ImGuiMouseCursor_Progress, SDL_SYSTEM_CURSOR_WAITARROW);
	create_imgui_cursor(ImGuiMouseCursor_NotAllowed, SDL_SYSTEM_CURSOR_NO);

	m_last_cursor = nullptr;
}

void SDLRendererBackend::destroy_cursors()
{
	if (m_window)
	{
		SDL_SetCursor(nullptr);
	}
	m_last_cursor = nullptr;

	for (SDL_Cursor*& cursor : m_tool_cursors)
	{
		if (cursor)
		{
			SDL_FreeCursor(cursor);
			cursor = nullptr;
		}
	}

	for (SDL_Cursor*& cursor : m_imgui_cursors)
	{
		if (cursor)
		{
			SDL_FreeCursor(cursor);
			cursor = nullptr;
		}
	}
	m_imgui_cursors.clear();
}

void SDLRendererBackend::update_cursor()
{
	if (!m_window)
	{
		return;
	}

	SDL_Cursor* cursor = nullptr;
	if (m_requested_cursor != PaintCore::CursorType::Arrow)
	{
		cursor = tool_cursor(m_requested_cursor);
	}
	else
	{
		// No tool override (or the default tool): keep ImGui's own cursor
		// behavior for text fields, resize handles, etc.
		const int imgui_cursor = ImGui::GetMouseCursor();
		if (imgui_cursor >= 0 && imgui_cursor < static_cast<int>(m_imgui_cursors.size()))
		{
			cursor = m_imgui_cursors[static_cast<std::size_t>(imgui_cursor)];
		}
		if (!cursor)
		{
			cursor = tool_cursor(PaintCore::CursorType::Arrow);
		}
	}

	if (cursor && cursor != m_last_cursor)
	{
		SDL_SetCursor(cursor);
		m_last_cursor = cursor;
	}
}

SDL_Cursor* SDLRendererBackend::tool_cursor(PaintCore::CursorType cursor) const
{
	const auto index = static_cast<std::size_t>(cursor);
	if (index >= m_tool_cursors.size())
	{
		return m_tool_cursors[static_cast<std::size_t>(PaintCore::CursorType::Arrow)];
	}
	return m_tool_cursors[index];
}
