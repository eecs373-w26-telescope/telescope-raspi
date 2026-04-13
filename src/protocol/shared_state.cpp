#include "protocol/shared_state.h"
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
