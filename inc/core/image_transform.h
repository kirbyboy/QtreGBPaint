#ifndef QTREGBPAINT_CORE_IMAGE_TRANSFORM_H
#define QTREGBPAINT_CORE_IMAGE_TRANSFORM_H

namespace PaintCore
{
// Pixel permutations that are their own inverse or whose inverse is another
// 90-degree rotation. No resampling or scaling is involved.
enum class ImageTransform
{
	FlipHorizontal,
	FlipVertical,
	Rotate90,
	Rotate180,
	Rotate270,
};

inline bool image_transform_swaps_axes(ImageTransform transform)
{
	return transform == ImageTransform::Rotate90 ||
	       transform == ImageTransform::Rotate270;
}

inline bool is_valid_image_transform(ImageTransform transform)
{
	switch (transform)
	{
		case ImageTransform::FlipHorizontal:
		case ImageTransform::FlipVertical:
		case ImageTransform::Rotate90:
		case ImageTransform::Rotate180:
		case ImageTransform::Rotate270:
			return true;
	}
	return false;
}

// 90 and 270 are clockwise; their inverse is the opposite quarter turn.
inline ImageTransform inverse_image_transform(ImageTransform transform)
{
	switch (transform)
	{
		case ImageTransform::FlipHorizontal:
			return ImageTransform::FlipHorizontal;
		case ImageTransform::FlipVertical:
			return ImageTransform::FlipVertical;
		case ImageTransform::Rotate90:
			return ImageTransform::Rotate270;
		case ImageTransform::Rotate180:
			return ImageTransform::Rotate180;
		case ImageTransform::Rotate270:
			return ImageTransform::Rotate90;
	}
	return transform;
}
} // namespace PaintCore

#endif
