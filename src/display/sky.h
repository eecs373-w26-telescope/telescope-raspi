#pragma once

#include <cstdint>

void InitSky();
void DrawSky();
void CleanupSky();

// Draw a DSO dot and catalog label at normalized screen coords [-1, 1].
// Silently clips if the position falls outside the unit circle.
void DrawDSO(float x, float y, uint16_t catalog_number);
