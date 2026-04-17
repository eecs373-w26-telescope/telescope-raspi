#include "serial/time_sender.h"
#include "serial/serial_reader.h"
#include "protocol/protocol.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>

static std::atomic<bool> running{false};
static std::thread sender_thread;

constexpr int SEND_INTERVAL_S = 5;

static void sender_loop() {
    while (running.load()) {
        struct timespec ts{};
        clock_gettime(CLOCK_REALTIME, &ts);

        struct tm utc{};
        gmtime_r(&ts.tv_sec, &utc);

        TimePayload payload{};
        payload.year   = static_cast<uint16_t>(utc.tm_year + 1900);
        payload.month  = static_cast<uint8_t>(utc.tm_mon + 1);
        payload.day    = static_cast<uint8_t>(utc.tm_mday);
        payload.hour   = static_cast<uint8_t>(utc.tm_hour);
        payload.minute = static_cast<uint8_t>(utc.tm_min);
        payload.second = static_cast<uint8_t>(utc.tm_sec);

        SendPacket(PACKET_TIME,
                   reinterpret_cast<const uint8_t*>(&payload),
                   sizeof(payload));

        std::this_thread::sleep_for(std::chrono::seconds(SEND_INTERVAL_S));
    }
}

void StartTimeSender() {
    running.store(true);
    sender_thread = std::thread(sender_loop);
}

void StopTimeSender() {
    running.store(false);
    if (sender_thread.joinable()) {
        sender_thread.join();
    }
}
