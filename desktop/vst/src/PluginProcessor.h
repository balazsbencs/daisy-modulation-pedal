#pragma once

#include <JuceHeader.h>

#include "modes/mode_registry.h"
#include "config/delay_mode_id.h"
#include "config/constants.h"
#include "params/param_set.h"

class DelayPluginProcessor : public juce::AudioProcessor {
public:
    DelayPluginProcessor();
    ~DelayPluginProcessor() override = default;

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
    pedal::DelayModeId getCurrentMode() const { return current_mode_; }

private:
    // host_period_s: beat-derived time override in seconds, or -1 if none.
    pedal::ParamSet buildParamsFromState(float host_period_s) const;
    void ensureModeFromParam();

    juce::AudioProcessorValueTreeState apvts_;

    pedal::ModeRegistry mode_registry_;
    pedal::DelayModeId  current_mode_ = pedal::DelayModeId::Digital;
    pedal::DelayMode*   active_mode_  = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayPluginProcessor)
};
