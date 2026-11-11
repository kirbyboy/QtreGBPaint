#ifndef QTREGBPAINT_WINDOW_GEOMETRY_H
#define QTREGBPAINT_WINDOW_GEOMETRY_H

namespace PaintCore
{
// Pixel geometry of the application window. `has_position` is false when the
// window manager should center the window (for example on first run or when
// the stored position is no longer on any display).
struct WindowGeometry
{
	int x = 0;
	int y = 0;
	int width = 800;
	int height = 600;
	bool maximized = false;
	bool has_position = false;
};
}

#endif
