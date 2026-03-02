#include "protocol/stream_decoder.h"
#include <cstring>

StreamDecoder::StreamDecoder(DispatchFn on_packet)
	: on_packet_(std::move(on_packet)) {}

void StreamDecoder::feed(const uint8_t* data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		process_byte(data[i]);
	}
}

void StreamDecoder::reset() {
	state_ = State::HUNT_SYNC_HI;
	header_pos_ = 0;
	payload_pos_ = 0;
	crc_pos_ = 0;
	expected_len_ = 0;
	packet_id_ = 0;
}

void StreamDecoder::process_byte(uint8_t byte) {
	switch (state_) {

	case State::HUNT_SYNC_HI:
		if (byte == SYNC_HI) {
			state_ = State::HUNT_SYNC_LO;
		}
		break;

	case State::HUNT_SYNC_LO:
		if (byte == SYNC_LO) {
			state_ = State::READ_HEADER;
			header_pos_ = 0;
		} else if (byte == SYNC_HI) {
			// Stay in HUNT_SYNC_LO: handles 0xAB 0xAB 0xCD
		} else {
			sync_losses_++;
			state_ = State::HUNT_SYNC_HI;
		}
		break;

	case State::READ_HEADER:
		header_buf_[header_pos_++] = byte;
		if (header_pos_ == 2) {
			packet_id_ = header_buf_[0];
			expected_len_ = header_buf_[1];
			if (expected_len_ > MAX_PAYLOAD_SIZE) {
				sync_losses_++;
				state_ = State::HUNT_SYNC_HI;
			} else if (expected_len_ == 0) {
				crc_pos_ = 0;
				state_ = State::READ_CRC;
			} else {
				payload_pos_ = 0;
				state_ = State::READ_PAYLOAD;
			}
		}
		break;

	case State::READ_PAYLOAD:
		payload_buf_[payload_pos_++] = byte;
		if (payload_pos_ == expected_len_) {
			crc_pos_ = 0;
			state_ = State::READ_CRC;
		}
		break;

	case State::READ_CRC:
		crc_buf_[crc_pos_++] = byte;
		if (crc_pos_ == 2) {
			// Reconstruct the full frame for CRC: sync + id + len + payload
			uint8_t frame[4 + MAX_PAYLOAD_SIZE];
			frame[0] = SYNC_HI;
			frame[1] = SYNC_LO;
			frame[2] = packet_id_;
			frame[3] = expected_len_;
			std::memcpy(frame + 4, payload_buf_, expected_len_);

			uint16_t computed = crc16_ccitt(frame, 4 + expected_len_);
			uint16_t received = static_cast<uint16_t>(crc_buf_[0])
			                  | (static_cast<uint16_t>(crc_buf_[1]) << 8);

			if (computed == received) {
				on_packet_(packet_id_, payload_buf_, expected_len_);
			} else {
				crc_errors_++;
			}

			state_ = State::HUNT_SYNC_HI;
		}
		break;
	}
}
