#include "serial/serial_reader.h"
#include "protocol/stream_decoder.h"
#include "protocol/shared_state.h"

#include <thread>
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

static std::atomic<bool> running{false};
static std::thread reader_thread;
static int serial_fd = -1;

static void dispatch(uint8_t packet_id, const uint8_t* payload, uint8_t length) {
	std::lock_guard<std::mutex> lock(g_shared_state.mtx);

	switch (packet_id) {
	case PACKET_GPS:
		if (length >= sizeof(GpsPayload)) {
			std::memcpy(&g_shared_state.gps, payload, sizeof(GpsPayload));
			g_shared_state.gps_update_count++;
		}
		break;

	case PACKET_ENCODER:
		if (length >= sizeof(EncoderPayload)) {
			std::memcpy(&g_shared_state.encoder, payload, sizeof(EncoderPayload));
			g_shared_state.encoder_update_count++;
		}
		break;

	case PACKET_IMU:
		if (length >= sizeof(ImuPayload)) {
			std::memcpy(&g_shared_state.imu, payload, sizeof(ImuPayload));
			g_shared_state.imu_update_count++;
		}
		break;

	case PACKET_TOUCH_EVENT:
		if (length >= sizeof(TouchEventPayload)) {
			std::memcpy(&g_shared_state.touch_event, payload, sizeof(TouchEventPayload));
			g_shared_state.touch_event_update_count++;
		}
		break;

	case PACKET_DSO_RESPONSE:
		if (length >= sizeof(DsoResponsePayload)) {
			std::memcpy(&g_shared_state.dso_response, payload, sizeof(DsoResponsePayload));
			g_shared_state.dso_response_update_count++;
		}
		break;

	case PACKET_DEBUG:
		if (length >= sizeof(DebugPayload)) {
			std::memcpy(&g_shared_state.debug, payload, sizeof(DebugPayload));
			g_shared_state.debug_update_count++;
		}
		break;

	default:
		break;
	}
}

static void reader_loop() {
	StreamDecoder decoder(dispatch);
	uint8_t buf[256];

	while (running.load()) {
		ssize_t n = read(serial_fd, buf, sizeof(buf));
		if (n > 0) {
			decoder.feed(buf, static_cast<size_t>(n));

			std::lock_guard<std::mutex> lock(g_shared_state.mtx);
			g_shared_state.crc_error_count = decoder.crc_errors();
			g_shared_state.sync_loss_count = decoder.sync_losses();
		}
	}
}

static bool configure_port(int fd) {
	struct termios tty{};
	if (tcgetattr(fd, &tty) != 0) return false;

	cfsetispeed(&tty, B115200);
	cfsetospeed(&tty, B115200);

	// 8N1, no flow control
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cflag |= CREAD | CLOCAL;

	// Raw input
	tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

	// Raw output
	tty.c_oflag &= ~OPOST;

	// Blocking read with 100ms timeout
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1;

	return tcsetattr(fd, TCSANOW, &tty) == 0;
}

void StartSerialReader(const char* device) {
	serial_fd = open(device, O_RDWR | O_NOCTTY);
	if (serial_fd < 0) return;

	if (!configure_port(serial_fd)) {
		close(serial_fd);
		serial_fd = -1;
		return;
	}

	running.store(true);
	reader_thread = std::thread(reader_loop);
}

void StopSerialReader() {
	running.store(false);
	if (reader_thread.joinable()) {
		reader_thread.join();
	}
	if (serial_fd >= 0) {
		close(serial_fd);
		serial_fd = -1;
	}
}
