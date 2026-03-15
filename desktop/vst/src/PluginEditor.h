#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

class ModulationPluginEditor : public juce::AudioProcessorEditor,
                               public juce::Timer {
public:
    explicit ModulationPluginEditor(ModulationPluginProcessor&);
    ~ModulationPluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attach;
    };

    ModulationPluginProcessor& processor_;

    std::array<Knob, 7> knobs_;
    juce::ComboBox mode_box_;
    juce::Label    mode_label_;
    std::unique_ptr<ComboAttachment> mode_attach_;

    juce::ToggleButton bypass_btn_{"Bypass"};
    std::unique_ptr<ButtonAttachment> bypass_attach_;

    juce::ToggleButton sync_btn_{"Tempo Sync"};
    std::unique_ptr<ButtonAttachment> sync_attach_;

    juce::ComboBox subdiv_box_;
    juce::Label    subdiv_label_;
    std::unique_ptr<ComboAttachment> subdiv_attach_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPluginEditor)
};
