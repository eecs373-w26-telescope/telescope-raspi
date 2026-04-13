#pragma once

#include "protocol/protocol.h"
#include <mutex>
#include <cstdint>

struct SharedState {
	std::mutex mtx;

	GpsPayload          gps{};
	EncoderPayload      encoder{};
	ImuPayload          imu{};
	TouchEventPayload   touch_event{};
	DsoResponsePayload  dso_response{};
	DebugPayload        debug{};

	uint32_t gps_update_count          = 0;
	uint32_t encoder_update_count      = 0;
	uint32_t imu_update_count          = 0;
	uint32_t touch_event_update_count  = 0;
	uint32_t dso_response_update_count = 0;
	uint32_t debug_update_count        = 0;

	uint32_t crc_error_count  = 0;
	uint32_t sync_loss_count  = 0;

	void updateGps(const uint8_t* payload);
	void updateEncoder(const uint8_t* payload);
	void updateImu(const uint8_t* payload);
	void updateTouchEvent(const uint8_t* payload);
	void updateDsoResponse(const uint8_t* payload);
	void updateDebug(const uint8_t* payload);
};

extern SharedState g_shared_state;
