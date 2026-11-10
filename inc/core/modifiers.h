#ifndef QTREGBPAINT_MODIFIERS_H
#define QTREGBPAINT_MODIFIERS_H

namespace PaintCore
{
// Transient keyboard modifiers for the current frame. Tools read this state
// while drawing; it is not part of the tool options or the history.
struct Modifiers
{
	bool shift = false;
	bool ctrl = false;
	bool alt = false;
};
}

#endif
