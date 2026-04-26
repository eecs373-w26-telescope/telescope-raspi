# Display System

The telescope uses a high-resolution square LCD for the eyepiece HUD.

## Specifications
- **Resolution**: 768x768 (set in `main.cpp`)
- **Backend**: Raylib (OpenGL ES 2.0 on Raspberry Pi)
- **Primary Color**: Red (#FF0000) by default to maintain scotopic vision (dark adaptation).

## Rendering Pipeline
1.  **Framebuffer**: All drawing occurs on a `RenderTexture2D` at native resolution.
2.  **Transformation**: The texture is drawn to the screen with a vertical flip if `DISPLAY_FLIP` is enabled.
3.  **Optics**: `DISPLAY_FLIP` (compile-time) corrects for the image inversion caused by the telescope's optical path (diagonal/mirror).

## UI Layering
- **Sky Layer**: Renders DSO objects as dots with catalog labels. Clipped by a circular mask to simulate the telescope FOV.
- **HUD Layer**: Vector overlays for telemetry (GPS, IMU, Encoders) and dynamic navigation guidance.
