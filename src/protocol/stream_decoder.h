#pragma once

#include "protocol/protocol.h"
#include <cstdint>
#include <cstddef>
#include <functional>

class StreamDecoder {
public:
	using DispatchFn = std::function<void(uint8_t packet_id,
	                                      const uint8_t* payload,
	                                      uint8_t length)>;

	explicit StreamDecoder(DispatchFn on_packet);

	void feed(const uint8_t* data, size_t len);
	void reset();

	uint32_t crc_errors() const { return crc_errors_; }
	uint32_t sync_losses() const { return sync_losses_; }

private:
	enum class State {
		HUNT_SYNC_HI,
		HUNT_SYNC_LO,
		READ_HEADER,
		READ_PAYLOAD,
		READ_CRC,
	};

	void process_byte(uint8_t byte);

	State state_ = State::HUNT_SYNC_HI;
	uint8_t header_buf_[2]{};
	uint8_t payload_buf_[MAX_PAYLOAD_SIZE]{};
	uint8_t crc_buf_[2]{};
	uint8_t header_pos_ = 0;
	uint8_t payload_pos_ = 0;
	uint8_t crc_pos_ = 0;
	uint8_t expected_len_ = 0;
	uint8_t packet_id_ = 0;

	uint32_t crc_errors_ = 0;
	uint32_t sync_losses_ = 0;

	DispatchFn on_packet_;
};
