#ifndef QTREGBPAINT_IBACKEND_H
#define QTREGBPAINT_IBACKEND_H

#include <cstdint>

#include "core/color.h"

struct ImDrawData;

class IGuiBackend
{
	public:
		virtual ~IGuiBackend() = default;

		virtual void init() = 0;
		virtual void shutdown() = 0;

		virtual void begin_imgui_frame() = 0;
		virtual void render_imgui(ImDrawData* drawData) = 0;

		virtual void poll_events(bool& running) = 0;
		
		virtual void* createTexture(int width, int height) = 0;
		virtual void updateTexture(void* texture_id, const PaintCore::Color* pixels, int width, int height) = 0;
		virtual void updateTextureRegion(void* texture_id, const PaintCore::Color* pixels,
		                                 int x, int y, int width, int height) = 0;
		virtual void destroyTexture(void* textureId) = 0;
};

#endif
