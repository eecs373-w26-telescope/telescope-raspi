#include "display/hud.h"
#include "protocol/shared_state.h"
#include "state_machine/state_machine.h"
#include "globals.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>
#include <mutex>

// ---- Mock target data (replace with real search state) ----
static float mock_direction = 0.0f;
static float mock_distance_deg = 0.0f;
static float mock_time = 0.0f;
static bool mock_has_target = false;

static void UpdateMockData() {
	mock_time += GetFrameTime();
	if (IsKeyPressed(KEY_T)) mock_has_target = !mock_has_target;
	if (mock_has_target) {
		mock_direction = fmodf(mock_time * 0.5f, 2.0f * PI);
		mock_distance_deg = 20.0f + 15.0f * sinf(mock_time * 0.3f);
	}
}
// ---- End mock data ----

static constexpr float PAD = 3.0f;
static constexpr float PING_FLASH_DURATION = 0.5f;
static constexpr float FONT_S = 40.0f;
static constexpr float FONT_L = 44.0f;
static constexpr float FONT_XL = 50.0f;
static constexpr float SPACING = 0.0f;
static constexpr float LINE_THICK = 1.5f;

static Color dim_color;
static uint32_t last_debug_count = 0;
static float ping_flash_timer = 0.0f;

static void MonoText(const char* text, float x, float y, float size, Color color) {
	DrawTextEx(monoFont, text, {x, y}, size, SPACING, color);
}

static float MonoWidth(const char* text, float size) {
	return MeasureTextEx(monoFont, text, size, SPACING).x;
}

static void DrawArrow(Vector2 start, Vector2 end, float thickness, float headSize, Color color) {
	DrawLineEx(start, end, thickness, color);

	Vector2 direction = {end.x - start.x, end.y - start.y};
	float angle = atan2f(direction.y, direction.x);

	Vector2 p1 = {
		end.x - headSize * cosf(angle - PI / 6),
		end.y - headSize * sinf(angle - PI / 6)
	};
	Vector2 p2 = {
		end.x - headSize * cosf(angle + PI / 6),
		end.y - headSize * sinf(angle + PI / 6)
	};

	DrawTriangle(end, p2, p1, color);
}

void InitHud() {
	mock_direction = 0.0f;
	mock_distance_deg = 0.0f;
	mock_time = 0.0f;
	mock_has_target = false;
	dim_color = {80, 80, 80, 255};
}

// Top-left: telescope state (INIT, SETUP, IDLE, SEARCH, FOUND)
static void DrawTopLeft() {
	const char* state_str = nullptr;
	switch (GetTelescopeState()) {
	case TelescopeState::INIT:   state_str = "INIT";   break;
	case TelescopeState::SETUP:  state_str = "SETUP";  break;
	case TelescopeState::IDLE:   state_str = "IDLE";   break;
	case TelescopeState::SEARCH: state_str = "SEARCH"; break;
	case TelescopeState::FOUND:  state_str = "FOUND";  break;
	}

	MonoText(state_str, PAD + 14.0f, PAD + 14.0f, FONT_XL, displayColor);
}

// Top-right: target direction arrow + distance to target, or idle dot when no target
static void DrawTopRight() {
	float s = static_cast<float>(screenRes);

	if (!mock_has_target) {
		const char* label = "NO TGT";
		float tw = MonoWidth(label, FONT_S);
		float tx = s - PAD - 14.0f - tw;
		DrawCircleV({tx - 16.0f, PAD + 14.0f + FONT_S / 2.0f}, 5.0f, dim_color);
		MonoText(label, tx, PAD + 14.0f, FONT_S, dim_color);
		return;
	}

	// Arrow centered in the top-right corner
	float arrow_cx = s - PAD - 50.0f;
	float arrow_cy = PAD + 40.0f;
	float arrow_len = 44.0f;
	float half_len = arrow_len / 2.0f;

	Vector2 arrow_start = {
		arrow_cx - half_len * cosf(mock_direction),
		arrow_cy - half_len * sinf(mock_direction)
	};
	Vector2 arrow_end = {
		arrow_cx + half_len * cosf(mock_direction),
		arrow_cy + half_len * sinf(mock_direction)
	};
	DrawArrow(arrow_start, arrow_end, 3.0f, 12.0f, displayColor);

	// Distance below the arrow
	char dist_buf[32];
	snprintf(dist_buf, sizeof(dist_buf), "%.1f", mock_distance_deg);
	float tw = MonoWidth(dist_buf, FONT_L);
	MonoText(dist_buf, arrow_cx - tw / 2.0f, arrow_cy + half_len + 6.0f, FONT_L, displayColor);
}

// Bottom-left: calibration status + compass heading
static void DrawBottomLeft() {
	float heading_deg;
	uint32_t imu_count;
	uint8_t calibration;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		heading_deg = g_shared_state.filtered_heading_deg;
		imu_count = g_shared_state.imu_update_count;
		calibration = g_shared_state.imu.calibration;
	}

	uint8_t accel_cal = (calibration >> 2) & 0x03;
	uint8_t mag_cal   = calibration & 0x03;

	float s = static_cast<float>(screenRes);
	float x = PAD + 6.0f;
	float hdg_y = s - PAD - FONT_L - 6.0f;
	float cal_y = hdg_y - FONT_S - 2.0f;

	// Calibration line: A* M* (each 0-3, 3 = fully calibrated)
	char cal_buf[16];
	if (imu_count > 0) {
		snprintf(cal_buf, sizeof(cal_buf), "A%d M%d", accel_cal, mag_cal);
	} else {
		snprintf(cal_buf, sizeof(cal_buf), "A- M-");
	}
	Color accel_color = (imu_count > 0 && accel_cal == 3) ? displayColor : dim_color;
	Color mag_color   = (imu_count > 0 && mag_cal == 3)   ? displayColor : dim_color;

	char a_buf[4];
	snprintf(a_buf, sizeof(a_buf), "A%c", (imu_count > 0) ? ('0' + accel_cal) : '-');
	MonoText(a_buf, x, cal_y, FONT_S, accel_color);
	float a_width = MonoWidth(a_buf, FONT_S);

	char m_buf[4];
	snprintf(m_buf, sizeof(m_buf), "M%c", (imu_count > 0) ? ('0' + mag_cal) : '-');
	MonoText(m_buf, x + a_width + 12.0f, cal_y, FONT_S, mag_color);

	// Heading
	char hdg_buf[16];
	if (imu_count > 0) {
		snprintf(hdg_buf, sizeof(hdg_buf), "HDG %03.0f", heading_deg);
	} else {
		snprintf(hdg_buf, sizeof(hdg_buf), "HDG ---");
	}
	Color hdg_color = (imu_count > 0) ? displayColor : dim_color;
	MonoText(hdg_buf, x, hdg_y, FONT_L, hdg_color);
}

// Bottom-right: GPS fix indicator + debug connection indicator
static void DrawBottomRight() {
	uint32_t gps_count;
	uint32_t current_debug_count;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		gps_count = g_shared_state.gps_update_count;
		current_debug_count = g_shared_state.debug_update_count;
	}

	if (current_debug_count != last_debug_count) {
		last_debug_count = current_debug_count;
		ping_flash_timer = PING_FLASH_DURATION;
	}

	ping_flash_timer -= GetFrameTime();
	if (ping_flash_timer < 0.0f) ping_flash_timer = 0.0f;

	float s = static_cast<float>(screenRes);
	float right_x = s - PAD - 6.0f;
	float bottom_y = s - PAD - 6.0f;

	// GPS: satellite count + label, inline (bottom row)
	uint8_t num_sats = 0;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		num_sats = g_shared_state.gps.num_satellites;
	}
	bool has_fix = (gps_count > 0 && num_sats > 0);
	Color gps_color = has_fix ? GREEN : dim_color;
	char gps_label[16];
	snprintf(gps_label, sizeof(gps_label), "GPS %02d", num_sats);
	float gps_tw = MonoWidth(gps_label, FONT_S);
	float gps_row_y = bottom_y - FONT_S;
	float gps_text_x = right_x - gps_tw;
	DrawCircleV({gps_text_x - 16.0f, gps_row_y + FONT_S / 2.0f}, 6.0f, gps_color);
	MonoText(gps_label, gps_text_x, gps_row_y, FONT_S, gps_color);

	// Debug connection: dot then label, inline (row above GPS)
	Color dbg_color = (ping_flash_timer > 0.0f) ? GREEN : dim_color;
	const char* dbg_label = "DBG";
	float dbg_tw = MonoWidth(dbg_label, FONT_S);
	float dbg_row_y = gps_row_y - FONT_S - 6.0f;
	float dbg_text_x = right_x - dbg_tw;
	DrawCircleV({dbg_text_x - 16.0f, dbg_row_y + FONT_S / 2.0f}, 6.0f, dbg_color);
	MonoText(dbg_label, dbg_text_x, dbg_row_y, FONT_S, dbg_color);
}

// Decorative accents in the corners outside the circle
static void DrawCornerAccents() {
	// Corner bracket lines in the four corners
	float b = 30.0f;
	Color bracket = {displayColor.r, displayColor.g, displayColor.b, 255};

	// Top-left
	DrawLineEx({PAD, PAD}, {PAD + b, PAD}, LINE_THICK, bracket);
	DrawLineEx({PAD, PAD}, {PAD, PAD + b}, LINE_THICK, bracket);

	// Top-right
	DrawLineEx({screenRes - PAD, PAD}, {screenRes - PAD - b, PAD}, LINE_THICK, bracket);
	DrawLineEx({screenRes - PAD, PAD}, {screenRes - PAD, PAD + b}, LINE_THICK, bracket);

	// Bottom-left
	DrawLineEx({PAD, screenRes - PAD}, {PAD + b, screenRes - PAD}, LINE_THICK, bracket);
	DrawLineEx({PAD, screenRes - PAD}, {PAD, screenRes - PAD - b}, LINE_THICK, bracket);

	// Bottom-right
	DrawLineEx({screenRes - PAD, screenRes - PAD}, {screenRes - PAD - b, screenRes - PAD}, LINE_THICK, bracket);
	DrawLineEx({screenRes - PAD, screenRes - PAD}, {screenRes - PAD, screenRes - PAD - b}, LINE_THICK, bracket);
}

void DrawHud() {
	UpdateMockData();

	// Circle border
	float half = screenRes / 2.0f;
	DrawCircleLines(screenRes / 2, screenRes / 2, half, displayColor);

	DrawCornerAccents();
	DrawTopLeft();
	DrawTopRight();
	DrawBottomLeft();
	DrawBottomRight();
}
