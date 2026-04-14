#pragma once

#include "raylib.h"
#include <stdint.h>

extern int   screenRes;
extern Color displayColor;
extern uint8_t dimAlpha;
extern Font  monoFont;

inline Color DimColor() {
	return {displayColor.r, displayColor.g, displayColor.b, dimAlpha};
}
