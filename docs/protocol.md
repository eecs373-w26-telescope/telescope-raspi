# UART Communication Protocol

USART1 between Feather and Raspi at 115200

- **Feather pins**: PB7 (RX), PB6 (TX)
- **Raspi pins**: GPIO 14 - pin 8 (TXD), GPIO 15 - pin 10 (RXD)
- **DMA**: TX normal mode, RX circular mode

## Frame Format


```
Offset  Size  Field
0       1     SYNC_HI   (0xAB)
1       1     SYNC_LO   (0xCD)
2       1     PACKET_ID
3       1     LENGTH    (payload size, 0-128)
4       N     PAYLOAD   (0 to 128 bytes)
4+N     1     CRC_LO
5+N     1     CRC_HI
```

- CRC-16/CCITT-FALSE: polynomial 0x1021, init 0xFFFF
- CRC computed over bytes 0 through 3+N (sync + header + payload)
- CRC transmitted little-endian
- Min frame: 6 bytes (empty payload), max frame: 134 bytes

## Packet ID Assignments

### Nucleo to Raspi

| ID   | Name               | Payload | Rate     |
|------|--------------------|---------|----------|
| 0x01 | PACKET_GPS         | 14B     | 1 Hz     |
| 0x02 | PACKET_ENCODER     | 12B     | 50 Hz    |
| 0x03 | PACKET_TOUCH_EVENT | 5B      | On event |
| 0x04 | PACKET_IMU         | 16B     | 50 Hz    |
| 0x21 | PACKET_DSO_RESPONSE| 33B     | Response |

### Raspi to Nucleo

| ID   | Name               | Payload | Rate      |
|------|--------------------|---------|-----------|
| 0x10 | PACKET_STATE_SYNC  | 4B      | 5 Hz      |
| 0x20 | PACKET_DSO_REQUEST | 5B      | On demand |

### Bidirectional

| ID   | Name          | Payload | Rate   |
|------|---------------|---------|--------|
| 0xFF | PACKET_DEBUG  | 64B     | Ad hoc |

## Payload Definitions

`#pragma pack(push, 1)` to tell compiler not to add padding

### GpsPayload (0x01, 14 bytes)

Adafruit PA1616S Ultimate GPS via UART.

```
Offset  Type     Field           Notes
0       int32_t  latitude_e7     degrees * 10^7
4       int32_t  longitude_e7    degrees * 10^7
8       int32_t  altitude_mm     millimeters
12      uint8_t  fix_quality     0=none, 1=GPS, 2=DGPS
13      uint8_t  num_satellites
```

### EncoderPayload (0x02, 12 bytes)

Two AS5600 magnetic rotary encoders via I2C. One for yaw (azimuth), one for pitch (elevation).

```
Offset  Type      Field            Notes
0       int32_t   azimuth_ticks    cumulative yaw ticks
4       int32_t   elevation_ticks  cumulative pitch ticks
8       uint16_t  azimuth_raw      raw 12-bit sensor value (0-4095)
10      uint16_t  elevation_raw    raw 12-bit sensor value (0-4095)
```

### ImuPayload (0x04, 16 bytes)

BNO055 9DoF IMU via I2C. Used to determine true north for the telescope base. All values in BNO055 native register format to avoid float conversion on the nucleo.

```
Offset  Type     Field         Notes
0       int16_t  heading       degrees * 16 (0 to 5759 = 0.0 to 359.9375 deg)
2       int16_t  roll          degrees * 16 (-2880 to 2880)
4       int16_t  pitch         degrees * 16 (-1440 to 1440)
6       int16_t  quat_w        quaternion, scale 1/16384
8       int16_t  quat_x
10      int16_t  quat_y
12      int16_t  quat_z
14      uint8_t  calibration   packed nibbles: [sys:2][gyro:2][accel:2][mag:2]
                               matches BNO055 CALIB_STAT register (0x35)
                               each field 0-3, 3 = fully calibrated
15      int8_t   temperature   degrees C
```

### TouchEventPayload (0x03, 5 bytes)

Nucleo handles raw touchscreen input locally via its touchscreen library and renders its own UI based on the mirrored TelescopeState. Only semantic user actions are sent to the raspi.

```
Offset  Type      Field        Notes
0       uint8_t   event_type   0=DSO_SELECTED, 1=SEARCH_START, 2=SEARCH_CANCEL, 3=MODE_CHANGE
1       uint16_t  param        event-specific (e.g., Messier number for DSO_SELECTED)
3       uint8_t   reserved[2]  zero-filled
```

### StateSyncPayload (0x10, 4 bytes)

Raspi sends this at 5 Hz. This is the heartbeat. Nucleo mirrors the state for its local touchscreen UI. If no state sync received for >1 second, nucleo treats connection as lost.

```
Offset  Type      Field      Notes
0       uint8_t   state      TelescopeState enum: INIT=0, SETUP=1, IDLE=2, SEARCH=3, FOUND=4
1       uint8_t   flags      reserved bitfield
2       uint16_t  sequence   monotonic counter, for gap detection
```

### DsoRequestPayload (0x20, 5 bytes)

Raspi requests a Messier catalog record from the nucleo's SD card. Retry after 500ms timeout, max 3 retries.

```
Offset  Type      Field          Notes
0       uint8_t   request_type   0=by_index (0-109), 1=by_catalog_number (M1-M110)
1       uint16_t  request_id     for matching response to request
3       uint16_t  key            index or Messier number depending on request_type
```

### DsoResponsePayload (0x21, 33 bytes)

Nucleo reads from `messier.bin` on SD card (110 records, 33 bytes each, 3.6 KB total). Index lookup is O(1) via fseek.

```
Offset  Type      Field            Notes
0       uint16_t  request_id       echoed from request
2       uint8_t   status           0=OK, 1=not_found, 2=sd_error
3       uint16_t  catalog_number   Messier number (1-110)
5       uint8_t   object_type      0=galaxy, 1=nebula, 2=open_cluster,
                                   3=globular_cluster, 4=planetary_nebula
6       int32_t   ra_mas           right ascension in milliarcseconds (0 to 1,296,000,000)
10      int32_t   dec_mas          declination in milliarcseconds (-324,000,000 to +324,000,000)
14      int16_t   magnitude_e2     apparent magnitude * 100 (e.g., 350 = mag 3.50)
16      uint8_t   constellation    index 0-87 into constellation enum
17      char      name[16]         null-terminated, e.g. "Andromeda"
```

### DebugPayload (0xFF, 64 bytes)

```
Offset  Type       Field   Notes
0       uint8_t    data[64]
```

## Reliability

- **Sensor streams** (GPS, encoder, IMU): fire-and-forget. Next packet supersedes a lost one. No ACK.
- **State sync**: serves as heartbeat. No ACK needed. Gap detection via sequence counter.
- **DSO request/response**: response serves as ACK. Raspi retries on timeout. Nucleo is idempotent (same request_id produces same response).

## Bandwidth Budget

115200 baud = 11,520 bytes/sec (8N1).

| Stream          | Frame Size | Rate   | Bytes/sec |
|-----------------|-----------|--------|-----------|
| GPS             | 20B       | 1 Hz   | 20        |
| Encoder         | 18B       | 50 Hz  | 900       |
| IMU             | 22B       | 50 Hz  | 1,100     |
| State sync      | 10B       | 5 Hz   | 50        |
| **Total**       |           |        | **2,070** |

18% utilization. Plenty of headroom.
