#ifndef QTREGBPAINT_IBACKEND_H
#define QTREGBPAINT_IBACKEND_H

#include <cstdint>

#include "core/color.h"
#include "core/cursor.h"

struct ImDrawData;

class IGuiBackend
{
	public:
		virtual ~IGuiBackend() = default;

		virtual void init() = 0;
		virtual void shutdown() = 0;

		virtual void begin_imgui_frame() = 0;
		virtual void render_imgui(ImDrawData* drawData) = 0;

		virtual void set_minimum_window_size(int width, int height) = 0;
		virtual void set_cursor(PaintCore::CursorType cursor) = 0;
		virtual bool set_cursor_image(PaintCore::CursorType cursor,
		                              const PaintCore::CursorImage& image) = 0;
		virtual void poll_events() = 0;

		// A window-manager close request is only a request: the frontend
		// decides whether to close immediately or to ask about unsaved changes.
		virtual bool quit_requested() const = 0;
		virtual void clear_quit_request() = 0;
		
		virtual void* createTexture(int width, int height) = 0;
		virtual void updateTexture(void* texture_id, const PaintCore::Color* pixels, int width, int height) = 0;
		virtual void updateTextureRegion(void* texture_id, const PaintCore::Color* pixels,
		                                 int x, int y, int width, int height) = 0;
		virtual void destroyTexture(void* textureId) = 0;
};

#endif
