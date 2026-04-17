#include "display/sky.h"
#include "globals.h"
#include "protocol/shared_state.h"
#include "raylib.h"
#include <cstdio>
#include <mutex>

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

	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		if (g_shared_state.fov_objects_received) {
			const FovObjectsPayload& fov = g_shared_state.fov_objects;
			for (uint8_t i = 0; i < fov.count; ++i) {
				float x = fov.objects[i].x_e4 / 10000.0f;
				float y = fov.objects[i].y_e4 / 10000.0f;
				DrawDSO(x, y, fov.objects[i].messier_id);
			}
		}
	}
}

void CleanupSky() {
	UnloadRenderTexture(circleMask);
}

void DrawDSO(float x, float y, uint16_t catalog_number) {
	if (x * x + y * y > 1.0f) return;

	float half = screenRes / 2.0f;
	float sx = half + x * half;
	float sy = half + y * half;

	DrawCircleV({sx, sy}, 3.0f, DisplayColor());

	char label[8];
	snprintf(label, sizeof(label), "%u", catalog_number);
	float label_w = MeasureTextEx(monoFont, label, 32.0f, 0.0f).x;
	float label_x = (x > 0.0f) ? sx - 14.0f - label_w : sx + 14.0f;
	DrawTextEx(monoFont, label, {label_x, sy - 16.0f}, 32.0f, 0.0f, DisplayColor());
}
