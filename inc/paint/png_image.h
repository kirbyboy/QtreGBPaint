#ifndef QTREGBPAINT_PAINT_PNG_IMAGE_H
#define QTREGBPAINT_PAINT_PNG_IMAGE_H

#include <string>
#include <vector>

#include "core/color.h"

namespace PaintCore
{
struct RgbaImage
{
	int width = 0;
	int height = 0;
	std::vector<Color> pixels;
};

// Decodes a PNG file into RGBA8. The decoder is stb_image, already vendored
// with ImGuiFileDialog; no SDL_image or platform codec is required.
bool load_png_rgba(const std::string& path, RgbaImage& image,
                   std::string& error);
} // namespace PaintCore

#endif
