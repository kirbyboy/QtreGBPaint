#ifndef QTREGBPAINT_ELLIPSE_H
#define QTREGBPAINT_ELLIPSE_H

#include <algorithm>
#include <cmath>

namespace PaintCore
{
struct EllipseSpan
{
	int x0 = 0;
	int x1 = -1;
};

// Horizontal span of the ellipse inscribed in the inclusive pixel rectangle
// [left..right] x [top..bottom] for row `y`. Both endpoints are inclusive.
//
// The ellipse is sampled at pixel centers. Rows where the ellipse is thinner
// than one pixel fall back to the center pixel(s), so even-width shapes keep
// connected top/bottom caps. For even heights the two central rows are treated
// as the equator, which makes the widest part touch the left and right edges.
inline EllipseSpan ellipse_span_for_row(int left, int top, int right,
                                        int bottom, int y)
{
	if (right < left || bottom < top || y < top || y > bottom)
	{
		return {};
	}

	const int width = right - left + 1;
	const int height = bottom - top + 1;
	const double radius_x = width * 0.5;
	const double radius_y = height * 0.5;
	const double center_x = left + radius_x;
	const double center_y = top + radius_y;

	double dy = (y + 0.5) - center_y;

	// With an even height there is no row exactly at the vertical center.
	// Treat the two central rows as the equator so the ellipse reaches both
	// the left and right edges of the bounding box.
	if ((height % 2) == 0 &&
	    (y == top + height / 2 - 1 || y == top + height / 2))
	{
		dy = 0.0;
	}

	const double normalized = dy / radius_y;
	double inside = 1.0 - normalized * normalized;
	if (inside < 0.0)
	{
		inside = 0.0;
	}

	const double dx = radius_x * std::sqrt(inside);
	constexpr double kEpsilon = 1e-9;
	int x0 = static_cast<int>(std::ceil(center_x - dx - 0.5 - kEpsilon));
	int x1 = static_cast<int>(std::floor(center_x + dx - 0.5 + kEpsilon));

	if (x0 > x1)
	{
		// The ellipse is thinner than one pixel here. Use the center
		// pixel(s): one for odd widths, two for even widths.
		x0 = left + (width - 1) / 2;
		x1 = left + width / 2;
	}

	return {std::max(left, x0), std::min(right, x1)};
}

// Calls span(y, x0, x1) for every row of the ellipse, top to bottom. This is
// the fill primitive: one horizontal run per row, with no overdraw and no
// per-pixel square root. A future bucket fill can reuse the same run-filling
// idea with a scanline flood fill: expand the seed horizontally, fill that
// run, then enqueue the overlapping runs above and below.
template <typename Span>
void rasterize_ellipse_spans(int left, int top, int right, int bottom,
                             Span&& span)
{
	if (right < left || bottom < top)
	{
		return;
	}

	for (int y = top; y <= bottom; ++y)
	{
		const EllipseSpan row =
			ellipse_span_for_row(left, top, right, bottom, y);
		span(y, row.x0, row.x1);
	}
}

// Calls plot(x, y) for every pixel of the one-pixel ellipse outline. The
// outline is derived from the same spans as the fill, so Outline, Fill and
// Both always agree. A point may be reported more than once in the narrow
// tips; drawing callbacks are expected to be idempotent.
template <typename Plot>
void rasterize_ellipse_outline(int left, int top, int right, int bottom,
                               Plot&& plot)
{
	if (right < left || bottom < top)
	{
		return;
	}

	EllipseSpan previous =
		ellipse_span_for_row(left, top, right, bottom, top);

	// The top cap is the whole first span.
	for (int x = previous.x0; x <= previous.x1; ++x)
	{
		plot(x, top);
	}
	if (top == bottom)
	{
		return;
	}

	for (int y = top + 1; y <= bottom; ++y)
	{
		const EllipseSpan current =
			ellipse_span_for_row(left, top, right, bottom, y);

		// Horizontal steps of the left border. Draw them on the wider row
		// so the outline stays connected and vertically symmetric.
		if (current.x0 < previous.x0)
		{
			for (int x = current.x0 + 1; x <= previous.x0; ++x)
			{
				plot(x, y);
			}
		}
		else if (current.x0 > previous.x0)
		{
			for (int x = previous.x0 + 1; x <= current.x0; ++x)
			{
				plot(x, y - 1);
			}
		}

		// Horizontal steps of the right border.
		if (current.x1 > previous.x1)
		{
			for (int x = previous.x1; x <= current.x1 - 1; ++x)
			{
				plot(x, y);
			}
		}
		else if (current.x1 < previous.x1)
		{
			for (int x = current.x1; x <= previous.x1 - 1; ++x)
			{
				plot(x, y - 1);
			}
		}

		// The bottom row is drawn as a full cap after the loop.
		if (y != bottom)
		{
			plot(current.x0, y);
			if (current.x1 != current.x0)
			{
				plot(current.x1, y);
			}
		}

		previous = current;
	}

	// Bottom cap.
	for (int x = previous.x0; x <= previous.x1; ++x)
	{
		plot(x, bottom);
	}
}
}

#endif
