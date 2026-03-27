#include "display/hud.h"
#include "display/arrow.h"
#include "protocol/shared_state.h"
#include "globals.h"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>
#include <mutex>

// ---- Mock data (replace with real sensor input) ----
static float mock_direction = 0.0f;
static float mock_distance_deg = 45.0f;
static float mock_time = 0.0f;

static void UpdateMockData() {
	mock_time += GetFrameTime();
	mock_direction = fmodf(mock_time * 0.5f, 2.0f * PI);
	mock_distance_deg = 20.0f + 15.0f * sinf(mock_time * 0.3f);
}
// ---- End mock data ----

static constexpr float HUD_PADDING = 16.0f;
static uint32_t last_imu_count = 0;
static constexpr float ARROW_RADIUS = 36.0f;
static constexpr float PING_FLASH_DURATION = 0.5f;

static uint32_t last_debug_count = 0;
static float ping_flash_timer = 0.0f;

void InitHud() {
	mock_direction = 0.0f;
	mock_distance_deg = 45.0f;
	mock_time = 0.0f;
}

static void DrawConnectionIndicator() {
	uint32_t current_debug_count;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		current_debug_count = g_shared_state.debug_update_count;
	}

	if (current_debug_count != last_debug_count) {
		last_debug_count = current_debug_count;
		ping_flash_timer = PING_FLASH_DURATION;
	}

	ping_flash_timer -= GetFrameTime();
	if (ping_flash_timer < 0.0f) ping_flash_timer = 0.0f;

	float x = screenRes - HUD_PADDING - 8.0f;
	float y = screenRes - HUD_PADDING - 8.0f;

	Color dot_color = (ping_flash_timer > 0.0f) ? GREEN : DARKGRAY;
	DrawCircleV({x, y}, 6.0f, dot_color);
}

static void DrawBaseCompassHeading() {
	float heading_deg;
	uint32_t current_imu_count;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		heading_deg = static_cast<float>(g_shared_state.imu.heading) / 16.0f;
		current_imu_count = g_shared_state.imu_update_count;
	}

	bool has_data = (current_imu_count > 0);
	Color text_color = has_data ? displayColor : DARKGRAY;

	char buf[16];
	if (has_data) {
		snprintf(buf, sizeof(buf), "%03.0f", heading_deg);
	} else {
		snprintf(buf, sizeof(buf), "---");
	}

	int fontSize = 20;
	int textW = MeasureText(buf, fontSize);
	float x = HUD_PADDING;
	float y = screenRes - HUD_PADDING - fontSize;
	DrawText(buf, static_cast<int>(x), static_cast<int>(y), fontSize, text_color);

	DrawText("deg", static_cast<int>(x + textW + 4), static_cast<int>(y + 4), 14, text_color);
}

void DrawHud() {
	UpdateMockData();

	float half = screenRes / 2.0f;
	float circleR = half;

	// Direction arrow in top-right corner outside the circle
	float cornerInset = half * (1.0f - 0.707f) * 0.5f + HUD_PADDING;
	float cx = screenRes - cornerInset - 10.0f;
	float cy = cornerInset + 10.0f;

	Vector2 center = {half, half};
	Vector2 hudCenter = {cx, cy};
	float distToCenter = Vector2Distance(hudCenter, center);
	if (distToCenter + ARROW_RADIUS + 4.0f > circleR) {
		Vector2 dir = Vector2Normalize(Vector2Subtract(hudCenter, center));
		float maxDist = circleR - ARROW_RADIUS - 4.0f;
		hudCenter = Vector2Add(center, Vector2Scale(dir, maxDist));
		cx = hudCenter.x;
		cy = hudCenter.y;
	}

	float halfLen = ARROW_RADIUS * 0.7f;
	Vector2 arrowStart = {
		cx - halfLen * cosf(mock_direction),
		cy - halfLen * sinf(mock_direction)
	};
	Vector2 arrowEnd = {
		cx + halfLen * cosf(mock_direction),
		cy + halfLen * sinf(mock_direction)
	};
	DrawArrow(arrowStart, arrowEnd, 2.5f, 12.0f, displayColor);

	// Distance counter below the arrow
	char distBuf[32];
	snprintf(distBuf, sizeof(distBuf), "%.1f deg", mock_distance_deg);
	int fontSize = 16;
	int textW = MeasureText(distBuf, fontSize);
	DrawText(distBuf, (int)(cx - textW / 2.0f), (int)(cy + ARROW_RADIUS + 4.0f),
	         fontSize, displayColor);

	// Circle border
	DrawCircleLines(screenRes / 2, screenRes / 2, circleR + 1, displayColor);

	DrawConnectionIndicator();
	DrawBaseCompassHeading();
}
