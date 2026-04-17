#!/usr/bin/env python3
"""
Send mock telescope packets to a serial port to test the raspi display.
Usage: python3 mock_sender.py [--port /dev/ttyS0] [--baud 115200]
Requires a loopback (TX->RX) or a USB-serial adapter feeding the Pi's UART RX.
"""

import serial
import struct
import time
import argparse
import math

SYNC_HI = 0xAB
SYNC_LO = 0xCD

PACKET_GPS        = 0x01
PACKET_ENCODER    = 0x02
PACKET_IMU        = 0x03
PACKET_STATE_SYNC = 0x04
PACKET_DSO_TARGET = 0x05
PACKET_DEBUG      = 0xFF

STATE_IDLE   = 1
STATE_SEARCH = 2
STATE_FOUND  = 3


_CRC_TABLE = [
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x4864,0x5845,0x6826,0x7807,0x08E0,0x18C1,0x28A2,0x38A3,
    0xC94C,0xD96D,0xE90E,0xF92F,0x89C8,0x99E9,0xA98A,0xB9AB,
    0x5A75,0x4A54,0x7A37,0x6A16,0x1AF1,0x0AD0,0x3AB3,0x2A92,
    0xDB7D,0xCB5C,0xFB3F,0xEB1E,0x9BF9,0x8BD8,0xBBBB,0xAB9A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x85A9,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x04A1,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A55,0x5A74,0x6A17,0x7A36,0x0AD1,0x1AF0,0x2A93,0x3AB2,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0,
]

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        index = ((crc >> 8) ^ byte) & 0xFF
        crc = (_CRC_TABLE[index] ^ (crc << 8)) & 0xFFFF
    return crc


def make_packet(packet_id: int, payload: bytes) -> bytes:
    header = bytes([SYNC_HI, SYNC_LO, packet_id, len(payload)])
    frame = header + payload
    crc = crc16_ccitt(frame)
    return frame + struct.pack('<H', crc)


def make_gps(lat_e7: int, lon_e7: int, num_sats: int,
             hour: int, minute: int, second: int,
             day: int, month: int, year: int) -> bytes:
    payload = struct.pack('<iiBBBBBBH',
        lat_e7, lon_e7, num_sats,
        hour, minute, second,
        day, month, year)
    return make_packet(PACKET_GPS, payload)


def make_encoder(yaw_raw: int, pitch_raw: int) -> bytes:
    payload = struct.pack('<HH', yaw_raw, pitch_raw)
    return make_packet(PACKET_ENCODER, payload)


def make_imu(heading: int, calibration: int) -> bytes:
    payload = struct.pack('<hB', heading, calibration)
    return make_packet(PACKET_IMU, payload)


def make_state_sync(state: int) -> bytes:
    payload = struct.pack('<BBH', state, 0, 0)
    return make_packet(PACKET_STATE_SYNC, payload)


def make_dso_target(status: int, catalog_number: int, object_type: int,
                    ra_mas: int, dec_mas: int, magnitude_e2: int,
                    constellation: int, name: str) -> bytes:
    name_bytes = name.encode('ascii')[:15].ljust(16, b'\x00')
    payload = struct.pack('<BHBiihB16s',
        status, catalog_number, object_type,
        ra_mas, dec_mas, magnitude_e2,
        constellation, name_bytes)
    return make_packet(PACKET_DSO_TARGET, payload)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='/tmp/mock_sender')
    parser.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args()

    port = serial.Serial(args.port, args.baud, timeout=1)
    print(f"Sending mock packets to {args.port} at {args.baud} baud. Ctrl+C to stop.")

    t = 0.0
    while True:
        # State: SEARCH
        port.write(make_state_sync(STATE_SEARCH))

        # GPS: London, 8 sats
        port.write(make_gps(
            lat_e7=515074000, lon_e7=-1278000,
            num_sats=8,
            hour=12, minute=0, second=0,
            day=17, month=4, year=2026))

        # IMU: heading sweeps 0-360, fully calibrated
        heading_deg = (t * 10.0) % 360.0
        heading_raw = int(heading_deg * 16)
        port.write(make_imu(heading=heading_raw, calibration=0xFF))

        # Encoder: yaw and pitch sweep
        yaw_raw   = int(((t * 5.0) % 360.0) / 360.0 * 16383)
        pitch_raw = int(((t * 2.0) % 360.0) / 360.0 * 16383)
        port.write(make_encoder(yaw_raw=yaw_raw, pitch_raw=pitch_raw))

        # DSO target: M31 Andromeda
        port.write(make_dso_target(
            status=0,
            catalog_number=31,
            object_type=0,
            ra_mas=166800000,   # 10h 41m 44s in mas * 15
            dec_mas=150000000,  # +41 deg in mas  (approx)
            magnitude_e2=244,
            constellation=2,
            name='M31'))

        t += 0.1
        time.sleep(0.1)


if __name__ == '__main__':
    main()
