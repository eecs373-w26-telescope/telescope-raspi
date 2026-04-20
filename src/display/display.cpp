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

	DrawHud();
}

void DrawFrame() {
	UpdateStateMachine();

	BeginTextureMode(framebuffer);
	ClearBackground(BLACK);
	DrawScene();
	EndTextureMode();

	BeginDrawing();
	ClearBackground(BLACK);
	
	// Left side (0 to screenRes): Primary monitor content
	// Uses existing DISPLAY_FLIP logic
	float src_h1 = DISPLAY_FLIP ? (float)screenRes : -(float)screenRes;
	DrawTexturePro(framebuffer.texture, 
		{0, 0, (float)screenRes, src_h1}, 
		{0, 0, (float)screenRes, (float)screenRes}, 
		{0, 0}, 0, WHITE);

	// Right side (screenRes to 2*screenRes): Secondary monitor content
	// Forced vertical flip as requested (opposite of the standard orientation)
	float src_h2 = (DISPLAY_FLIP == 0) ? (float)screenRes : -(float)screenRes;
	DrawTexturePro(framebuffer.texture, 
		{0, 0, (float)screenRes, src_h2}, 
		{(float)screenRes, 0, (float)screenRes, (float)screenRes}, 
		{0, 0}, 0, WHITE);

	EndDrawing();
}

void CleanupDisplay() {
	CleanupSky();
	UnloadRenderTexture(framebuffer);
}
