#include "display/hud.h"
#include "protocol/shared_state.h"
#include "state_machine/state_machine.h"
#include "globals.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

static constexpr float PAD        = 3.0f;
static constexpr float PING_FLASH = 0.5f;
static constexpr float FONT_S     = 30.0f;
static constexpr float FONT_L     = 44.0f;
static constexpr float FONT_XL    = 50.0f;
static constexpr float FONT_XXL   = 72.0f;
static constexpr float SPACING    = 0.0f;
static constexpr float LINE_THICK = 1.5f;

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
	ping_flash_timer = 0.0f;
	last_debug_count = 0;
}

// Top-left: telescope state (INIT, IDLE, SEARCH, FOUND) + UTC clock
static void DrawTopLeft() {
	const char* state_str = nullptr;
	switch (GetTelescopeState()) {
	case TelescopeState::INIT:   state_str = "INIT";   break;
	case TelescopeState::IDLE:   state_str = "IDLE";   break;
	case TelescopeState::SEARCH: state_str = "SEARCH"; break;
	case TelescopeState::FOUND:  state_str = "FOUND";  break;
	}

	float x = PAD + 8.0f;
	float y = PAD + 8.0f;
	MonoText(state_str, x, y, FONT_XL, DisplayColor());

	time_t now = time(nullptr);
	struct tm utc{};
	gmtime_r(&now, &utc);
	char clock_buf[12];
	snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d:%02d",
		utc.tm_hour, utc.tm_min, utc.tm_sec);
	MonoText(clock_buf, x, y + FONT_XL + 1.0f, FONT_S, DisplayColor());

	uint8_t time_mode;
	bool time_mode_received;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		time_mode          = g_shared_state.time_mode.mode;
		time_mode_received = g_shared_state.time_mode_received;
	}
	if (time_mode_received) {
		const char* mode_label = "C";
		if (time_mode == TIME_MODE_SATELLITE)  mode_label = "S";
		else if (time_mode == TIME_MODE_RASPI) mode_label = "R";
		MonoText(mode_label, x, y + FONT_XL + 1.0f + FONT_S + 1.0f, FONT_XL, DisplayColor());
	}
}

// Top-right: active target name, or idle indicator when no target
static void DrawTopRight() {
	bool has_target;
	char name[17]{};
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		has_target = g_shared_state.dso_target_received &&
		             g_shared_state.dso_target.status == DSO_OK;
		if (has_target) {
			std::memcpy(name, g_shared_state.dso_target.name, 16);
			name[16] = '\0';
		}
	}

	float s = static_cast<float>(screenRes);

	if (!has_target) {
		const char* label = "NO TGT";
		float tw = MonoWidth(label, FONT_S);
		float tx = s - PAD - 14.0f - tw;
		DrawCircleV({tx - 16.0f, PAD + 14.0f + FONT_S / 2.0f}, 5.0f, DimColor());
		MonoText(label, tx, PAD + 14.0f, FONT_S, DimColor());
		return;
	}

	float tw = MonoWidth(name, FONT_XXL);
	MonoText(name, s - PAD - 8.0f - tw, PAD + 8.0f, FONT_XXL, DisplayColor());
}

// Bottom-left: encoder yaw/pitch + compass heading + calibration indicators
static void DrawBottomLeft() {
	float heading_deg;
	bool imu_received, enc_received;
	uint8_t calibration;
	uint16_t yaw_raw, pitch_raw;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		heading_deg  = static_cast<float>(g_shared_state.imu.heading) / 16.0f;
		imu_received = g_shared_state.imu_received;
		calibration  = g_shared_state.imu.calibration;
		yaw_raw      = g_shared_state.encoder.yaw_raw;
		pitch_raw    = g_shared_state.encoder.pitch_raw;
		enc_received = g_shared_state.encoder_received;
	}

	uint8_t accel_cal = (calibration >> 2) & 0x03;
	uint8_t mag_cal   = calibration & 0x03;

	float s = static_cast<float>(screenRes);
	float x = PAD + 8.0f;

	char a_buf[4], m_buf[4];
	snprintf(a_buf, sizeof(a_buf), "A%c", imu_received ? ('0' + accel_cal) : '-');
	snprintf(m_buf, sizeof(m_buf), "M%c", imu_received ? ('0' + mag_cal)   : '-');

	float hdg_y = s - PAD - 8.0f - FONT_L;
	float pit_y = hdg_y - FONT_S - 1.0f;
	float yaw_y = pit_y - FONT_S - 1.0f;
	float m_y   = yaw_y - FONT_S - 1.0f;
	float a_y   = m_y   - FONT_S - 1.0f;

	float yaw_deg   = (static_cast<float>(yaw_raw)   / 16383.0f) * 360.0f;
	float pitch_deg = (static_cast<float>(pitch_raw)  / 16383.0f) * 360.0f;
	char yaw_buf[16], pit_buf[16];
	if (enc_received) {
		snprintf(yaw_buf, sizeof(yaw_buf), "Y %05.1f", yaw_deg);
		snprintf(pit_buf, sizeof(pit_buf), "P %05.1f", pitch_deg);
	} else {
		snprintf(yaw_buf, sizeof(yaw_buf), "Y ---.-");
		snprintf(pit_buf, sizeof(pit_buf), "P ---.-");
	}
	Color enc_color = enc_received ? DisplayColor() : DimColor();
	MonoText(a_buf, x, a_y, FONT_S, (imu_received && accel_cal == 3) ? DisplayColor() : DimColor());
	MonoText(m_buf, x, m_y, FONT_S, (imu_received && mag_cal   == 3) ? DisplayColor() : DimColor());
	MonoText(yaw_buf, x, yaw_y, FONT_S, enc_color);
	MonoText(pit_buf, x, pit_y, FONT_S, enc_color);

	char hdg_buf[16];
	if (imu_received) {
		snprintf(hdg_buf, sizeof(hdg_buf), "HDG %05.1f", heading_deg);
	} else {
		snprintf(hdg_buf, sizeof(hdg_buf), "HDG ---.-");
	}
	MonoText(hdg_buf, x, hdg_y, FONT_L, imu_received ? DisplayColor() : DimColor());
}

// Bottom-right: GPS fix indicator + debug connection indicator
static void DrawBottomRight() {
	bool gps_received;
	uint8_t num_sats;
	uint32_t current_debug_count;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		gps_received        = g_shared_state.gps_received;
		num_sats            = g_shared_state.gps.num_satellites;
		current_debug_count = g_shared_state.debug_update_count;
	}

	if (current_debug_count != last_debug_count) {
		last_debug_count = current_debug_count;
		ping_flash_timer = PING_FLASH;
	}

	ping_flash_timer -= GetFrameTime();
	if (ping_flash_timer < 0.0f) ping_flash_timer = 0.0f;

	float s = static_cast<float>(screenRes);
	float right_x = s - PAD - 6.0f;
	float bottom_y = s - PAD - 6.0f;

	bool has_fix = (gps_received && num_sats > 0);
	Color gps_color = has_fix ? DisplayColor() : DimColor();
	char gps_label[16];
	snprintf(gps_label, sizeof(gps_label), "GPS %02d", num_sats);
	float gps_tw = MonoWidth(gps_label, FONT_S);
	float gps_row_y = bottom_y - FONT_S;
	float gps_text_x = right_x - gps_tw;
	DrawCircleV({gps_text_x - 16.0f, gps_row_y + FONT_S / 2.0f}, 6.0f, gps_color);
	MonoText(gps_label, gps_text_x, gps_row_y, FONT_S, gps_color);

	Color dbg_color = (ping_flash_timer > 0.0f) ? DisplayColor() : DimColor();
	const char* dbg_label = "DBG";
	float dbg_tw = MonoWidth(dbg_label, FONT_S);
	float dbg_row_y = gps_row_y - FONT_S - 6.0f;
	float dbg_text_x = right_x - dbg_tw;
	DrawCircleV({dbg_text_x - 16.0f, dbg_row_y + FONT_S / 2.0f}, 6.0f, dbg_color);
	MonoText(dbg_label, dbg_text_x, dbg_row_y, FONT_S, dbg_color);
}

// Center: chevron on circle edge pointing toward target + distance inside
static void DrawCenter() {
	bool has_target;
	float dx, dy, distance_deg;
	{
		std::lock_guard<std::mutex> lock(g_shared_state.mtx);
		if (!g_shared_state.search_guidance_received) return;
		has_target   = g_shared_state.search_guidance.has_target != 0;
		dx           = g_shared_state.search_guidance.dx_e4 / 10000.0f;
		dy           = g_shared_state.search_guidance.dy_e4 / 10000.0f;
		distance_deg = g_shared_state.search_guidance.distance_e2 / 100.0f;
	}

	if (!has_target) return;

	float cx = screenRes / 2.0f;
	float cy = screenRes / 2.0f;
	float R  = screenRes / 2.0f;

	char dist_buf[16];
	snprintf(dist_buf, sizeof(dist_buf), "%.1f", distance_deg);
	float tw = MonoWidth(dist_buf, FONT_L);

	if (dx * dx + dy * dy < 1e-6f) {
		DrawCircleV({cx, cy}, 8.0f, DisplayColor());
		MonoText(dist_buf, cx - tw / 2.0f, cy + 16.0f, FONT_L, DisplayColor());
		return;
	}

	constexpr float ARM_LEN        = 22.0f;
	constexpr float HALF_ANGLE     = 0.55f; // radians (~31 deg per arm)
	constexpr float THICKNESS      = 3.0f;
	constexpr float TIP_INSET      = 10.0f;
	constexpr float TEXT_INSET     = 12.0f;

	float angle = atan2f(dy, dx);
	float tip_r = R - TIP_INSET;
	Vector2 tip = {cx + tip_r * cosf(angle), cy + tip_r * sinf(angle)};

	float a1 = angle + PI - HALF_ANGLE;
	float a2 = angle + PI + HALF_ANGLE;
	Vector2 arm1 = {tip.x + ARM_LEN * cosf(a1), tip.y + ARM_LEN * sinf(a1)};
	Vector2 arm2 = {tip.x + ARM_LEN * cosf(a2), tip.y + ARM_LEN * sinf(a2)};
	DrawLineEx(arm1, tip,  THICKNESS, DisplayColor());
	DrawLineEx(tip,  arm2, THICKNESS, DisplayColor());

	float text_r = tip_r - ARM_LEN - TEXT_INSET;
	MonoText(dist_buf,
	         cx + text_r * cosf(angle) - tw / 2.0f,
	         cy + text_r * sinf(angle) - FONT_L / 2.0f,
	         FONT_L, DisplayColor());
}

// Decorative corner bracket accents
static void DrawCornerAccents() {
	float b = 30.0f;

	DrawLineEx({PAD, PAD}, {PAD + b, PAD}, LINE_THICK, DisplayColor());
	DrawLineEx({PAD, PAD}, {PAD, PAD + b}, LINE_THICK, DisplayColor());
	DrawLineEx({screenRes - PAD, PAD}, {screenRes - PAD - b, PAD}, LINE_THICK, DisplayColor());
	DrawLineEx({screenRes - PAD, PAD}, {screenRes - PAD, PAD + b}, LINE_THICK, DisplayColor());
	DrawLineEx({PAD, screenRes - PAD}, {PAD + b, screenRes - PAD}, LINE_THICK, DisplayColor());
	DrawLineEx({PAD, screenRes - PAD}, {PAD, screenRes - PAD - b}, LINE_THICK, DisplayColor());
	DrawLineEx({screenRes - PAD, screenRes - PAD}, {screenRes - PAD - b, screenRes - PAD}, LINE_THICK, DisplayColor());
	DrawLineEx({screenRes - PAD, screenRes - PAD}, {screenRes - PAD, screenRes - PAD - b}, LINE_THICK, DisplayColor());
}

void DrawHud() {
	DrawRing({screenRes / 2.0f, screenRes / 2.0f}, screenRes / 2.0f - LINE_THICK, screenRes / 2.0f, 0.0f, 360.0f, 64, DisplayColor());

	DrawCornerAccents();
	DrawTopLeft();
	DrawTopRight();
	DrawBottomLeft();
	DrawBottomRight();
	DrawCenter();
}
