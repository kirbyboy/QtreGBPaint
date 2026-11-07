#include "paint/cursor_asset.h"

#include <cstddef>
#include <string>
#include <utility>

#include "paint/indexed_image.h"

namespace
{
bool fail(std::string* error, std::string message)
{
	if (error)
	{
		*error = std::move(message);
	}
	return false;
}

PaintCore::Color cursor_color(int index)
{
	switch (index)
	{
		case 3: return {0, 0, 0, 255};       // black
		case 4: return {255, 255, 255, 255}; // white
		default: return {0, 0, 0, 0};        // transparent (0 and hotspot)
	}
}
}

namespace PaintCore
{
bool load_cursor_asset(const std::string& path, CursorImage& image,
                       std::string* error)
{
	if (error)
	{
		error->clear();
	}
	image = CursorImage{};

	IndexedImage source = IndexedImage::loadFromFile(path);
	if (!source.isValid())
	{
		return fail(error, source.error());
	}

	if (source.bitsPerPixel() < 3)
	{
		return fail(error, "Cursor assets require at least 3 bpp");
	}

	const auto& data = source.data();
	int hotspot_count = 0;
	int hotspot_x = 0;
	int hotspot_y = 0;

	for (int y = 0; y < source.height(); ++y)
	{
		for (int x = 0; x < source.width(); ++x)
		{
			const int index =
				data[static_cast<std::size_t>(y) * source.width() + x];
			switch (index)
			{
				case 0:
				case 1:
				case 3:
				case 4:
					break;
				default:
					return fail(error,
						"Unsupported cursor index " + std::to_string(index) +
						" at (" + std::to_string(x) + ", " +
						std::to_string(y) + ")");
			}

			if (index == 1)
			{
				++hotspot_count;
				hotspot_x = x;
				hotspot_y = y;
			}
		}
	}

	if (hotspot_count != 1)
	{
		return fail(error,
			"Cursor asset must contain exactly one hotspot (index 1)");
	}

	image.width = source.width();
	image.height = source.height();
	image.hotspot_x = hotspot_x;
	image.hotspot_y = hotspot_y;
	image.pixels.resize(data.size());

	for (std::size_t i = 0; i < data.size(); ++i)
	{
		image.pixels[i] = cursor_color(data[i]);
	}

	return true;
}
}
