#pragma once

#include "raylib.h"
#include <stdint.h>

extern int     screenRes;
extern Color   _displayColor;
extern uint8_t dimAlpha;
extern Font    monoFont;

inline Color DisplayColor() { return _displayColor; }
inline Color DimColor()     { return {_displayColor.r, _displayColor.g, _displayColor.b, dimAlpha}; }
