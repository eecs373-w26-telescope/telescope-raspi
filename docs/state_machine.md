# State Mirroring

The Raspi UI state is a slave to the Nucleo master state machine.

## Synchronization
- **Trigger**: Nucleo broadcasts `PACKET_STATE_SYNC` (0x04) on transition or timer.
- **Fields**:
    - `state`: Current operational mode (`INIT`, `IDLE`, `SEARCH`, `FOUND`).
    - `overlay_visible`: Global toggle for UI rendering. Used for power management/dark mode.

## Logic Mapping
| State | HUD Behavior | Sky Rendering |
|-------|--------------|---------------|
| `INIT` | Display "INIT" | Disabled |
| `IDLE` | Display "IDLE" | FOV objects enabled |
| `SEARCH` | Display "SEARCH" + Guidance | FOV objects enabled |
| `FOUND` | Display "FOUND" | Target highlighted |

If `overlay_visible == 0`, all rendering is bypassed (black screen).
