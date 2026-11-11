#ifndef QTREGBPAINT_WINDOW_TITLE_H
#define QTREGBPAINT_WINDOW_TITLE_H

#include <string>

namespace PaintCore
{
// Application name shown in the OS window and in the ImGui title bar.
inline constexpr const char* kApplicationName = "QtreGBPaint";

// Visible main-window title: "*<name> - QtreGBPaint" while the document has
// unsaved changes. The frontend appends a stable "###" suffix so the ImGui
// window ID does not change when the visible title does.
inline std::string make_window_title(const std::string& display_name,
                                     bool modified)
{
	std::string title;
	if (modified)
	{
		title += '*';
	}
	title += display_name;
	title += " - ";
	title += kApplicationName;
	return title;
}
} // namespace PaintCore

#endif
