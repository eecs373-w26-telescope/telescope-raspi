#include "display/directions.h"
#include "display/arrow.h"
#include "globals.h"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdio>

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
static constexpr float ARROW_RADIUS = 36.0f;

void InitDirections() {
	mock_direction = 0.0f;
	mock_distance_deg = 45.0f;
	mock_time = 0.0f;
}

void DrawDirectionsHud() {
	UpdateMockData();

	float half = screenRes / 2.0f;
	float circleR = half;

	// Top-right corner region center: midpoint of the corner triangle
	// The circle touches (screenRes, half) and (half, 0), so the corner
	// centroid sits roughly at (screenRes - cornerInset, cornerInset).
	float cornerInset = half * (1.0f - 0.707f) * 0.5f + HUD_PADDING;
	float cx = screenRes - cornerInset - 10.0f;
	float cy = cornerInset + 10.0f;

	// Clamp so the arrow circle doesn't overlap the viewport circle
	Vector2 center = {(float)screenRes / 2.0f, (float)screenRes / 2.0f};
	Vector2 hudCenter = {cx, cy};
	float distToCenter = Vector2Distance(hudCenter, center);
	if (distToCenter + ARROW_RADIUS + 4.0f > circleR) {
		Vector2 dir = Vector2Normalize(Vector2Subtract(hudCenter, center));
		float maxDist = circleR - ARROW_RADIUS - 4.0f;
		hudCenter = Vector2Add(center, Vector2Scale(dir, maxDist));
		cx = hudCenter.x;
		cy = hudCenter.y;
	}

	// Draw arrow showing which direction to move
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

	// Draw distance below the arrow
	char distBuf[32];
	snprintf(distBuf, sizeof(distBuf), "%.1f deg", mock_distance_deg);
	int fontSize = 16;
	int textW = MeasureText(distBuf, fontSize);
	DrawText(distBuf, (int)(cx - textW / 2.0f), (int)(cy + ARROW_RADIUS + 4.0f),
	         fontSize, displayColor);
}
