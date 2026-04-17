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

void SharedState::updateTimeMode(const uint8_t* payload) {
	std::memcpy(&time_mode, payload, sizeof(TimeModePayload));
	time_mode_received = true;
}

void SharedState::updateFovObjects(const uint8_t* payload, uint8_t length) {
	if (length < 1) return;
	uint8_t count = payload[0];
	if (count > FOV_OBJECTS_MAX) count = FOV_OBJECTS_MAX;
	fov_objects.count = count;
	const uint8_t expected = static_cast<uint8_t>(1 + count * sizeof(FovObjectEntry));
	if (length >= expected)
		std::memcpy(fov_objects.objects, payload + 1, count * sizeof(FovObjectEntry));
	fov_objects_received = true;
}
