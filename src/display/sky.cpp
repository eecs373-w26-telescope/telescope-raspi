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

	// Test DSOs at various positions to validate label placement
	DrawDSO( 0.0f,   0.0f,  1);   // center
	DrawDSO( 0.85f,  0.0f,  31);  // right edge
	DrawDSO(-0.85f,  0.0f,  42);  // left edge
	DrawDSO( 0.0f,  -0.85f, 45);  // top
	DrawDSO( 0.0f,   0.85f, 57);  // bottom
	DrawDSO( 0.6f,   0.6f,  81);  // bottom-right
	DrawDSO(-0.6f,   0.6f,  101); // bottom-left
	DrawDSO( 0.6f,  -0.6f,  13);  // top-right
	DrawDSO(-0.6f,  -0.6f,  97);  // top-left
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
	float label_w = MeasureTextEx(monoFont, label, 32.0f, 0.0f).x;
	float label_x = (x > 0.0f) ? sx - 14.0f - label_w : sx + 14.0f;
	DrawTextEx(monoFont, label, {label_x, sy - 16.0f}, 32.0f, 0.0f, displayColor);
}
