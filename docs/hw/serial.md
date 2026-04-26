# Serial Interface (UART)

Physical link between Raspberry Pi and Nucleo-F405 master.

## Connection
- **Device**: `/dev/ttyS0` (default)
- **Baud Rate**: 115200 (8N1)
- **Pins**: GPIO 14 (TX), GPIO 15 (RX)

## Implementation
- **Threaded RX**: `serial_reader.cpp` runs a dedicated thread to avoid blocking the 60fps render loop.
- **Decoder**: A state-based `StreamDecoder` handles `0xABCD` sync byte detection and CRC-16 validation.
- **Latency**: Minimal; packets are dispatched to `SharedState` immediately upon CRC validation.

## Outbound (Raspi -> Nucleo)
- **Time Sync**: Raspi sends `PACKET_TIME` (0x10) every 10 seconds.
- **Purpose**: Provides UTC reference to Nucleo for sidereal time calculations when GPS lock is absent.
