#pragma once

#include "protocol/protocol.h"
#include <array>
#include <mutex>
#include <cstdint>

static constexpr size_t IMU_FILTER_WINDOW = 8;

struct SharedState {
	std::mutex mtx;

	GpsPayload          gps{};
	EncoderPayload      encoder{};
	ImuPayload          imu{};
	TouchEventPayload   touch_event{};
	DsoResponsePayload  dso_response{};
	DebugPayload        debug{};

	float filtered_heading_deg = 0.0f;

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

private:
	std::array<float, IMU_FILTER_WINDOW> sin_buf{};
	std::array<float, IMU_FILTER_WINDOW> cos_buf{};
	float sin_sum = 0.0f;
	float cos_sum = 0.0f;
	size_t ring_idx = 0;
	size_t ring_count = 0;
};

extern SharedState g_shared_state;
