// desktop/vst/include/submode_info.h
#pragma once

struct SubmodeInfo {
    bool             is_discrete;
    int              num_choices;
    const char* const* labels;
};

// kSubmodeInfo[mode_index][0] = P1 descriptor
// kSubmodeInfo[mode_index][1] = P2 descriptor
// mode_index matches pedal::ModModeId integer values (0–11)
extern const SubmodeInfo kSubmodeInfo[12][2];
