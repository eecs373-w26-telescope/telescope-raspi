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
	TelescopeState state = GetTelescopeState();

	if (state == TelescopeState::IDLE || state == TelescopeState::SEARCH || state == TelescopeState::FOUND) {
		DrawSky();
	}

	if (IsOverlayVisible()) {
		DrawHud();
	}
}

void DrawFrame() {
	UpdateStateMachine();

	BeginTextureMode(framebuffer);
	ClearBackground(BLACK);
	DrawScene();
	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);
	float src_h = DISPLAY_FLIP ? (float)screenRes : -(float)screenRes;
	Rectangle src = {0, 0, (float)screenRes, src_h};
	Rectangle dst = {0, 0, (float)screenRes, (float)screenRes};
	DrawTexturePro(framebuffer.texture, src, dst, {0, 0}, 0, WHITE);
	EndDrawing();
}

void CleanupDisplay() {
	CleanupSky();
	UnloadRenderTexture(framebuffer);
}
