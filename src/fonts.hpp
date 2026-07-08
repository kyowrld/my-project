#pragma once

#include "imgui.h"

// Font handles populated by main() at startup. Any of these may be null if a
// suitable font file could not be loaded, in which case callers should fall
// back to the default font.
namespace ui {
extern ImFont* fontRegular;  // body text
extern ImFont* fontMedium;   // labels / values
extern ImFont* fontLarge;    // headings
extern ImFont* fontHuge;     // brand title

// Pushes `font` if non-null and returns true (so the caller knows to pop).
bool pushFont(ImFont* font);
}  // namespace ui
