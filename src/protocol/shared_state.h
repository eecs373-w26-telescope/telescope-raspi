#pragma once

#include "protocol/protocol.h"
#include <mutex>
#include <cstdint>

struct SharedState {
	std::mutex mtx;

	GpsPayload         gps{};
	EncoderPayload     encoder{};
	ImuPayload         imu{};
	StateSyncPayload   state_sync{};
	DsoTargetPayload   dso_target{};
	DebugPayload       debug{};

	bool     gps_received        = false;
	bool     encoder_received    = false;
	bool     imu_received        = false;
	bool     state_sync_received = false;
	bool     dso_target_received = false;
	uint32_t debug_update_count  = 0;

	uint32_t crc_error_count = 0;
	uint32_t sync_loss_count = 0;

	void updateGps(const uint8_t* payload);
	void updateEncoder(const uint8_t* payload);
	void updateImu(const uint8_t* payload);
	void updateStateSync(const uint8_t* payload);
	void updateDsoTarget(const uint8_t* payload);
	void updateDebug(const uint8_t* payload);
};

extern SharedState g_shared_state;
