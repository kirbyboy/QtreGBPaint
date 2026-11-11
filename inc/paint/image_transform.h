#ifndef QTREGBPAINT_PAINT_IMAGE_TRANSFORM_H
#define QTREGBPAINT_PAINT_IMAGE_TRANSFORM_H

#include "core/image_transform.h"
#include "paint/image_buffer.h"

namespace PaintCore
{
// Returns a new image with the same bits per pixel and the pixel permutation
// described by `transform`. Width and height swap for 90/270.
ImageBuffer transform_image(const ImageBuffer& source,
                            ImageTransform transform);
} // namespace PaintCore

#endif
