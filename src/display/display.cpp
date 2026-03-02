#include "display/display.h"
#include "globals.h"
#include "display/arrow.h"
#include "resource_dir.h"
#include <cmath>

static Texture block_m;
static float direction = 0;

void InitDisplay() {
	SearchAndSetResourceDir("resources");
	block_m = LoadTexture("block_m.png");
}

void DrawFrame() {
	BeginDrawing();
	ClearBackground(BLACK);

	DrawText("Hello Raylib", 200, 200, 20, displayColor);
	DrawTextureEx(block_m, {400, 200}, 0, 0.15f, displayColor);
	DrawDirectionArrow(direction);
	DrawCircleLines(screenRes / 2, screenRes / 2, (float)screenRes / 2 + 1, displayColor);

	EndDrawing();

	direction = fmodf(direction + PI / 500, 2 * PI);
}

void CleanupDisplay() {
	UnloadTexture(block_m);
}
