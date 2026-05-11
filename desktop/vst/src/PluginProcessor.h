#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "modes/mode_registry.h"
#include "config/mod_mode_id.h"
#include "config/constants.h"
#include "params/param_set.h"
#include "submode_info.h"

class ModulationPluginProcessor : public juce::AudioProcessor {
public:
    ModulationPluginProcessor();
    ~ModulationPluginProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& parameters() { return apvts_; }

private:
    // host_period_s: beat-derived LFO period override in seconds, or -1 if none.
    pedal::ParamSet buildParamsFromState(float host_period_s) const;
    void ensureModeFromParam();

    juce::AudioProcessorValueTreeState apvts_;
    float sample_rate_ = pedal::SAMPLE_RATE;

    pedal::ModeRegistry mode_registry_;
    std::atomic<pedal::ModModeId> current_mode_ {pedal::ModModeId::Chorus};
    pedal::ModMode*     active_mode_  = nullptr;
    juce::SpinLock      modeLock_;

    // Per-mode memory for P1 and P2 normalized values (atomic for cross-thread access).
    std::atomic<float> p1_per_mode_[pedal::NUM_MODES];
    std::atomic<float> p2_per_mode_[pedal::NUM_MODES];

public:
    pedal::ModModeId getCurrentMode() const { return current_mode_.load(std::memory_order_acquire); }
    float getP1ForMode(int mode) const { return p1_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].load(std::memory_order_relaxed); }
    float getP2ForMode(int mode) const { return p2_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].load(std::memory_order_relaxed); }
    void  saveP1ForMode(int mode, float v) { p1_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].store(v, std::memory_order_relaxed); }
    void  saveP2ForMode(int mode, float v) { p2_per_mode_[juce::jlimit(0, pedal::NUM_MODES - 1, mode)].store(v, std::memory_order_relaxed); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPluginProcessor)
};
