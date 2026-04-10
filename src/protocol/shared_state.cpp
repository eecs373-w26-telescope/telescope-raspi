#include "protocol/shared_state.h"
#include <cmath>
#include <cstring>

SharedState g_shared_state;

void SharedState::updateGps(const uint8_t* payload) {
	std::memcpy(&gps, payload, sizeof(GpsPayload));
	gps_update_count++;
}

void SharedState::updateEncoder(const uint8_t* payload) {
	std::memcpy(&encoder, payload, sizeof(EncoderPayload));
	encoder_update_count++;
}

void SharedState::updateImu(const uint8_t* payload) {
	std::memcpy(&imu, payload, sizeof(ImuPayload));
	imu_update_count++;

	float heading_rad = (static_cast<float>(imu.heading) / 16.0f) * (M_PI / 180.0f);
	float s = sinf(heading_rad);
	float c = cosf(heading_rad);

	sin_sum -= sin_buf[ring_idx];
	cos_sum -= cos_buf[ring_idx];
	sin_buf[ring_idx] = s;
	cos_buf[ring_idx] = c;
	sin_sum += s;
	cos_sum += c;

	ring_idx = (ring_idx + 1) % IMU_FILTER_WINDOW;
	if (ring_count < IMU_FILTER_WINDOW) ring_count++;

	float avg_rad = atan2f(sin_sum, cos_sum);
	float avg_deg = avg_rad * (180.0f / M_PI);
	if (avg_deg < 0.0f) avg_deg += 360.0f;
	filtered_heading_deg = avg_deg;
}

void SharedState::updateTouchEvent(const uint8_t* payload) {
	std::memcpy(&touch_event, payload, sizeof(TouchEventPayload));
	touch_event_update_count++;
}

void SharedState::updateDsoResponse(const uint8_t* payload) {
	std::memcpy(&dso_response, payload, sizeof(DsoResponsePayload));
	dso_response_update_count++;
}

void SharedState::updateDebug(const uint8_t* payload) {
	std::memcpy(&debug, payload, sizeof(DebugPayload));
	debug_update_count++;
}
