#ifndef QTREGBPAINT_BEZIER_H
#define QTREGBPAINT_BEZIER_H

#include <algorithm>
#include <array>
#include <cmath>

#include "core/line.h"
#include "core/vec2.h"

namespace PaintCore
{
struct CubicBezier
{
	Vec2 p0{};
	Vec2 p1{};
	Vec2 p2{};
	Vec2 p3{};
};

// Evaluates the cubic Bezier at t in [0, 1].
inline Vec2f cubic_bezier_point(const CubicBezier& curve, float t)
{
	const float u = 1.0f - t;
	const float uu = u * u;
	const float tt = t * t;
	const float uuu = uu * u;
	const float ttt = tt * t;

	return Vec2f{
		uuu * static_cast<float>(curve.p0.x) +
			3.0f * uu * t * static_cast<float>(curve.p1.x) +
			3.0f * u * tt * static_cast<float>(curve.p2.x) +
			ttt * static_cast<float>(curve.p3.x),
		uuu * static_cast<float>(curve.p0.y) +
			3.0f * uu * t * static_cast<float>(curve.p1.y) +
			3.0f * u * tt * static_cast<float>(curve.p2.y) +
			ttt * static_cast<float>(curve.p3.y),
	};
}

// Calls plot(x, y) for the pixels of a one-pixel cubic Bezier stroke. The
// curve is subdivided adaptively until each piece is flat enough to be drawn
// as one integer line. Long straight or gently curved sections become a single
// segment, so the stroke keeps a uniform one-pixel width and does not get the
// short overlapping runs produced by dense fixed-step sampling.
template <typename Plot>
void rasterize_cubic_bezier(const CubicBezier& curve, Plot&& plot)
{
	using Points = std::array<Vec2f, 4>;
	constexpr double kFlatness = 0.5;
	constexpr int kMaxDepth = 16;

	const auto midpoint = [](const Vec2f& a, const Vec2f& b)
	{
		return Vec2f{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
	};

	const Points points{
		Vec2f{static_cast<float>(curve.p0.x),
		      static_cast<float>(curve.p0.y)},
		Vec2f{static_cast<float>(curve.p1.x),
		      static_cast<float>(curve.p1.y)},
		Vec2f{static_cast<float>(curve.p2.x),
		      static_cast<float>(curve.p2.y)},
		Vec2f{static_cast<float>(curve.p3.x),
		      static_cast<float>(curve.p3.y)},
	};

	const auto is_flat = [](const Points& p)
	{
		const Vec2f chord{p[3].x - p[0].x, p[3].y - p[0].y};
		const double length2 =
			static_cast<double>(chord.x) * chord.x +
			static_cast<double>(chord.y) * chord.y;

		const auto projection = [&](const Vec2f& point)
		{
			if (length2 <= 1e-9)
			{
				return 0.0;
			}
			return (static_cast<double>(point.x - p[0].x) * chord.x +
			        static_cast<double>(point.y - p[0].y) * chord.y) /
			       length2;
		};

		const auto distance = [&](const Vec2f& point)
		{
			if (length2 <= 1e-9)
			{
				const double dx = point.x - p[0].x;
				const double dy = point.y - p[0].y;
				return std::sqrt(dx * dx + dy * dy);
			}
			const double cross =
				static_cast<double>(point.x - p[0].x) * chord.y -
				static_cast<double>(point.y - p[0].y) * chord.x;
			return std::abs(cross) / std::sqrt(length2);
		};

		const double t1 = projection(p[1]);
		const double t2 = projection(p[2]);
		return std::max(distance(p[1]), distance(p[2])) <= kFlatness &&
		       t1 >= 0.0 && t1 <= 1.0 &&
		       t2 >= 0.0 && t2 <= 1.0;
	};

	const auto emit_line = [&](const Vec2f& from, const Vec2f& to)
	{
		rasterize_line(
			static_cast<int>(std::lround(from.x)),
			static_cast<int>(std::lround(from.y)),
			static_cast<int>(std::lround(to.x)),
			static_cast<int>(std::lround(to.y)),
			plot);
	};

	const auto subdivide = [&](auto&& self, const Points& p, int depth) -> void
	{
		if (depth >= kMaxDepth || is_flat(p))
		{
			emit_line(p[0], p[3]);
			return;
		}

		const Vec2f p01 = midpoint(p[0], p[1]);
		const Vec2f p12 = midpoint(p[1], p[2]);
		const Vec2f p23 = midpoint(p[2], p[3]);
		const Vec2f p012 = midpoint(p01, p12);
		const Vec2f p123 = midpoint(p12, p23);
		const Vec2f p0123 = midpoint(p012, p123);

		const Points left{p[0], p01, p012, p0123};
		const Points right{p0123, p123, p23, p[3]};
		self(self, left, depth + 1);
		self(self, right, depth + 1);
	};

	subdivide(subdivide, points, 0);
}
}

#endif
