#pragma once

#include <cstdint>

void StartSerialReader(const char* device);
void StopSerialReader();

// Send a framed packet over the serial port. Thread-safe.
bool SendPacket(uint8_t packet_id, const uint8_t* payload, uint8_t length);
