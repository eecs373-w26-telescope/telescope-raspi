#include "display/display.h"
#include "display/sky.h"
#include "display/hud.h"
#include "globals.h"
#include "raylib.h"
#include "resource_dir.h"
#include "state_machine/state_machine.h"

void InitDisplay() {
	SearchAndSetResourceDir("resources");
	InitSky();
	InitHud();
}

void DrawFrame() {
	BeginDrawing();
	ClearBackground(BLACK);

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

	EndDrawing();
}

void CleanupDisplay() {
	CleanupSky();
}
