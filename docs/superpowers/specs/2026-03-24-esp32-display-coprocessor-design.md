# ESP32 Display Co-Processor Design

## Problem

The current 128x64 SSD1306 OLED is too small to show full parameter names, per-mode P1/P2 labels, all 7 parameters at once, and preset management info. Driving a larger color TFT directly from the Daisy Seed is not viable — flash is at 84%, the I2C OLED already caused a MIDI DMA conflict, and a 320x240 TFT would risk audio glitches.

## Solution

Offload all display rendering to an ESP32-based co-processor (Cheap Yellow Display module) connected to the Daisy Seed over UART. The Daisy sends a compact binary state packet at 30 Hz. The ESP32 renders a rich color UI independently.

## Architecture

```
Daisy Seed (audio DSP)          ESP32 CYD (display only)
┌─────────────────────┐         ┌──────────────────────────┐
│ AudioEngine         │         │ UART Receiver            │
│ ModMode::Process()  │         │   ↓                      │
│ ParamSet pipeline   │         │ DisplayState (diffed)    │
│   ↓                 │         │   ↓                      │
│ UartDisplay         │──UART──→│ LVGL UI Renderer         │
│ (29-byte packet)    │  TX     │   ↓                      │
│   ↓                 │         │ ILI9341 320x240 TFT      │
│ DisplayManager      │         └──────────────────────────┘
│ (OLED, parallel)    │
└─────────────────────┘
```

Both `UartDisplay` and the existing `DisplayManager` (OLED) run in parallel during development. The OLED can be compiled out later via a build flag.

## Communication Protocol

### Physical Layer

- UART 115200 baud, 8N1
- Daisy USART1: D13 (TX) → CYD IO16 (RX2)
- Future bidirectional: D14 (RX) ← CYD IO17 (TX2), not connected initially
- 3.3V logic on both sides, no level shifter needed

### Display State Packet (Daisy → ESP32)

Sent every 33 ms (~30 Hz). Fixed-size, fire-and-forget, no ACK.

```
Byte  Field                   Type       Range / Notes
──────────────────────────────────────────────────────────
0     sync                    uint8      0xA5 (magic byte)
1     version                 uint8      0x01 (protocol version)
2     length                  uint8      payload length (bytes 3..N-1)
3     mode_id                 uint8      0-11
4     sub_mode                uint8      0-15 (Daisy computes from quantized P1)
5     bypass                  uint8      0 = bypassed, 1 = active
6     tempo_source            uint8      0 = none, 1 = tap, 2 = MIDI clock
7     preset_slot             uint8      0-7
8     ui_flags                uint8      bit 0: shift layer active
                                         bit 1: preset-loaded flash
                                         bit 2: preset-saved flash
9-22  params[7]               uint16x7   normalized 0-65535 (maps to 0.0-1.0)
                                         little-endian byte order
23    speed_override_active   uint8      0 or 1
24-27 speed_display           float32    physical speed value for display
                                         formatting (Hz, ms, or ratio)
28    checksum                uint8      XOR of bytes 0-27
──────────────────────────────────────────────────────────
Total: 29 bytes (~2.5 ms at 115200 baud)
```

**Version and length fields** allow the ESP32 receiver to handle future protocol revisions without a coordinated two-repo flash. The receiver reads the 3-byte header, then reads `length` bytes of payload, then validates the checksum. Unknown versions or unexpected lengths are dropped.

**Sub-mode derivation:** The Daisy computes `sub_mode` by quantizing the current P1 value into the mode's sub-mode count. This keeps the mapping logic co-located with the DSP code. The ESP32 only needs a label lookup table indexed by `[mode_id][sub_mode]`.

**Endianness note:** Both Cortex-M7 and ESP32 (Xtensa LX6) are little-endian. The packing code uses `memcpy` from a packed struct to avoid unaligned access issues.

**Error handling:** ESP32 validates sync byte and checksum. Corrupted packets are dropped silently; the next valid packet arrives within 33 ms.

**Bandwidth headroom:** 115200 baud supports ~340 bytes per 33 ms frame. The 27-byte packet uses <8% of available bandwidth, leaving room for future fields (multi-effect chain state, waveform data, etc.).

### Future Command Packet (ESP32 → Daisy, not implemented)

Reserved for touch interaction / preset management. Different sync byte (0x5A) to distinguish from display packets.

```
Byte  Field           Type       Notes
──────────────────────────────────────
0     sync            uint8      0x5A
1     command_type    uint8      MODE_CHANGE, PARAM_SET, PRESET_LOAD, etc.
2-5   payload         4 bytes    command-dependent
6     checksum        uint8      XOR of bytes 0-6
──────────────────────────────────────
Total: 7 bytes
```

## Daisy-Side Changes

### New Struct: `DisplayState`

Both `DisplayManager::Update()` and `UartDisplay::SendState()` consume the same data. A `DisplayState` struct is populated once per main-loop iteration and passed to both:

```cpp
struct DisplayState {
    ModModeId       mode;
    uint8_t         sub_mode;        // Daisy-computed from quantized P1
    ParamSet        params;
    bool            bypass_active;
    uint8_t         tempo_source;    // 0=none, 1=tap, 2=MIDI
    int             preset_slot;
    bool            shift_layer_active;
    PresetUiEvent   preset_event;
    float           speed_display;   // physical speed value
    bool            speed_override_active;
};
```

This eliminates the 10-parameter function signatures and creates a clean seam for the OLED compile-out. The struct also makes packet packing unit-testable in the VST build.

### New Module: `src/display/uart_display.h/.cpp`

- Owns a USART TX handle (DMA-backed, non-blocking)
- `Init()` configures the chosen USART TX pin with DMA
- `SendState(const DisplayState&)` packs state into the 29-byte packet and fires DMA transmit
- Rate-limited to 30 Hz internally (same `DISPLAY_UPDATE_MS` constant)
- Called from the main loop at the same point as `display.Update()`

### Changes to `main.cpp`

- Add `DisplayState` population before display calls
- Add `UartDisplay uart_display;` alongside existing `DisplayManager display;`
- Call both `display.Update(state)` and `uart_display.SendState(state)` each iteration
- Both run in parallel during development

### UART Selection

**The existing MIDI handler uses USART1 (D14 RX).** MIDI is currently disabled because its UART DMA init interfered with the I2C OLED on boot. Two options:

1. **Use USART1 TX-only (D13)** — Since the MIDI conflict was caused by RX DMA init, TX-only may work without triggering the same issue. This must be verified experimentally before committing to this approach.
2. **Use a different USART** — USART6 (available on other Daisy Seed pins) avoids the USART1 conflict entirely and keeps USART1 free for MIDI when it is re-enabled.

**Decision gate:** The first implementation task is to test USART1 TX-only init and verify it does not freeze the I2C OLED. If it does, fall back to USART6.

**Pin map update:** Whichever UART is chosen, add the TX (and future RX) pin to `src/config/pin_map.h` to prevent future conflicts.

### OLED Compile-Out

When the CYD display is validated and the OLED is no longer needed, `DisplayManager` and its dependencies (SSD1306 driver, font tables) can be excluded via a `#define HAS_OLED 1` build flag in `constants.h`. This is out of scope for now but the `DisplayState` struct makes it straightforward.

### Flash Impact

`UartDisplay` adds ~500-800 bytes (DMA init + packet packing). When the OLED is eventually compiled out, `DisplayManager` (including font tables and rendering) is removed — net flash usage decreases.

## ESP32 (CYD) Firmware

### Separate Repository

The ESP32 firmware lives in its own repo. It shares no code with the Daisy firmware. The protocol packet format is documented in both repos.

### Tech Stack

- Arduino framework
- TFT_eSPI (ILI9341 driver, CYD presets available)
- LVGL (UI toolkit with partial-update rendering)

### Architecture

Three responsibilities:

1. **UART Receiver** — reads packets on Serial2 (IO16/IO17), validates sync + checksum, writes to a `DisplayState` struct
2. **State Diffing** — compares new state to previous frame, triggers LVGL widget updates only for changed fields
3. **LVGL UI Renderer** — renders the screen layout, LVGL handles optimized partial redraws to the ILI9341

### Screen Layout (320x240, landscape)

```
+-------------------------------------------------+
|  CHORUS            ON   TAP       > Preset 3    |  Mode name (large), status
+-------------------------------------------------+
|  Speed      Depth       Mix         Tone        |  Row 1: labels
|  3.5 Hz     [====  ]    [====  ]    [====  ]    |  Row 1: value + bars
|                                                 |
|  Feedback   Stages      Level                   |  Row 2: per-mode P1/P2 names
|  [====  ]   [====  ]    [====  ]                |  Row 2: bars
|                                                 |
+-------------------------------------------------+
|  MIDI Clock                                     |  Tempo footer
+-------------------------------------------------+
```

### Per-Mode Label Table

Lives entirely on the ESP32. Maps `mode_id` to human-readable P1/P2 names:

| Mode       | P1 Label    | P2 Label  |
|------------|-------------|-----------|
| Chorus     | Type        | Spread    |
| Flanger    | Type        | Regen     |
| Rotary     | Balance     | Drive     |
| Vibe       | Symmetry    | —         |
| Phaser     | Stages      | Regen     |
| Filter     | Shape       | Resonance |
| Formant    | Vowel Pair  | Resonance |
| VintTrem   | Type        | Shape     |
| PattTrem   | Pattern     | Subdivide |
| AutoSwell  | Sensitivity | —         |
| Destroyer  | Bit Depth   | —         |
| Quadrature | Type        | Carrier   |

Adding or renaming labels is an ESP32-only change — no Daisy firmware update needed.

### Startup and Connection Handling

- On boot, the ESP32 displays a "Connecting..." screen until the first valid packet arrives.
- If no valid packet is received for 500 ms after the last one, the display shows a "Disconnected" state.
- The CYD's onboard RGB LED can be used for diagnostic feedback: steady = connected, blinking = no data.

## Hardware Wiring

```
Daisy Seed              CYD (ESP32)
──────────              ───────────
D13 (USART1 TX)  ────→  IO16 (RX2)
D14 (USART1 RX)  ←────  IO17 (TX2)  [not connected initially]
GND  ─────────────────  GND
```

**Power during development:** CYD powered by its own USB-C. In a final pedal build, 5V tapped from the pedal's internal supply to the CYD's 5V pin.

## Scope Boundaries

**In scope:**
- Daisy-side `UartDisplay` module (TX-only)
- ESP32 firmware with UART receiver and LVGL UI
- Display state protocol (29-byte versioned packet)
- `DisplayState` struct shared by both display outputs
- Parallel operation with existing OLED during development

**Out of scope (future work):**
- Bidirectional communication (touch/navigation commands)
- Multi-effect chaining architecture
- Preset name editing on the display
- OTA firmware updates via ESP32 WiFi
- Removing the OLED from the Daisy firmware

## Risks

| Risk | Severity | Mitigation |
|------|----------|------------|
| USART1 TX-only DMA init may reproduce the MIDI/I2C freeze | CRITICAL | Test TX-only init as the first implementation task. If it freezes, use USART6 instead. |
| MIDI and display both want USART1 when MIDI is re-enabled | HIGH | Decision: either move MIDI to USART2 or move display to USART6. Decide during the UART verification step. |
| Packet corruption on noisy pedal PSU | MEDIUM | XOR checksum + drop-and-wait. At 30 Hz, one lost frame is imperceptible. Can upgrade to CRC-8 later if needed. |
| ESP32 boots slower than Daisy — receives noise before sync | MEDIUM | ESP32 shows "Connecting..." until first valid packet. Daisy sends continuously; no startup handshake needed. |
| Daisy stops sending (crash/cable disconnect) | MEDIUM | ESP32 timeout: if no valid packet for 500 ms, show "Disconnected" state. |
| LVGL rendering too slow on ESP32 | LOW | CYD runs at 240 MHz with hardware SPI. Partial updates keep it fast. Profile early. |
| Two repos drift out of sync on protocol | LOW | Version + length fields in packet header. ESP32 can detect and reject unknown versions. |
| Ground loop noise from dual USB power during development | LOW | Acceptable for dev. Final pedal uses single 5V supply to both boards. |
