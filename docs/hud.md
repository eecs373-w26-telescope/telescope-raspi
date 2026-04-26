# HUD Layout

Red monochrome interface for night-vision preservation.

## UI Elements

### Top-Left: Status
- **State**: Current Nucleo state (`INIT`, `IDLE`, `SEARCH`, `FOUND`).
- **Clock**: UTC time.
- **Source**: `S` (GPS), `R` (Raspi), `C` (Compile-time fallback).

### Top-Right: Target
- Active DSO name (e.g., "Andromeda") or "NONE".

### Bottom-Left: Sensors
- **A / M**: Accelerometer and Magnetometer calibration (0-3).
- **Y / P**: Raw Yaw/Pitch encoder values.
- **HDG**: IMU-fused heading.

### Bottom-Right: System
- **DBG**: Pulse dot (rx activity).
- **GPS**: Satellite count.
- **AZ / ALT**: Calculated horizontal coordinates.

### Center: Guidance
Visible during `SEARCH` state:
- **Chevron**: Points toward target along the FOV ring.
- **Distance**: Angular distance to target in degrees.
- **Bullseye**: Replaces chevron when target is centered.

### FOV (Sky)
- Objects from `PACKET_FOV_OBJECTS` rendered as labeled dots (e.g., "M31").
- Active target is highlighted with larger text/dot.
