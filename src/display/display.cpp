#include "display/display.h"
#include "display/directions.h"
#include "globals.h"
#include "resource_dir.h"
#include <cmath>

static Texture block_m;

void InitDisplay() {
	SearchAndSetResourceDir("resources");
	block_m = LoadTexture("block_m.png");
	InitDirections();
}

void DrawFrame() {
	BeginDrawing();
	ClearBackground(BLACK);

	DrawText("Hello Raylib", 200, 200, 40, displayColor);
	DrawTextureEx(block_m, {400, 200}, 0, 0.15f, displayColor);
	DrawDirectionsHud();
	DrawCircleLines(screenRes / 2, screenRes / 2, (float)screenRes / 2 + 1, displayColor);

	EndDrawing();
}

void CleanupDisplay() {
	UnloadTexture(block_m);
}
