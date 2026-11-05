// Tests headless de PaintCore::ViewTransform.
// Ejecutar con `make -C tests` o `make test` desde la raíz.

#include "doctest/doctest.h"

#include <cmath>

#include "core/rect.h"
#include "core/vec2.h"
#include "core/view_transform.h"

using PaintCore::Rect;
using PaintCore::Vec2;
using PaintCore::Vec2f;
using PaintCore::ViewTransform;

namespace
{
bool rect_eq(const Rect& a, const Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

#define CHECK_NEAR(actual, expected) \
	CHECK(std::fabs((actual) - (expected)) <= 1e-4f)

#define CHECK_RECT(actual, expected) \
	CHECK(rect_eq((actual), (expected)))

TEST_CASE("test_make_basic")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{10.0f, 20.0f}, Vec2f{0.0f, 0.0f}, Vec2f{100.0f, 80.0f}, 3.0f);

	CHECK_NEAR(view.origin.x, 10.0f);
	CHECK_NEAR(view.origin.y, 20.0f);
	CHECK_NEAR(view.scroll_px.x, 0.0f);
	CHECK_NEAR(view.scroll_px.y, 0.0f);
	CHECK_NEAR(view.viewport_size.x, 100.0f);
	CHECK_NEAR(view.viewport_size.y, 80.0f);
	CHECK_NEAR(view.zoom, 3.0f);
}

TEST_CASE("test_make_aligns_origin_and_keeps_fractional_scroll")
{
	// scroll 7.5 con zoom 3 => snapped 6, fracción 1.5; el origen se redondea
	// a píxel entero de pantalla.
	const ViewTransform view = ViewTransform::make(
		Vec2f{10.25f, 20.75f}, Vec2f{7.5f, 2.5f}, Vec2f{100.0f, 80.0f}, 3.0f);

	CHECK_NEAR(view.origin.x, 12.0f);  // floor(10.25 + 1.5 + 0.5)
	CHECK_NEAR(view.origin.y, 23.0f);  // floor(20.75 + 2.5 + 0.5)
	CHECK_NEAR(std::fmod(view.origin.x, 1.0f), 0.0f);
	CHECK_NEAR(std::fmod(view.origin.y, 1.0f), 0.0f);
}

TEST_CASE("test_make_clamps_invalid_zoom")
{
	const ViewTransform zero = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{0.0f, 0.0f}, Vec2f{10.0f, 10.0f}, 0.0f);
	const ViewTransform negative = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{0.0f, 0.0f}, Vec2f{10.0f, 10.0f}, -4.0f);

	CHECK_NEAR(zero.zoom, 1.0f);
	CHECK_NEAR(negative.zoom, 1.0f);
}

TEST_CASE("test_round_trip")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{-13.0f, 5.0f}, Vec2f{0.0f, 0.0f}, Vec2f{64.0f, 64.0f}, 4.0f);

	const Vec2f image{3.5f, 2.25f};
	const Vec2f screen = view.image_to_screen(image);
	const Vec2f back = view.screen_to_image(screen);

	CHECK_NEAR(screen.x, 1.0f);   // -13 + 3.5 * 4
	CHECK_NEAR(screen.y, 14.0f);  // 5 + 2.25 * 4
	CHECK_NEAR(back.x, image.x);
	CHECK_NEAR(back.y, image.y);
}

TEST_CASE("test_screen_to_image_pixel_floors")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{10.2f, 20.7f}, Vec2f{0.0f, 0.0f}, Vec2f{100.0f, 100.0f}, 2.0f);

	const Vec2 pixel = view.screen_to_image_pixel(Vec2f{15.0f, 20.0f});
	CHECK_EQ(pixel.x, 2);   // (15 - 10) / 2 = 2.5 -> 2
	CHECK_EQ(pixel.y, -1);  // (20 - 21) / 2 = -0.5 -> -1
}

TEST_CASE("test_screen_to_image_pixel_is_consistent_with_render")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{7.0f, 9.0f}, Vec2f{0.0f, 0.0f}, Vec2f{100.0f, 100.0f}, 5.0f);

	for (int iy = -2; iy < 12; ++iy)
	{
		for (int ix = -2; ix < 12; ++ix)
		{
			// Centro del píxel de imagen: debe volver al mismo píxel.
			const Vec2f center = view.image_to_screen(
				Vec2f{ix + 0.5f, iy + 0.5f});
			const Vec2 pixel = view.screen_to_image_pixel(center);
			CHECK_EQ(pixel.x, ix);
			CHECK_EQ(pixel.y, iy);
		}
	}
}

TEST_CASE("test_visible_rect_at_origin")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{0.0f, 0.0f}, Vec2f{10.0f, 8.0f}, 1.0f);

	CHECK_RECT(view.visible_image_rect(100, 100), (Rect{0, 0, 10, 8}));
}

TEST_CASE("test_visible_rect_with_zoom_and_scroll")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{20.0f, 10.0f}, Vec2f{30.0f, 20.0f}, 2.0f);

	// start = floor(scroll / zoom); tamaño visible = ceil(viewport / zoom).
	CHECK_RECT(view.visible_image_rect(100, 100), (Rect{10, 5, 15, 10}));
}

TEST_CASE("test_visible_rect_clips_right_and_bottom")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{30.0f, 30.0f}, Vec2f{20.0f, 20.0f}, 2.0f);

	CHECK_RECT(view.visible_image_rect(20, 20), (Rect{15, 15, 5, 5}));
}

TEST_CASE("test_visible_rect_empty_cases")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{100.0f, 100.0f}, Vec2f{20.0f, 20.0f}, 2.0f);

	CHECK(view.visible_image_rect(20, 20).empty());
	CHECK(view.visible_image_rect(0, 20).empty());
	CHECK(view.visible_image_rect(20, 0).empty());
	CHECK(view.visible_image_rect(-1, -1).empty());
}

TEST_CASE("test_visible_rect_negative_scroll_clamps_to_image")
{
	const ViewTransform view = ViewTransform::make(
		Vec2f{0.0f, 0.0f}, Vec2f{-5.0f, -5.0f}, Vec2f{10.0f, 10.0f}, 2.0f);

	CHECK_RECT(view.visible_image_rect(100, 100), (Rect{0, 0, 5, 5}));
}

TEST_CASE("test_visible_rect_covers_viewport")
{
	// canvas_pos incluye el -scroll (lo que devuelve GetCursorScreenPos).
	const ViewTransform view = ViewTransform::make(
		Vec2f{-13.0f, -7.0f}, Vec2f{13.0f, 7.0f}, Vec2f{40.0f, 30.0f}, 3.0f);
	const Rect visible = view.visible_image_rect(256, 256);

	CHECK(!visible.empty());
	// El píxel inicial visible se dibuja en la parte superior izquierda de la
	// ventana (con el desfase de alineado menor que un píxel de imagen).
	const Vec2f first = view.image_to_screen(
		Vec2f{static_cast<float>(visible.x), static_cast<float>(visible.y)});
	CHECK(first.x >= -1.0f);
	CHECK(first.x <= 1.0f);
	CHECK(first.y >= -1.0f);
	CHECK(first.y <= 1.0f);

	// El último píxel visible debe solaparse con el viewport.
	const Vec2f last = view.image_to_screen(
		Vec2f{static_cast<float>(visible.right() - 1),
			  static_cast<float>(visible.bottom() - 1)});
	CHECK(last.x < view.viewport_size.x);
	CHECK(last.y < view.viewport_size.y);
}
}
