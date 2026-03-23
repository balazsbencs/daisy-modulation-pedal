#pragma once
#include "mod_mode.h"

namespace pedal {

/// Transparent passthrough for modes not yet implemented.
/// Outputs the input unmodified so the dry/wet mix produces predictable results.
class PassthroughMode : public ModMode {
public:
    explicit PassthroughMode(const char* name) : name_(name) {}
    void Init() override {}
    void Reset() override {}
    StereoFrame Process(StereoFrame input, const ParamSet& /*params*/) override {
        return input;
    }
    const char* Name() const override { return name_; }
private:
    const char* name_;
};

} // namespace pedal
