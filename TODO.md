# Raspi TODO

## Protocol Cleanup
- [ ] Remove `PACKET_DSO_TARGET` (0x05) and `DsoTargetPayload` from `protocol.h` - nucleo no longer sends this
- [ ] Remove `dso_target` / `dso_target_received` from `shared_state.h/.cpp`
- [ ] Remove `PACKET_DSO_TARGET` case from `serial_reader.cpp`
- [ ] Remove DSO_TARGET section from `docs/protocol.md`

## HUD - Replace Mock Data
- [ ] Replace mock target direction/distance with real data from state machine (search target vs current FOV)
- [ ] Drive `has_target` from `TelescopeState::SEARCH` or `TelescopeState::FOUND` instead of keyboard toggle (KEY_T)
- [ ] Show selected Messier ID on HUD when in SEARCH/FOUND state

## State Machine Integration
- [ ] React to state transitions beyond just mirroring the byte (e.g. switch HUD layout per state)
- [ ] IDLE: show general sky info (encoder angles, heading, GPS)
- [ ] SEARCH: show target arrow, distance to target
- [ ] FOUND: show confirmation, object info

## Display
- [ ] Render DSO objects in FOV (requires nucleo to send FOV/object data, or raspi computes from encoder+GPS)
- [ ] Connection timeout indicator - if no packets received for N seconds, show disconnect warning

## SD Card / Catalogue
- [ ] Decide whether raspi needs its own catalogue copy or if all lookups happen on the nucleo
- [ ] If raspi-side: load catalogue, render object names/positions in overlay
