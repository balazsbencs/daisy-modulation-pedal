// desktop/vst/src/submode_info.cpp
#include "submode_info.h"

namespace {
// P1 label arrays (only Pattern Trem P1 is discrete)
const char* const kPatternLabels[] = {
    "1","2","3","4","5","6","7","8",
    "9","10","11","12","13","14","15","16"
};

// P2 label arrays
const char* const kChorusP2[]      = { "dBucket", "Multi", "Vibrato", "Detune", "Digital" };
const char* const kFlangerP2[]     = { "Silver", "Grey", "Black+", "Black-", "Zero+", "Zero-" };
const char* const kPhaserP2[]      = { "2-stage", "4-stage", "6-stage", "8-stage", "12-stage", "16-stage", "Barber Pole" };
const char* const kFilterP2[]      = { "Sine", "Triangle", "Square", "Ramp Up", "Ramp Down", "S&H", "Env+", "Env-" };
const char* const kFormantP2[]     = { "AA", "EE", "EYE", "OH", "OOH" };
const char* const kVintTremP2[]    = { "Tube", "Harmonic", "Photoresistor" };
const char* const kPattTremP2[]    = { "Straight", "Triplet", "Dotted" };
const char* const kQuadratureP2[]  = { "AM", "FM", "FreqShift+", "FreqShift-" };
} // namespace

// kSubmodeInfo[mode][0] = P1, kSubmodeInfo[mode][1] = P2
const SubmodeInfo kSubmodeInfo[12][2] = {
    // 0: Chorus
    { {false, 0, nullptr},        {true,  5, kChorusP2}     },
    // 1: Flanger
    { {false, 0, nullptr},        {true,  6, kFlangerP2}    },
    // 2: Rotary
    { {false, 0, nullptr},        {false, 0, nullptr}        },
    // 3: Vibe
    { {false, 0, nullptr},        {false, 0, nullptr}        },
    // 4: Phaser
    { {false, 0, nullptr},        {true,  7, kPhaserP2}     },
    // 5: Filter
    { {false, 0, nullptr},        {true,  8, kFilterP2}     },
    // 6: Formant
    { {false, 0, nullptr},        {true,  5, kFormantP2}    },
    // 7: Vintage Trem
    { {false, 0, nullptr},        {true,  3, kVintTremP2}   },
    // 8: Pattern Trem
    { {true,  16, kPatternLabels},{true,  3, kPattTremP2}   },
    // 9: AutoSwell
    { {false, 0, nullptr},        {false, 0, nullptr}        },
    // 10: Destroyer
    { {false, 0, nullptr},        {false, 0, nullptr}        },
    // 11: Quadrature
    { {false, 0, nullptr},        {true,  4, kQuadratureP2} },
};
