#include "paint/image_transform.h"

namespace PaintCore
{
ImageBuffer transform_image(const ImageBuffer& source,
                            ImageTransform transform)
{
	if (!is_valid_image_transform(transform) ||
	    source.width() <= 0 || source.height() <= 0)
	{
		return source;
	}

	const int old_width = source.width();
	const int old_height = source.height();
	const bool swap_axes = image_transform_swaps_axes(transform);
	const int new_width = swap_axes ? old_height : old_width;
	const int new_height = swap_axes ? old_width : old_height;

	ImageBuffer transformed(new_width, new_height, source.bitsPerPixel());
	for (int y = 0; y < old_height; ++y)
	{
		for (int x = 0; x < old_width; ++x)
		{
			int new_x = x;
			int new_y = y;
			switch (transform)
			{
				case ImageTransform::FlipHorizontal:
					new_x = old_width - 1 - x;
					break;
				case ImageTransform::FlipVertical:
					new_y = old_height - 1 - y;
					break;
				case ImageTransform::Rotate90:
					new_x = old_height - 1 - y;
					new_y = x;
					break;
				case ImageTransform::Rotate180:
					new_x = old_width - 1 - x;
					new_y = old_height - 1 - y;
					break;
				case ImageTransform::Rotate270:
					new_x = y;
					new_y = old_width - 1 - x;
					break;
			}
			transformed.set_pixel(new_x, new_y, source.get_pixel(x, y));
		}
	}

	return transformed;
}
} // namespace PaintCore
