#include "preset_manager.h"
#include <cstring>
#include <new>

namespace pedal {

namespace {
constexpr uint32_t kPresetAddressOffset = 0;

static float Clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}
} // namespace

bool PresetManager::PresetSlot::operator!=(const PresetSlot& other) const {
    if (valid != other.valid || mode != other.mode) {
        return true;
    }
    for (int i = 0; i < NUM_PARAMS; ++i) {
        if (norm[i] != other.norm[i]) {
            return true;
        }
    }
    return false;
}

bool PresetManager::PresetStorageState::operator!=(const PresetStorageState& other) const {
    if (magic != other.magic || version != other.version
        || active_slot != other.active_slot || crc != other.crc) {
        return true;
    }
    for (int i = 0; i < PRESET_SLOT_COUNT; ++i) {
        if (slots[i] != other.slots[i]) {
            return true;
        }
    }
    return false;
}

uint32_t PresetManager::ComputeCrc(const PresetStorageState& state) {
    uint32_t hash = 2166136261u; // FNV-1a

    // Hash header fields explicitly (without padding)
    auto hash_bytes = [&](const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            hash ^= data[i];
            hash *= 16777619u;
        }
    };

    hash_bytes(reinterpret_cast<const uint8_t*>(&state.magic), sizeof(state.magic));
    hash_bytes(reinterpret_cast<const uint8_t*>(&state.version), sizeof(state.version));
    hash_bytes(reinterpret_cast<const uint8_t*>(&state.active_slot), sizeof(state.active_slot));

    // Hash preset slots
    for (int i = 0; i < PRESET_SLOT_COUNT; ++i) {
        const PresetSlot& slot = state.slots[i];
        hash_bytes(reinterpret_cast<const uint8_t*>(&slot.valid), sizeof(slot.valid));
        hash_bytes(reinterpret_cast<const uint8_t*>(&slot.mode), sizeof(slot.mode));
        hash_bytes(reinterpret_cast<const uint8_t*>(slot.norm), sizeof(slot.norm));
    }

    return hash;
}

void PresetManager::FillDefaultSlot(PresetSlot& slot) {
    slot.valid = 1;
    slot.mode  = static_cast<uint8_t>(ModModeId::Chorus);
    slot.norm[0] = 0.3f; // speed
    slot.norm[1] = 0.5f; // depth
    slot.norm[2] = 0.5f; // mix
    slot.norm[3] = 0.5f; // tone
    slot.norm[4] = 0.0f; // p1
    slot.norm[5] = 0.0f; // p2
    slot.norm[6] = 0.5f; // level
}

void PresetManager::Init(daisy::DaisySeed& hw) {
    if (!storage_initialized_) {
        new (storage_buf_) StorageType(hw.qspi);
        storage_initialized_ = true;
    }

    PresetStorageState defaults{};
    defaults.magic       = kMagic;
    defaults.version     = kVersion;
    defaults.active_slot = 0;
    for (int i = 0; i < PRESET_SLOT_COUNT; ++i) {
        FillDefaultSlot(defaults.slots[i]);
    }
    defaults.crc = ComputeCrc(defaults);

    storage().Init(defaults, kPresetAddressOffset);

    PresetStorageState& st = storage().GetSettings();
    if (!IsValidState(st)) {
        st = defaults;
        storage().Save();
    }

    active_slot_ = NormalizeSlotIndex(static_cast<int>(st.active_slot));
}

void PresetManager::SetActiveSlot(int slot) {
    active_slot_ = NormalizeSlotIndex(slot);
}

bool PresetManager::LoadActive(ModModeId& out_mode, float out_norm[NUM_PARAMS]) {
    return LoadSlot(active_slot_, out_mode, out_norm);
}

bool PresetManager::LoadSlot(int slot,
                             ModModeId& out_mode,
                             float out_norm[NUM_PARAMS]) {
    const int s = NormalizeSlotIndex(slot);

    PresetStorageState& st = storage().GetSettings();
    if (!IsValidState(st)) {
        return false;
    }

    const PresetSlot& ps = st.slots[s];
    if (!ps.valid) {
        return false;
    }

    out_mode = static_cast<ModModeId>(ps.mode % NUM_MODES);
    for (int i = 0; i < NUM_PARAMS; ++i) {
        out_norm[i] = Clamp01(ps.norm[i]);
    }

    // Only save if active_slot actually changed; avoid flash wear from loading the same slot
    if (st.active_slot != s) {
        active_slot_    = s;
        st.active_slot  = static_cast<uint16_t>(s);
        st.crc          = ComputeCrc(st);
        storage().Save();
    } else {
        active_slot_ = s;
    }
    return true;
}

bool PresetManager::SaveSlot(int slot,
                             ModModeId mode,
                             const float norm[NUM_PARAMS]) {
    const int s = NormalizeSlotIndex(slot);

    PresetStorageState& st = storage().GetSettings();
    if (!IsValidState(st)) {
        return false;
    }

    PresetSlot& ps = st.slots[s];
    ps.valid       = 1;
    ps.mode        = static_cast<uint8_t>(mode);
    for (int i = 0; i < NUM_PARAMS; ++i) {
        ps.norm[i] = Clamp01(norm[i]);
    }

    active_slot_    = s;
    st.active_slot  = static_cast<uint16_t>(s);
    st.crc          = ComputeCrc(st);
    storage().Save();
    return true;
}

bool PresetManager::IsValidState(const PresetStorageState& state) const {
    if (state.magic != kMagic || state.version != kVersion) {
        return false;
    }
    if (state.active_slot >= PRESET_SLOT_COUNT) {
        return false;
    }
    if (state.crc != ComputeCrc(state)) {
        return false;
    }
    return true;
}

int PresetManager::NormalizeSlotIndex(int slot) const {
    if (slot < 0) {
        return PRESET_SLOT_COUNT - 1;
    }
    if (slot >= PRESET_SLOT_COUNT) {
        return 0;
    }
    return slot;
}

PresetManager::StorageType& PresetManager::storage() {
    return *reinterpret_cast<StorageType*>(storage_buf_);
}

const PresetManager::StorageType& PresetManager::storage() const {
    return *reinterpret_cast<const StorageType*>(storage_buf_);
}

} // namespace pedal
