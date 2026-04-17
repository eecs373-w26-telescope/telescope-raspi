#include "display/sky.h"
#include "globals.h"
#include "raylib.h"

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
