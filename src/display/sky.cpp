#include "display/sky.h"
#include "globals.h"
#include "raylib.h"
#include <cstdio>

static RenderTexture2D circleMask;

void InitSky() {
	circleMask = LoadRenderTexture(screenRes, screenRes);

	BeginTextureMode(circleMask);
	ClearBackground(BLACK);
	DrawCircle(screenRes / 2, screenRes / 2, screenRes / 2.0f, WHITE);
	EndTextureMode();
}

void DrawSky() {
	// TODO: replace with actual sky rendering (star field, etc.)
	Rectangle src = {0, 0, (float)screenRes, -(float)screenRes};
	Rectangle dst = {0, 0, (float)screenRes, (float)screenRes};
	DrawRectangle(0, 0, screenRes, screenRes, BLACK);

	BeginBlendMode(BLEND_MULTIPLIED);
	DrawTexturePro(circleMask.texture, src, dst, {0, 0}, 0, WHITE);
	EndBlendMode();
}

void CleanupSky() {
	UnloadRenderTexture(circleMask);
}

void DrawDSO(float x, float y, uint16_t catalog_number) {
	if (x * x + y * y > 1.0f) return;

	float half = screenRes / 2.0f;
	float sx = half + x * half;
	float sy = half + y * half;

	DrawCircleV({sx, sy}, 3.0f, displayColor);

	char label[8];
	snprintf(label, sizeof(label), "%u", catalog_number);
	DrawTextEx(monoFont, label, {sx + 6.0f, sy - 8.0f}, 20.0f, 0.0f, DimColor());
}
