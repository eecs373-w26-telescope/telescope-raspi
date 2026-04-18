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
		
		uint16_t target_id = 0;
		bool has_active_target = g_shared_state.dso_target_received && (g_shared_state.dso_target.status == DSO_OK);
		if (has_active_target) {
			target_id = g_shared_state.dso_target.catalog_number;
		}

		if (g_shared_state.fov_objects_received) {
			const FovObjectsPayload& fov = g_shared_state.fov_objects;
			for (uint8_t i = 0; i < fov.count; ++i) {
				float x = fov.objects[i].x_e4 / 10000.0f;
				float y = fov.objects[i].y_e4 / 10000.0f;
				bool is_target = has_active_target && (fov.objects[i].messier_id == target_id);
				DrawDSO(x, y, fov.objects[i].messier_id, is_target);
			}
		}
	}
}

void CleanupSky() {
	UnloadRenderTexture(circleMask);
}

void DrawDSO(float x, float y, uint16_t catalog_number, bool is_target) {
	if (x * x + y * y > 1.0f) return;

	float half = screenRes / 2.0f;
	float sx = half + x * half;
	float sy = half + y * half;

	float dot_radius = is_target ? 6.0f : 4.0f;
	DrawCircleV({sx, sy}, dot_radius, DisplayColor());

	float font_size = is_target ? 60.0f : 40.0f;
	char label[8];
	snprintf(label, sizeof(label), "M%u", catalog_number);
	
	float label_w = MeasureTextEx(monoFont, label, font_size, 0.0f).x;
	float offset = is_target ? 20.0f : 14.0f;
	float label_x = (x > 0.0f) ? sx - offset - label_w : sx + offset;
	float label_y = sy - (font_size * 0.4f);

	DrawTextEx(monoFont, label, {label_x, label_y}, font_size, 0.0f, DisplayColor());
}
