#include "protocol/shared_state.h"
#include <cstring>

SharedState g_shared_state;

void SharedState::updateGps(const uint8_t* payload) {
	std::memcpy(&gps, payload, sizeof(GpsPayload));
	gps_received = true;
}

void SharedState::updateEncoder(const uint8_t* payload) {
	std::memcpy(&encoder, payload, sizeof(EncoderPayload));
	encoder_received = true;
}

void SharedState::updateImu(const uint8_t* payload) {
	std::memcpy(&imu, payload, sizeof(ImuPayload));
	imu_received = true;
}

void SharedState::updateStateSync(const uint8_t* payload) {
	std::memcpy(&state_sync, payload, sizeof(StateSyncPayload));
	state_sync_received = true;
}

void SharedState::updateDsoTarget(const uint8_t* payload) {
	std::memcpy(&dso_target, payload, sizeof(DsoTargetPayload));
	dso_target_received = true;
}

void SharedState::updateDebug(const uint8_t* payload) {
	std::memcpy(&debug, payload, sizeof(DebugPayload));
	debug_update_count++;
}
