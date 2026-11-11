#include "paint/png_image.h"

#include <cstddef>
#include <cstdint>

// Keep the decoder private to this translation unit. ImGuiFileDialog can also
// compile stb_image when thumbnails are enabled; STB_IMAGE_STATIC avoids
// duplicate symbols in that configuration.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb/stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace PaintCore
{
namespace
{
constexpr int kMaxImageDimension = 16384;
constexpr std::size_t kMaxPixelCount =
	static_cast<std::size_t>(64) * 1024 * 1024;
} // namespace

bool load_png_rgba(const std::string& path, RgbaImage& image,
                   std::string& error)
{
	image = {};
	error.clear();

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
	if (!data)
	{
		const char* reason = stbi_failure_reason();
		error = "Could not load image '" + path + "': " +
		        (reason ? reason : "unknown error");
		return false;
	}

	if (width <= 0 || height <= 0 ||
	    width > kMaxImageDimension || height > kMaxImageDimension ||
	    static_cast<std::size_t>(width) * static_cast<std::size_t>(height) >
		    kMaxPixelCount)
	{
		stbi_image_free(data);
		error = "Image dimensions are not supported: " +
		        std::to_string(width) + "x" + std::to_string(height);
		return false;
	}

	const std::size_t count =
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	image.pixels.resize(count);
	for (std::size_t i = 0; i < count; ++i)
	{
		const std::size_t offset = i * 4;
		image.pixels[i] = Color{data[offset], data[offset + 1],
		                        data[offset + 2], data[offset + 3]};
	}

	image.width = width;
	image.height = height;
	stbi_image_free(data);
	return true;
}
} // namespace PaintCore
