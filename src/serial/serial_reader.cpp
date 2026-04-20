#include "serial/serial_reader.h"
#include "protocol/protocol.h"
#include "protocol/stream_decoder.h"
#include "protocol/shared_state.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstdio>

static std::atomic<bool> running{false};
static std::thread reader_thread;
static int serial_fd = -1;
static std::mutex tx_mutex;

static void dispatch(uint8_t packet_id, const uint8_t* payload, uint8_t length) {
	std::lock_guard<std::mutex> lock(g_shared_state.mtx);

	switch (packet_id) {
	case PACKET_GPS:
		if (length >= sizeof(GpsPayload)) g_shared_state.updateGps(payload);
		break;
	case PACKET_ENCODER:
		if (length >= sizeof(EncoderPayload)) g_shared_state.updateEncoder(payload);
		break;
	case PACKET_IMU:
		if (length >= sizeof(ImuPayload)) g_shared_state.updateImu(payload);
		break;
	case PACKET_STATE_SYNC:
		if (length >= sizeof(StateSyncPayload)) g_shared_state.updateStateSync(payload);
		break;
	case PACKET_DSO_TARGET:
		if (length >= sizeof(DsoTargetPayload)) g_shared_state.updateDsoTarget(payload);
		break;
	case PACKET_DEBUG:
		if (length >= sizeof(DebugPayload)) g_shared_state.updateDebug(payload);
		break;
	case PACKET_FOV_OBJECTS:
		g_shared_state.updateFovObjects(payload, length);
		break;
	case PACKET_TIME_MODE:
		if (length >= sizeof(TimeModePayload)) g_shared_state.updateTimeMode(payload);
		break;
	case PACKET_SEARCH_GUIDANCE:
		if (length >= sizeof(SearchGuidancePayload)) g_shared_state.updateSearchGuidance(payload);
		break;
	case PACKET_POINTING:
		if (length >= sizeof(PointingPayload)) g_shared_state.updatePointing(payload);
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

			fprintf(stderr, "[serial] rx %zd bytes  crc_err=%u sync_loss=%u\n",
				n, decoder.crc_errors(), decoder.sync_losses());
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
	if (serial_fd < 0) {
		fprintf(stderr, "[serial] failed to open %s\n", device);
		return;
	}

	if (!configure_port(serial_fd)) {
		fprintf(stderr, "[serial] configure_port failed for %s, continuing anyway\n", device);
	}

	fprintf(stderr, "[serial] opened %s\n", device);
	running.store(true);
	reader_thread = std::thread(reader_loop);
}

bool SendPacket(uint8_t packet_id, const uint8_t* payload, uint8_t length) {
	if (serial_fd < 0 || length > MAX_PAYLOAD_SIZE) return false;

	uint8_t frame[6 + MAX_PAYLOAD_SIZE];
	frame[0] = SYNC_HI;
	frame[1] = SYNC_LO;
	frame[2] = packet_id;
	frame[3] = length;
	for (uint8_t i = 0; i < length; ++i) frame[4 + i] = payload[i];

	uint16_t crc = crc16_ccitt(frame, 4 + length);
	frame[4 + length]     = static_cast<uint8_t>(crc & 0xFF);
	frame[5 + length]     = static_cast<uint8_t>(crc >> 8);

	std::lock_guard<std::mutex> lock(tx_mutex);
	ssize_t written = write(serial_fd, frame, 6 + length);
	return written == static_cast<ssize_t>(6 + length);
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
