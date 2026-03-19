#include "display/display.h"
#include "display/sky.h"
#include "display/hud.h"
#include "globals.h"
#include "raylib.h"
#include "resource_dir.h"
#include "state_machine/state_machine.h"

static RenderTexture2D framebuffer;

void InitDisplay() {
	SearchAndSetResourceDir("resources");
	framebuffer = LoadRenderTexture(screenRes, screenRes);
	InitSky();
	InitHud();
}

static void DrawScene() {
	switch (GetTelescopeState()) {
	case TelescopeState::INIT:
		DrawHud();
		DrawText("Init", screenRes / 2, screenRes / 2, 40, WHITE);
		break;
	case TelescopeState::SETUP:
		DrawHud();
		DrawText("Setup", screenRes / 2, screenRes / 2, 40, WHITE);
		break;
	case TelescopeState::IDLE:
		DrawHud();
		DrawSky();
		DrawText("Idle", screenRes / 2, screenRes / 2, 40, WHITE);
		break;
	case TelescopeState::SEARCH:
		DrawSky();
		DrawHud();
		DrawText("Search", screenRes / 2, screenRes / 2, 40, WHITE);
		break;
	case TelescopeState::FOUND:
		DrawSky();
		DrawHud();
		DrawText("Found", screenRes / 2, screenRes / 2, 40, WHITE);
		break;
	}
}

void DrawFrame() {
	BeginTextureMode(framebuffer);
	ClearBackground(BLACK);
	DrawScene();
	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);
	// Flip vertically: positive height in src = mirrored across x-axis
	// (RenderTextures are already y-flipped in OpenGL, so using positive
	// height flips it an extra time, giving the mirror effect)
	Rectangle src = {0, 0, (float)screenRes, (float)screenRes};
	Rectangle dst = {0, 0, (float)screenRes, (float)screenRes};
	DrawTexturePro(framebuffer.texture, src, dst, {0, 0}, 0, WHITE);
	EndDrawing();
}

void CleanupDisplay() {
	CleanupSky();
	UnloadRenderTexture(framebuffer);
}
