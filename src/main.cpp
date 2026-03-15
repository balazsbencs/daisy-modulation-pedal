#include "daisy_seed.h"
#include "config/constants.h"
#include "config/mod_mode_id.h"
#include "config/pin_map.h"
#include "hardware/controls.h"
#include "audio/audio_engine.h"
#include "audio/bypass.h"
#include "params/param_set.h"
#include "params/param_map.h"
#include "params/param_range.h"
#include "params/param_id.h"
#include "modes/mode_registry.h"
#include "midi/midi_handler.h"
#include "display/display_manager.h"
#include "tempo/tempo_sync.h"
#include "presets/preset_manager.h"

using namespace daisy;

// -- Hardware objects ---------------------------------------------------------
static DaisySeed hw;
static GPIO      relay;
static GPIO      led_bypass;

// -- Subsystem singletons -----------------------------------------------------
static pedal::Controls         controls;
static pedal::AudioEngine      audio_engine;
static pedal::ModeRegistry     mode_registry;
static pedal::MidiHandlerPedal midi_handler;
static pedal::DisplayManager   display;
static pedal::TempoSync        tempo_sync;
static pedal::PresetManager    preset_manager;

// -- Mutable main-loop state --------------------------------------------------
static pedal::ModModeId current_mode = pedal::ModModeId::Chorus;
static pedal::Bypass    bypass_disp;

namespace {

struct ParamEditState {
    float norm[pedal::NUM_PARAMS];
};

static float Clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

static void InitDefaultParamNorm(ParamEditState& st) {
    st.norm[0] = 0.3f;  // Speed: moderate LFO rate
    st.norm[1] = 0.5f;  // Depth
    st.norm[2] = 0.5f;  // Mix
    st.norm[3] = 0.5f;  // Tone: flat
    st.norm[4] = 0.0f;  // P1
    st.norm[5] = 0.0f;  // P2
    st.norm[6] = 0.5f;  // Level: maps to 1.0 gain (unity at 0.5 norm since range is 0..2)
}

static int WrapSlotIndex(int slot) {
    if (slot < 0) {
        return pedal::PRESET_SLOT_COUNT - 1;
    }
    if (slot >= pedal::PRESET_SLOT_COUNT) {
        return 0;
    }
    return slot;
}

static void ApplyEncoderEdit(float& target,
                             int delta,
                             uint32_t now,
                             uint32_t& last_tick_ms) {
    if (delta == 0) {
        return;
    }

    const int dir   = (delta > 0) ? 1 : -1;
    int       steps = (delta > 0) ? delta : -delta;
    while (steps-- > 0) {
        const bool fast = (last_tick_ms != 0)
                       && (now - last_tick_ms <= pedal::ENCODER_FAST_WINDOW_MS);
        const float step = fast ? pedal::PARAM_STEP_FAST : pedal::PARAM_STEP_SLOW;
        target           = Clamp01(target + (dir > 0 ? step : -step));
        last_tick_ms     = now;
    }
}

static pedal::ParamSet BuildParams(const ParamEditState& edit,
                                   pedal::ModModeId      mode,
                                   float                 speed_override) {
    using namespace pedal;

    ParamSet ps;
    ps.speed = map_param(edit.norm[0], get_param_range(mode, ParamId::Speed));
    ps.depth = map_param(edit.norm[1], get_param_range(mode, ParamId::Depth));
    ps.mix   = map_param(edit.norm[2], get_param_range(mode, ParamId::Mix));
    ps.tone  = map_param(edit.norm[3], get_param_range(mode, ParamId::Tone));
    ps.p1    = map_param(edit.norm[4], get_param_range(mode, ParamId::P1));
    ps.p2    = map_param(edit.norm[5], get_param_range(mode, ParamId::P2));
    ps.level = map_param(edit.norm[6], get_param_range(mode, ParamId::Level));

    if (speed_override > 0.0f) {
        const float rate_hz = 1.0f / speed_override;
        ps.speed = (rate_hz < 10.0f) ? rate_hz : 10.0f;
    }

    return ps;
}

static void ActivateMode(pedal::ModModeId new_id) {
    if (new_id == current_mode) {
        return;
    }
    current_mode = new_id;
    pedal::ModMode* m = mode_registry.get(new_id);
    m->Reset();
    audio_engine.SetMode(m);
}

} // namespace

int main() {
    hw.Init();
    hw.SetAudioBlockSize(pedal::BLOCK_SIZE);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    relay.Init(pedal::pins::RELAY, GPIO::Mode::OUTPUT);
    led_bypass.Init(pedal::pins::LED_BYPASS, GPIO::Mode::OUTPUT);

    controls.Init(hw);
    mode_registry.Init();
    audio_engine.Init(&hw);
    midi_handler.Init();
    display.Init();
    tempo_sync.Init();
    preset_manager.Init(hw);

    ParamEditState param_edit{};
    InitDefaultParamNorm(param_edit);

    pedal::ModModeId boot_mode = current_mode;
    float            boot_norm[pedal::NUM_PARAMS]{};
    if (preset_manager.LoadActive(boot_mode, boot_norm)) {
        current_mode = boot_mode;
        for (int i = 0; i < pedal::NUM_PARAMS; ++i) {
            param_edit.norm[i] = Clamp01(boot_norm[i]);
        }
    }

    bypass_disp.Init();
    audio_engine.SetBypass(false);
    relay.Write(true);
    led_bypass.Write(true);

    pedal::ModMode* initial_mode = mode_registry.get(current_mode);
    initial_mode->Reset();
    audio_engine.SetMode(initial_mode);

    hw.StartAudio(pedal::AudioEngine::AudioCallback);

    uint32_t param_last_tick_ms[pedal::NUM_PARAMS]{};
    bool     preset_tap_chord_active = false;
    bool     preset_long_fired       = false;
    pedal::DisplayManager::PresetUiEvent preset_event
        = pedal::DisplayManager::PresetUiEvent::None;
    uint32_t preset_event_until = 0;

    while (true) {
        const uint32_t now = System::GetNow();

        controls.Poll();
        const pedal::ControlState& ctrl = controls.state();

        // Mode encoder controls preset slot while held, else modulation mode.
        if (ctrl.mode_encoder_increment != 0) {
            if (ctrl.mode_encoder_held) {
                int slot = preset_manager.GetActiveSlot();
                int inc  = ctrl.mode_encoder_increment;
                const int dir = inc > 0 ? 1 : -1;
                while (inc != 0) {
                    slot = WrapSlotIndex(slot + dir);
                    inc -= dir;
                }
                preset_manager.SetActiveSlot(slot);
            } else {
                int idx = static_cast<int>(current_mode) + ctrl.mode_encoder_increment;
                if (idx < 0) {
                    idx = pedal::NUM_MODES - 1;
                }
                if (idx >= pedal::NUM_MODES) {
                    idx = 0;
                }
                ActivateMode(static_cast<pedal::ModModeId>(idx));
            }
        }

        // 4 parameter encoders with shift layer for the last 3 params.
        for (int e = 0; e < 4; ++e) {
            const int delta = ctrl.param_encoder_increment[e];
            if (delta == 0) {
                continue;
            }

            int param_index = -1;
            if (!ctrl.mode_encoder_held) {
                static const int kPrimaryMap[4] = {0, 1, 2, 3};
                param_index = kPrimaryMap[e];
            } else {
                static const int kShiftMap[4] = {4, 5, 6, -1};
                param_index = kShiftMap[e];
            }

            if (param_index >= 0 && param_index < pedal::NUM_PARAMS) {
                ApplyEncoderEdit(param_edit.norm[param_index],
                                 delta,
                                 now,
                                 param_last_tick_ms[param_index]);
            }
        }

        if (ctrl.bypass_pressed) {
            bypass_disp.Toggle();
            const bool active = bypass_disp.IsActive();
            audio_engine.SetBypass(!active);
            relay.Write(active);
            led_bypass.Write(active);
        }

        // Chorded preset actions: hold mode-encoder switch + use tap switch.
        if (ctrl.tap_pressed) {
            if (ctrl.mode_encoder_held) {
                preset_tap_chord_active = true;
                preset_long_fired       = false;
            } else {
                tempo_sync.OnTap(now);
            }
        }

        if (preset_tap_chord_active
            && ctrl.tap_held
            && !preset_long_fired
            && ctrl.tap_held_ms >= pedal::PRESET_HOLD_MS) {
            if (preset_manager.SaveSlot(preset_manager.GetActiveSlot(),
                                        current_mode,
                                        param_edit.norm)) {
                preset_event       = pedal::DisplayManager::PresetUiEvent::Saved;
                preset_event_until = now + pedal::PRESET_STATUS_MS;
            }
            preset_long_fired = true;
        }

        if (preset_tap_chord_active && ctrl.tap_released) {
            if (!preset_long_fired) {
                pedal::ModModeId loaded_mode = current_mode;
                float            loaded_norm[pedal::NUM_PARAMS]{};
                if (preset_manager.LoadSlot(preset_manager.GetActiveSlot(),
                                            loaded_mode,
                                            loaded_norm)) {
                    for (int i = 0; i < pedal::NUM_PARAMS; ++i) {
                        param_edit.norm[i] = Clamp01(loaded_norm[i]);
                    }
                    ActivateMode(loaded_mode);
                    preset_event       = pedal::DisplayManager::PresetUiEvent::Loaded;
                    preset_event_until = now + pedal::PRESET_STATUS_MS;
                }
            }
            preset_tap_chord_active = false;
            preset_long_fired       = false;
        }

        pedal::MidiState midi_state{};
        midi_handler.Poll(midi_state);

        if (midi_state.clock_tick) {
            tempo_sync.OnMidiClock(now);
        }
        if (midi_state.clock_stop) {
            tempo_sync.OnMidiStop();
        }

        if (midi_state.program_change >= 0
            && midi_state.program_change < pedal::NUM_MODES) {
            ActivateMode(static_cast<pedal::ModModeId>(midi_state.program_change));
        }

        // MIDI CC writes directly into editable normalized state.
        for (int i = 0; i < pedal::NUM_PARAMS; ++i) {
            if (midi_state.cc_received[i]) {
                param_edit.norm[i] = Clamp01(midi_state.cc_normalized[i]);
            }
        }

        tempo_sync.Process(now);

        const float speed_override = tempo_sync.GetOverrideSeconds();
        const pedal::ParamSet params = BuildParams(param_edit, current_mode, speed_override);
        audio_engine.SetParams(params);

        if (preset_event != pedal::DisplayManager::PresetUiEvent::None
            && now >= preset_event_until) {
            preset_event = pedal::DisplayManager::PresetUiEvent::None;
        }

        display.Update(current_mode,
                       params,
                       bypass_disp,
                       tempo_sync,
                       preset_manager.GetActiveSlot(),
                       ctrl.mode_encoder_held,
                       preset_event,
                       now);

        System::Delay(1);
    }
}
