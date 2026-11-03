#include <SDL2/SDL.h>
#include <cstring>
#include <stdexcept>

#include "core/sdl_renderer_backend.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

SDLRendererBackend::SDLRendererBackend()
{}

void SDLRendererBackend::init()
{
	if(!ImGui::GetCurrentContext())
	{
		throw std::runtime_error("ImGui context required before backend init");
	}

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
	{
		throw std::runtime_error("SDL could not init");
	}	
	m_window = SDL_CreateWindow("QtreGB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if(!m_window)
	{
		throw std::runtime_error("SDL could not create window");
	}
	m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(!m_renderer)
	{
		throw std::runtime_error("SDL could not create renderer");
	}

	ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
	ImGui_ImplSDLRenderer2_Init(m_renderer);
	m_imgui_ready = true;
}

void SDLRendererBackend::begin_imgui_frame()
{
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
}

void SDLRendererBackend::render_imgui(ImDrawData* drawData)
{
	ImGui_ImplSDLRenderer2_RenderDrawData(drawData, m_renderer);
	SDL_RenderPresent(m_renderer);
}

void SDLRendererBackend::poll_events(bool& running)
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		if(event.type == SDL_QUIT)
		{
			running = false;
		}
	}
}

void SDLRendererBackend::shutdown()
{
	if(m_imgui_ready)
	{
		ImGui_ImplSDLRenderer2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		m_imgui_ready = false;
	}

	if(m_renderer)
	{
		SDL_DestroyRenderer(m_renderer);
		m_renderer = nullptr;
	}
	if(m_window)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
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
	void* id = reinterpret_cast<void*>(texture);
	m_textures[id] = texture;
	return id;
}

void SDLRendererBackend::updateTexture(void *textureId, const PaintCore::Color *pixels, int width, int height)
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
	void* dst;
	int pitch;
	if(SDL_LockTexture(texture, NULL, &dst, &pitch) != 0)
	{
		return;
	}

	// SDL_PIXELFORMAT_RGBA32 means the bytes in memory are R,G,B,A.
	// We write channel by channel instead of memcpy so the pitch and the
	// endianness never matter.
	uint8_t* bytes = static_cast<uint8_t*>(dst);
	for(int y = 0; y < height; ++y)
	{
		uint8_t* row = bytes + static_cast<size_t>(y) * static_cast<size_t>(pitch);
		for(int x = 0; x < width; ++x)
		{
			const PaintCore::Color& color = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
			row[x * 4 + 0] = color.r;
			row[x * 4 + 1] = color.g;
			row[x * 4 + 2] = color.b;
			row[x * 4 + 3] = color.a;
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
