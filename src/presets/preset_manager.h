#pragma once

#include "daisy_seed.h"
#include "util/PersistentStorage.h"
#include "../config/constants.h"
#include "../config/mod_mode_id.h"
#include <cstdint>

namespace pedal {

class PresetManager {
public:
    void Init(daisy::DaisySeed& hw);

    int  GetActiveSlot() const { return active_slot_; }
    void SetActiveSlot(int slot);

    bool LoadActive(ModModeId& out_mode, float out_norm[NUM_PARAMS]);
    bool LoadSlot(int slot, ModModeId& out_mode, float out_norm[NUM_PARAMS]);

    bool SaveSlot(int slot, ModModeId mode, const float norm[NUM_PARAMS]);

private:
    struct PresetSlot {
        uint8_t valid;
        uint8_t mode;
        float   norm[NUM_PARAMS];

        bool operator!=(const PresetSlot& other) const;
    };

    struct PresetStorageState {
        uint32_t magic;
        uint16_t version;
        uint16_t active_slot;
        uint32_t crc;
        PresetSlot slots[PRESET_SLOT_COUNT];

        bool operator!=(const PresetStorageState& other) const;
    };

    using StorageType = daisy::PersistentStorage<PresetStorageState>;

    static constexpr uint32_t kMagic   = 0x44504C59; // DPLY
    static constexpr uint16_t kVersion = 1;

    static uint32_t ComputeCrc(const PresetStorageState& state);
    static void FillDefaultSlot(PresetSlot& slot);

    bool IsValidState(const PresetStorageState& state) const;
    int  NormalizeSlotIndex(int slot) const;

    StorageType& storage();
    const StorageType& storage() const;

    alignas(StorageType) uint8_t storage_buf_[sizeof(StorageType)]{};
    bool storage_initialized_ = false;
    int  active_slot_         = 0;
};

} // namespace pedal
