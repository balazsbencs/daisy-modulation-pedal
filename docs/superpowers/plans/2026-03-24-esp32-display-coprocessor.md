# ESP32 Display Co-Processor (Daisy Side) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add UART-based display state transmission from the Daisy Seed to an ESP32 CYD co-processor, running in parallel with the existing OLED display.

**Architecture:** A new `UartDisplay` module packs the current pedal state into a 29-byte binary packet and transmits it over USART1 TX (DMA, non-blocking) at 30 Hz. A shared `DisplayState` struct decouples both display outputs from the main loop's raw variables. The existing `DisplayManager` (OLED) continues to run alongside — no existing behavior is changed.

**Tech Stack:** C++ (ARM Cortex-M7), libDaisy `UartHandler` (DMA TX), existing `ParamSet`/`Bypass`/`TempoSync` types.

**Spec:** `docs/superpowers/specs/2026-03-24-esp32-display-coprocessor-design.md`

---

## File Structure

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `src/display/display_state.h` | `DisplayState` struct + `PresetUiEvent` enum (shared by both display outputs) |
| Create | `src/display/display_protocol.h` | Protocol constants (sync byte, version, packet size, field offsets) and `PackDisplayState()` pure function |
| Create | `src/display/uart_display.h` | `UartDisplay` class declaration |
| Create | `src/display/uart_display.cpp` | `UartDisplay::Init()` and `SendState()` implementation |
| Modify | `src/display/display_manager.h` | Move `PresetUiEvent` to `display_state.h`, change `Update()` signature to take `const DisplayState&` |
| Modify | `src/display/display_manager.cpp` | Adapt `Update()` and `Render()` to use `DisplayState` fields |
| Modify | `src/config/pin_map.h` | Add `UART_DISPLAY_TX` pin constant |
| Modify | `src/main.cpp` | Build `DisplayState`, call both `display.Update()` and `uart_display.SendState()` |
| Modify | `Makefile` | Add `src/display/uart_display.cpp` to `CPP_SOURCES` |

---

## Task 1: Define `DisplayState` struct and `PresetUiEvent` enum

Extract a shared data struct so both display outputs consume the same type.

**Files:**
- Create: `src/display/display_state.h`
- Modify: `src/display/display_manager.h:20-24` (remove `PresetUiEvent` from class body, include new header)

- [ ] **Step 1: Create `src/display/display_state.h`**

```cpp
#pragma once
#include "../config/mod_mode_id.h"
#include <cstdint>

namespace pedal {

enum class PresetUiEvent : uint8_t {
    None,
    Loaded,
    Saved,
};

/// Snapshot of all UI-relevant state, populated once per main-loop iteration.
/// Consumed by both DisplayManager (OLED) and UartDisplay (ESP32).
struct DisplayState {
    ModModeId       mode;
    uint8_t         sub_mode;               // Daisy-computed from quantized P2
    bool            bypass_active;
    uint8_t         tempo_source;           // 0=none, 1=tap, 2=MIDI clock
    int             preset_slot;
    bool            shift_layer_active;
    PresetUiEvent   preset_event;
    float           param_norm[7];          // normalized [0,1] for bar rendering
    float           speed_display;          // physical speed value for formatting
    bool            speed_override_active;
    int             mode_encoder_delta;     // accumulated since last frame (OLED only)
    int             param_encoder_delta[4]; // accumulated since last frame (OLED only)
    uint32_t        now_ms;
};

} // namespace pedal
```

- [ ] **Step 2: Update `display_manager.h` to use shared `PresetUiEvent`**

Remove the `PresetUiEvent` enum from the `DisplayManager` class body. Add `#include "display_state.h"` at the top. Change:
- `Update()` signature: keep the existing signature for now (it will be refactored in Task 4)
- Use `pedal::PresetUiEvent` instead of `DisplayManager::PresetUiEvent`

In `display_manager.h`:
- Remove lines 20-24 (the `enum class PresetUiEvent` block)
- Add `#include "display_state.h"` after the existing includes
- Replace all `PresetUiEvent` references with `pedal::PresetUiEvent`

- [ ] **Step 3: Update `display_manager.cpp` to compile with moved enum**

Replace `DisplayManager::PresetUiEvent` with `PresetUiEvent` (now at namespace scope). No functional change.

- [ ] **Step 4: Update `main.cpp` to use the new enum location**

Change:
```cpp
// Before:
pedal::DisplayManager::PresetUiEvent preset_event
    = pedal::DisplayManager::PresetUiEvent::None;
// After:
pedal::PresetUiEvent preset_event = pedal::PresetUiEvent::None;
```

Apply the same replacement for all other `pedal::DisplayManager::PresetUiEvent` references in `main.cpp` (there are 4 total: declaration, `::None`, `::Saved`, `::Loaded`).

- [ ] **Step 5: Build and verify**

Run: `make -j4`
Expected: Clean build, zero warnings. No behavioral change.

- [ ] **Step 6: Commit**

```bash
git add src/display/display_state.h src/display/display_manager.h src/display/display_manager.cpp src/main.cpp
git commit -m "refactor: extract DisplayState struct and PresetUiEvent to shared header"
```

---

## Task 2: Define the packet protocol constants and packing function

A pure function that converts `DisplayState` → 29-byte wire packet. No hardware dependency — testable on desktop.

**Files:**
- Create: `src/display/display_protocol.h`

- [ ] **Step 1: Create `src/display/display_protocol.h`**

```cpp
#pragma once
#include "display_state.h"
#include <cstdint>
#include <cstring>

namespace pedal {
namespace protocol {

constexpr uint8_t SYNC_BYTE       = 0xA5;
constexpr uint8_t PROTOCOL_VERSION = 0x01;
constexpr uint8_t PAYLOAD_LENGTH  = 25;  // bytes 3..27
constexpr uint8_t PACKET_SIZE     = 29;  // total packet

/// Pack a DisplayState into a 29-byte wire packet.
/// Returns the number of bytes written (always PACKET_SIZE).
inline uint8_t PackDisplayState(const DisplayState& state, uint8_t* out) {
    out[0] = SYNC_BYTE;
    out[1] = PROTOCOL_VERSION;
    out[2] = PAYLOAD_LENGTH;
    out[3] = static_cast<uint8_t>(state.mode);
    out[4] = state.sub_mode;
    out[5] = state.bypass_active ? 1 : 0;
    out[6] = state.tempo_source;
    out[7] = static_cast<uint8_t>(state.preset_slot & 0xFF);

    uint8_t flags = 0;
    if (state.shift_layer_active)              flags |= 0x01;
    if (state.preset_event == PresetUiEvent::Loaded) flags |= 0x02;
    if (state.preset_event == PresetUiEvent::Saved)  flags |= 0x04;
    out[8] = flags;

    // 7 params as uint16 little-endian (normalized 0..65535)
    for (int i = 0; i < 7; ++i) {
        float clamped = state.param_norm[i];
        if (clamped < 0.0f) clamped = 0.0f;
        if (clamped > 1.0f) clamped = 1.0f;
        uint16_t val = static_cast<uint16_t>(clamped * 65535.0f);
        out[9 + i * 2]     = static_cast<uint8_t>(val & 0xFF);
        out[9 + i * 2 + 1] = static_cast<uint8_t>(val >> 8);
    }

    out[23] = state.speed_override_active ? 1 : 0;

    // speed_display as float32 little-endian
    std::memcpy(&out[24], &state.speed_display, 4);

    // XOR checksum over bytes 0..27
    uint8_t cksum = 0;
    for (int i = 0; i < 28; ++i) {
        cksum ^= out[i];
    }
    out[28] = cksum;

    return PACKET_SIZE;
}

} // namespace protocol
} // namespace pedal
```

- [ ] **Step 2: Build and verify**

Run: `make -j4`
Expected: Clean build. The header is included by no one yet, but it should compile if included.

- [ ] **Step 3: Commit**

```bash
git add src/display/display_protocol.h
git commit -m "feat: add display protocol constants and packet packing function"
```

---

## Task 3: Add UART display TX pin to pin map

**Files:**
- Modify: `src/config/pin_map.h:26-29`

- [ ] **Step 1: Add pin constant**

After the `LED_BYPASS` line (line 29), add:

```cpp
// UART display co-processor (ESP32 CYD) — USART1 TX
constexpr daisy::Pin UART_DISPLAY_TX = daisy::seed::D13;
```

- [ ] **Step 2: Build and verify**

Run: `make -j4`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/config/pin_map.h
git commit -m "feat: add UART display TX pin (D13) to pin map"
```

---

## Task 4: Refactor DisplayManager to accept `DisplayState`

Change the `Update()` and `Render()` signatures to take `const DisplayState&` instead of 10 individual parameters. This makes both display outputs consume the same struct.

**Files:**
- Modify: `src/display/display_manager.h:35-57`
- Modify: `src/display/display_manager.cpp:84-133`
- Modify: `src/main.cpp:318-327`

- [ ] **Step 1: Change `Update()` and `Render()` signatures in `display_manager.h`**

Replace the `Update()` declaration:

```cpp
void Update(const DisplayState& state);
```

Replace the `Render()` declaration:

```cpp
void Render(const DisplayState& state);
```

Remove `#include "../audio/bypass.h"` and `#include "../tempo/tempo_sync.h"` if they are no longer needed after this change (they won't be — `DisplayState` already carries the extracted fields).

- [ ] **Step 2: Update `Update()` in `display_manager.cpp`**

```cpp
void DisplayManager::Update(const DisplayState& state) {
    pending_mode_delta_ += state.mode_encoder_delta;
    for (int i = 0; i < 4; ++i) {
        pending_param_delta_[i] += state.param_encoder_delta[i];
    }

    if (state.now_ms - last_update_ms_ < DISPLAY_UPDATE_MS) {
        return;
    }
    last_update_ms_ = state.now_ms;

    // Build a local copy with accumulated deltas for rendering
    DisplayState render_state = state;
    render_state.mode_encoder_delta = pending_mode_delta_;
    for (int i = 0; i < 4; ++i) {
        render_state.param_encoder_delta[i] = pending_param_delta_[i];
    }

    Render(render_state);

    pending_mode_delta_ = 0;
    for (int i = 0; i < 4; ++i) {
        pending_param_delta_[i] = 0;
    }
}
```

- [ ] **Step 3: Update `Render()` in `display_manager.cpp`**

Change the signature to `void DisplayManager::Render(const DisplayState& state)` and replace all parameter references:
- `mode` → `state.mode`
- `params.speed` → `state.speed_display` (for `FormatSpeed`)
- `params.depth` → `state.param_norm[1]` (already [0,1])
- `params.mix` → `state.param_norm[2]`
- `params.tone` → `state.param_norm[3]`
- `params.p1` → `state.param_norm[4]`
- `params.p2` → `state.param_norm[5]`
- `params.level / 2.0f` → `state.param_norm[6]` (already normalized in DisplayState)
- `bypass.IsActive()` → `state.bypass_active`
- `tempo.HasMidiClock()` / `tempo.HasTap()` → check `state.tempo_source` (2 = MIDI, 1 = tap)
- `preset_slot` → `state.preset_slot`
- `shift_layer_active` → `state.shift_layer_active`
- `preset_event` → `state.preset_event`
- `now_ms` → `state.now_ms`

Note: The `FormatSpeed` helper uses `mode` and `speed` — pass `state.mode` and `state.speed_display`.

- [ ] **Step 4: Update `main.cpp` to populate and pass `DisplayState`**

Replace the `display.Update(...)` call (lines 318-327) with:

```cpp
// --- Build shared display state ---
pedal::DisplayState disp_state{};
disp_state.mode                  = current_mode;
disp_state.sub_mode              = 0;  // placeholder — computed in Task 6
disp_state.bypass_active         = bypass_disp.IsActive();
disp_state.tempo_source          = tempo_sync.HasMidiClock() ? 2
                                 : tempo_sync.HasTap()       ? 1
                                                             : 0;
disp_state.preset_slot           = preset_manager.GetActiveSlot();
disp_state.shift_layer_active    = ctrl.mode_encoder_held;
disp_state.preset_event          = preset_event;
disp_state.speed_display         = params.speed;
disp_state.speed_override_active = speed_override > 0.0f;
disp_state.mode_encoder_delta    = ctrl.mode_encoder_increment;
disp_state.now_ms                = now;

for (int i = 0; i < 7; ++i) {
    disp_state.param_norm[i] = param_edit.norm[i];
}
for (int i = 0; i < 4; ++i) {
    disp_state.param_encoder_delta[i] = ctrl.param_encoder_increment[i];
}

display.Update(disp_state);
```

- [ ] **Step 5: Remove unused includes from `display_manager.h`**

Remove these includes if they are no longer referenced:
- `#include "../params/param_set.h"`
- `#include "../audio/bypass.h"`
- `#include "../tempo/tempo_sync.h"`

Keep `#include "display_state.h"` (already added in Task 1).

- [ ] **Step 6: Build and verify**

Run: `make -j4`
Expected: Clean build, zero warnings. OLED display still works identically (flash to hardware to verify if possible).

- [ ] **Step 7: Commit**

```bash
git add src/display/display_manager.h src/display/display_manager.cpp src/main.cpp
git commit -m "refactor: change DisplayManager to accept DisplayState struct"
```

---

## Task 5: Implement `UartDisplay` module

The new UART display output. Packs state and fires DMA TX at 30 Hz.

**Files:**
- Create: `src/display/uart_display.h`
- Create: `src/display/uart_display.cpp`
- Modify: `Makefile:30` (add source file)

- [ ] **Step 1: Create `src/display/uart_display.h`**

```cpp
#pragma once
#include "display_state.h"
#include "per/uart.h"
#include <cstdint>

namespace pedal {

/// Sends display state to an ESP32 CYD co-processor over UART.
/// Transmits a 29-byte binary packet at ~30 Hz using DMA (non-blocking).
class UartDisplay {
public:
    /// Initialize USART1 TX on the display TX pin.
    void Init();

    /// Pack the current state and transmit if the rate-limit interval has elapsed.
    /// Call once per main-loop iteration.
    void SendState(const DisplayState& state, uint32_t now_ms);

private:
    daisy::UartHandler uart_;
    uint32_t           last_send_ms_ = 0;
    volatile bool      tx_busy_      = false;
    uint8_t            tx_buf_[32]   = {};
};

} // namespace pedal
```

- [ ] **Step 2: Create `src/display/uart_display.cpp`**

```cpp
#include "uart_display.h"
#include "display_protocol.h"
#include "../config/constants.h"
#include "../config/pin_map.h"

using namespace daisy;

namespace pedal {

void UartDisplay::Init() {
    UartHandler::Config cfg;
    cfg.periph              = UartHandler::Config::Peripheral::USART_1;
    cfg.mode                = UartHandler::Config::Mode::TX;
    cfg.pin_config.tx       = pins::UART_DISPLAY_TX;
    cfg.pin_config.rx       = {DSY_GPIOX, 0};  // unused — TX only
    cfg.baudrate            = 115200;
    cfg.stopbits            = UartHandler::Config::StopBits::BITS_1;
    cfg.parity              = UartHandler::Config::Parity::NONE;
    cfg.wordlength          = UartHandler::Config::WordLength::BITS_8;

    uart_.Init(cfg);
    tx_busy_ = false;
}

void UartDisplay::SendState(const DisplayState& state, uint32_t now_ms) {
    if (now_ms - last_send_ms_ < DISPLAY_UPDATE_MS) {
        return;
    }
    if (tx_busy_) {
        return;  // previous DMA transfer still in flight
    }

    last_send_ms_ = now_ms;

    protocol::PackDisplayState(state, tx_buf_);

    tx_busy_ = true;
    auto result = uart_.DmaTransmit(
        tx_buf_,
        protocol::PACKET_SIZE,
        nullptr,  // no start callback
        [](void* ctx, UartHandler::Result /*r*/) {
            static_cast<UartDisplay*>(ctx)->tx_busy_ = false;
        },
        this
    );

    if (result != UartHandler::Result::OK) {
        tx_busy_ = false;  // DMA setup failed — allow retry next frame
    }
}

} // namespace pedal
```

- [ ] **Step 3: Add source to Makefile**

In the `CPP_SOURCES` list, after the `src/display/display_manager.cpp \` line, add:

```
    src/display/uart_display.cpp \
```

- [ ] **Step 4: Build and verify**

Run: `make -j4`
Expected: Clean build. `UartDisplay` is compiled but not yet called from `main.cpp`.

- [ ] **Step 5: Commit**

```bash
git add src/display/uart_display.h src/display/uart_display.cpp Makefile
git commit -m "feat: add UartDisplay module for ESP32 co-processor communication"
```

---

## Task 6: Wire `UartDisplay` into `main.cpp` and compute `sub_mode`

Connect the new display output to the main loop, running alongside the OLED.

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add include and instance**

At the top of `main.cpp`, add:
```cpp
#include "display/uart_display.h"
```

In the static singletons section (after line 30), add:
```cpp
static pedal::UartDisplay uart_display;
```

- [ ] **Step 2: Add sub-mode computation helper**

Inside the anonymous namespace in `main.cpp` (before `main()`), add:

```cpp
/// Compute the display sub-mode index from the current mode and normalized P2.
/// Mirrors the quantization logic in each mode's Prepare() method.
/// IMPORTANT: These factors must match the Prepare() in each mode source file.
static uint8_t ComputeSubMode(pedal::ModModeId mode, float p2_norm) {
    using Id = pedal::ModModeId;
    switch (mode) {
        case Id::Chorus:     return static_cast<uint8_t>(p2_norm * 4.999f);  // 0-4 (chorus_mode.cpp:37)
        case Id::Flanger:    return static_cast<uint8_t>(p2_norm * 5.999f);  // 0-5 (flanger_mode.cpp:28)
        case Id::Phaser:     return static_cast<uint8_t>(p2_norm * 6.999f);  // 0-6, 6=Barber Pole (phaser_mode.cpp:35)
        case Id::Filter:     return static_cast<uint8_t>(p2_norm * 7.999f);  // 0-7 waveshape index (filter_mode.cpp:25)
        case Id::Formant:    return static_cast<uint8_t>(p2_norm * 4.999f);  // 0-4 vowel index (formant_mode.cpp:34)
        case Id::VintTrem:   return static_cast<uint8_t>(p2_norm * 2.999f);  // 0-2 (vintage_trem_mode.cpp:17)
        case Id::PattTrem:   return static_cast<uint8_t>(p2_norm * 2.999f);  // 0-2 subdivision (pattern_trem_mode.cpp:25)
        case Id::Quadrature: return static_cast<uint8_t>(p2_norm * 3.999f);  // 0-3 (quadrature_mode.cpp:41)
        default:             return 0;  // Rotary, Vibe, AutoSwell, Destroyer: no sub-modes
    }
}
```

**Important:** These quantization factors must match the `Prepare()` methods in each mode. If a mode's sub-mode count changes, update this function too. Verified source locations:
- `src/modes/chorus_mode.cpp:37` — `params.p2 * 4.999f` (5 sub-modes)
- `src/modes/flanger_mode.cpp:28` — `params.p2 * 5.999f` (6 sub-modes)
- `src/modes/phaser_mode.cpp:35` — `params.p2 * 6.999f` (7 sub-modes, 6=Barber Pole)
- `src/modes/filter_mode.cpp:25` — `params.p2 * 7.999f` (8 waveshapes)
- `src/modes/formant_mode.cpp:34` — `params.p2 * 4.999f` (5 vowels)
- `src/modes/vintage_trem_mode.cpp:17` — `params.p2 * 2.999f` (3 types)
- `src/modes/pattern_trem_mode.cpp:25` — `params.p2 * 2.999f` (3 subdivisions)
- `src/modes/quadrature_mode.cpp:41` — `params.p2 * 3.999f` (4 sub-modes)

- [ ] **Step 3: Initialize `UartDisplay` in `main()`**

After `display.Init();` (around line 142), add:

```cpp
uart_display.Init();
```

- [ ] **Step 4: Populate `sub_mode` and call `uart_display.SendState()`**

In the main loop, after the `display.Update(disp_state);` line added in Task 4, add:

```cpp
disp_state.sub_mode = ComputeSubMode(current_mode, param_edit.norm[5]);
uart_display.SendState(disp_state, now);
```

(Note: `param_edit.norm[5]` is the normalized P2 value. Sub-mode is set after the OLED call because the OLED doesn't use it, but it must be set before the UART call.)

- [ ] **Step 5: Build and verify**

Run: `make -j4`
Expected: Clean build. Both display outputs are now active.

- [ ] **Step 6: Check flash usage**

Run: `arm-none-eabi-size build/modulation.elf`
Expected: Text section should increase by ~500-800 bytes from the previous build. Document the before/after sizes.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire UartDisplay into main loop alongside OLED display"
```

---

## Task 7: Hardware verification (CRITICAL decision gate)

This task must be done with actual hardware. Flash the firmware and verify USART1 TX-only DMA init does not interfere with the I2C OLED.

**Files:** None (hardware test only)

- [ ] **Step 1: Flash firmware to Daisy Seed**

Run: `make program-dfu` (hold BOOT, tap RESET, release BOOT first)

- [ ] **Step 2: Verify OLED still works**

Expected: The OLED should display the mode name, parameter bars, and status indicators exactly as before. If the OLED freezes or fails to initialize, the USART1 DMA is conflicting — proceed to Step 4.

- [ ] **Step 3: Verify UART TX output**

Connect a logic analyzer or USB-serial adapter to D13 (TX) and GND. Open a serial monitor at 115200 baud.

Expected: You should see a stream of 29-byte packets starting with `0xA5 0x01 0x19` (sync, version, length=25) at approximately 30 Hz. Turning encoders should change the parameter bytes.

If both OLED and UART work: **The USART1 approach is validated. Skip Step 4. Commit and continue.**

- [ ] **Step 4: (Only if OLED freezes) Fall back to USART6**

If USART1 TX-only causes the same freeze:

1. In `pin_map.h`, change: `constexpr daisy::Pin UART_DISPLAY_TX = daisy::seed::D25;` (USART6 TX)
2. In `uart_display.cpp`, change: `cfg.periph = UartHandler::Config::Peripheral::USART_6;`
3. Rebuild, reflash, and retest.

Document which UART was used in a comment in `uart_display.cpp`.

- [ ] **Step 5: Commit verification result**

```bash
git add -A
git commit -m "feat: verify UART display co-processor on hardware

USART[1|6] TX confirmed working alongside I2C OLED.
No DMA conflicts observed."
```

---

## Summary

After all 7 tasks, the Daisy firmware:

1. Populates a `DisplayState` struct each main-loop iteration
2. Passes it to the existing OLED `DisplayManager` (unchanged behavior)
3. Packs it into a 29-byte versioned protocol packet
4. Transmits via UART DMA to the ESP32 CYD at 30 Hz
5. Has zero impact on the audio ISR path

The ESP32 CYD firmware (separate repo, separate plan) will receive and render these packets.
