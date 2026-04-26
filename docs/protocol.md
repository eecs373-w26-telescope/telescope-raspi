# UART Communication Protocol

USART1 between STM32F405 Feather and Raspberry Pi at 115200 baud (8N1).

- **Feather pins**: PB7 (RX), PB6 (TX)
- **Raspi pins**: GPIO 14 - pin 8 (TXD), GPIO 15 - pin 10 (RXD)
- **DMA**: TX normal mode, RX circular mode (both sides)

All packets flow **Nucleo -> Raspi** only, except for periodic time synchronization from **Raspi -> Nucleo**.

## Frame Format

```
Offset  Size  Field
0       1     SYNC_HI   (0xAB)
1       1     SYNC_LO   (0xCD)
2       1     PACKET_ID
3       1     LENGTH    (payload size in bytes, 0-128)
4       N     PAYLOAD
4+N     1     CRC_LO
5+N     1     CRC_HI
```

- CRC-16/CCITT-FALSE: polynomial 0x1021, init 0xFFFF
- CRC computed over bytes 0 through 3+N (sync + header + payload)
- CRC transmitted little-endian
- Min frame: 6 bytes (empty payload), max frame: 134 bytes

## Packet ID Assignments

| ID   | Name                   | Payload | Direction       | Rate        |
|------|------------------------|---------|-----------------|-------------|
| 0x01 | PACKET_GPS             | 16B     | N -> R          | 1 Hz        |
| 0x02 | PACKET_ENCODER         | 4B      | N -> R          | 10 Hz       |
| 0x03 | PACKET_IMU             | 3B      | N -> R          | 10 Hz       |
| 0x04 | PACKET_STATE_SYNC      | 5B      | N -> R          | On change   |
| 0x05 | PACKET_DSO_TARGET      | 32B     | N -> R          | On change   |
| 0x06 | PACKET_FOV_OBJECTS     | Var     | N -> R          | 5 Hz        |
| 0x07 | PACKET_TIME_MODE       | 1B      | N -> R          | On change   |
| 0x08 | PACKET_SEARCH_GUIDANCE | 7B      | N -> R          | 10 Hz       |
| 0x09 | PACKET_POINTING        | 8B      | N -> R          | 10 Hz       |
| 0x10 | PACKET_TIME            | 7B      | R -> N          | 0.1 Hz      |
| 0xFF | PACKET_DEBUG           | 16B     | Bidirectional   | Ad hoc      |

## Payload Definitions

All structs use `#pragma pack(push, 1)`.

### GpsPayload (0x01, 16 bytes)

Adafruit PA1616S GPS via UART6. Position and time from RMC sentence, satellite count from GGA.

```
Offset  Type      Field           Notes
0       int32_t   latitude_e7     degrees * 10^7, negative = South
4       int32_t   longitude_e7    degrees * 10^7, negative = West
8       uint8_t   num_satellites  from GGA sentence
9       uint8_t   utc_hour        0-23
10      uint8_t   utc_minute      0-59
11      uint8_t   utc_second      0-59
12      uint8_t   utc_day         1-31
13      uint8_t   utc_month       1-12
14      uint16_t  utc_year        e.g. 2025
```

### EncoderPayload (0x02, 4 bytes)

Two AS5048A magnetic encoders via SPI1. Yaw CS: PC2 (D12), Pitch CS: PC1 (D13).
Values are offset-corrected and moving-average filtered (window=8, circular).

```
Offset  Type      Field      Notes
0       uint16_t  yaw_raw    14-bit (0-16383 = 0-360 deg)
2       uint16_t  pitch_raw  14-bit (0-16383 = 0-360 deg)
```

### ImuPayload (0x03, 3 bytes)

BNO055 9DoF IMU via I2C1 in NDOF fusion mode. Heading from Euler register 0x1A.

```
Offset  Type     Field         Notes
0       int16_t  heading       degrees * 16 (0 to 5759 = 0.0 to 359.9375 deg)
2       uint8_t  calibration   CALIB_STAT register (0x35)
                               bits [7:6]=sys, [5:4]=gyro, [3:2]=accel, [1:0]=mag
                               each field 0-3, 3 = fully calibrated
```

### StateSyncPayload (0x04, 5 bytes)

Sent by Nucleo on every state transition or display toggle. Raspi mirrors this as its authoritative state.

```
Offset  Type      Field            Notes
0       uint8_t   state            TelescopeState: INIT=0, IDLE=1, SEARCH=2, FOUND=3
1       uint8_t   flags            reserved bitfield
2       uint8_t   overlay_visible  1 = render HUD/Sky, 0 = black screen
3       uint16_t  sequence         monotonic counter for gap detection
```

### DsoTargetPayload (0x05, 32 bytes)

Sent by Nucleo when a target DSO is selected (on SEARCH entry). Raspi uses this to
render the target overlay and name.

```
Offset  Type      Field            Notes
0       uint8_t   status           0=OK, 1=not_found, 2=sd_error, 3=no_target
1       uint16_t  catalog_number   Messier or NGC number
3       uint8_t   catalog_mode     0=Messier, 1=NGC
4       uint8_t   object_type      0=galaxy, 1=nebula, 2=open_cluster,
                                   3=globular_cluster, 4=planetary_nebula
5       int32_t   ra_mas           right ascension in milliarcseconds (0 to 1,296,000,000)
9       int32_t   dec_mas          declination in milliarcseconds (-324,000,000 to +324,000,000)
13      int16_t   magnitude_e2     apparent magnitude * 100 (e.g. 350 = mag 3.50)
15      uint8_t   constellation    index 0-87 into constellation enum
16      char      name[16]         null-terminated common name, e.g. "Andromeda"
```

### FovObjectsPayload (0x06, variable)

List of objects currently within the telescope's Field of View, calculated by the Nucleo.

```
Offset  Type            Field    Notes
0       uint8_t         count    Number of objects (max 21)
1       FovObjectEntry  objects  Array of count objects
```

**FovObjectEntry (5 bytes):**
```
Offset  Type      Field         Notes
0       uint16_t  catalog_id    Messier/NGC number
2       uint8_t   catalog_mode  0=Messier, 1=NGC
3       int16_t   x_e4          FOV x-coordinate * 10000 (-1.0 to 1.0)
5       int16_t   y_e4          FOV y-coordinate * 10000 (-1.0 to 1.0)
```

### TimeModePayload (0x07, 1 byte)

Indicates the source of the current system time on the Nucleo.

```
Offset  Type      Field      Notes
0       uint8_t   mode       0=SATELLITE, 1=RASPI, 2=COMPILE
```

### SearchGuidancePayload (0x08, 7 bytes)

Guidance vector and distance to the selected target.

```
Offset  Type      Field         Notes
0       int16_t   dx_e4         direction x * 10000 (toward target in FOV coords)
2       int16_t   dy_e4         direction y * 10000
4       int16_t   distance_e2   angular distance in degrees * 100
6       uint8_t   has_target    1 = target selected, 0 = no active target
```

### PointingPayload (0x09, 8 bytes)

Current telescope pointing coordinates in Altitude/Azimuth.

```
Offset  Type      Field      Notes
0       int32_t   alt        Altitude in milliarcseconds
4       int32_t   az         Azimuth in milliarcseconds
```

### TimePayload (0x10, 7 bytes)

Sent by Raspi to Nucleo to synchronize the RTC if GPS is unavailable.

```
Offset  Type      Field    Notes
0       uint16_t  year     e.g. 2025
2       uint8_t   month    1-12
3       uint8_t   day      1-31
4       uint8_t   hour     0-23
5       uint8_t   minute   0-59
6       uint8_t   second   0-59
```

### DebugPayload (0xFF, 16 bytes)

```
Offset  Type      Field    Notes
0       uint8_t   data[16] arbitrary debug string or binary data
```
