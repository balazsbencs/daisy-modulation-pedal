#include "PluginEditor.h"
#include "params/param_map.h"
#include "params/param_id.h"
#include <cmath>

namespace {
// Modulation parameter IDs and display names (must match createParameterLayout order).
constexpr const char* kParamIds[7] = {
    "speed", "depth", "mix", "tone", "p1", "p2", "level"
};

constexpr const char* kParamNames[7] = {
    "Speed", "Depth", "Mix", "Tone", "P1", "P2", "Level"
};

constexpr const char* kModeNames[pedal::NUM_MODES] = {
    "Chorus", "Flanger", "Rotary", "Vibe", "Phaser",
    "Filter", "Formant", "VintTrem", "PattTrem", "AutoSwell",
    "Destroyer", "Quadrature"
};
} // namespace

ModulationPluginEditor::ModulationPluginEditor(ModulationPluginProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , processor_(p) {
    auto& apvts = processor_.parameters();

    for (int i = 0; i < (int)knobs_.size(); ++i) {
        auto& k = knobs_[i];
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        k.slider.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(k.slider);

        k.label.setText(kParamNames[i], juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(k.label);

        k.attach = std::make_unique<SliderAttachment>(apvts, kParamIds[i], k.slider);
    }

    // Speed knob (index 0): display mapped Hz value, or "Synced" when tempo sync is on.
    knobs_[0].slider.textFromValueFunction = [this](double v) -> juce::String {
        if (processor_.parameters().getRawParameterValue("tempo_sync")->load() >= 0.5f)
            return "Synced";
        const auto& range = pedal::get_param_range(processor_.getCurrentMode(), pedal::ParamId::Speed);
        const float hz = pedal::map_param(static_cast<float>(v), range);
        return juce::String(hz, 2) + " Hz";
    };
    knobs_[0].slider.valueFromTextFunction = [this](const juce::String& text) -> double {
        const float hz = text.getFloatValue();
        const auto& range = pedal::get_param_range(processor_.getCurrentMode(), pedal::ParamId::Speed);
        return static_cast<double>(pedal::unmap_param(hz, range));
    };
    knobs_[0].slider.updateText();

    // Level knob (index 6): display as dB.
    knobs_[6].slider.textFromValueFunction = [](double v) -> juce::String {
        // v in [0,1] maps to gain [0,2]; show in dB (unity at v=0.5 → gain=1.0)
        const float gain = static_cast<float>(v) * 2.0f;
        if (gain <= 0.0f) return "-inf dB";
        const float db = 20.0f * std::log10(gain);
        return juce::String(db, 1) + " dB";
    };
    knobs_[6].slider.valueFromTextFunction = [](const juce::String& text) -> double {
        const float db = text.getFloatValue();
        const float gain = std::pow(10.0f, db / 20.0f);
        return static_cast<double>(gain * 0.5f); // back to [0,1]
    };
    knobs_[6].slider.updateText();

    mode_label_.setText("Mode", juce::dontSendNotification);
    mode_label_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(mode_label_);

    for (int i = 0; i < pedal::NUM_MODES; ++i)
        mode_box_.addItem(kModeNames[i], i + 1);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<ComboAttachment>(apvts, "mode", mode_box_);

    addAndMakeVisible(bypass_btn_);
    bypass_attach_ = std::make_unique<ButtonAttachment>(apvts, "bypass", bypass_btn_);

    addAndMakeVisible(sync_btn_);
    sync_attach_ = std::make_unique<ButtonAttachment>(apvts, "tempo_sync", sync_btn_);

    subdiv_label_.setText("Subdiv", juce::dontSendNotification);
    subdiv_label_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subdiv_label_);

    const char* subdivNames[] = { "1/1", "1/2", "1/4.", "1/4", "1/8.", "1/8", "1/8T", "1/16" };
    for (int i = 0; i < 8; ++i)
        subdiv_box_.addItem(subdivNames[i], i + 1);
    addAndMakeVisible(subdiv_box_);
    subdiv_attach_ = std::make_unique<ComboAttachment>(apvts, "subdivision", subdiv_box_);

    startTimerHz(10);
    setSize(760, 340);
}

ModulationPluginEditor::~ModulationPluginEditor() {
    stopTimer();
}

void ModulationPluginEditor::timerCallback() {
    const bool sync_on = processor_.parameters().getRawParameterValue("tempo_sync")->load() >= 0.5f;
    knobs_[0].slider.setEnabled(!sync_on);
    subdiv_box_.setEnabled(sync_on);
    if (sync_on)
        knobs_[0].slider.updateText(); // refresh "Synced" display
}

void ModulationPluginEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour::fromRGB(22, 24, 28));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("Daisy Modulation", 16, 10, 260, 30, juce::Justification::left);

    g.setColour(juce::Colour::fromRGB(70, 76, 90));
    g.drawLine(16.0f, 46.0f, (float)getWidth() - 16.0f, 46.0f, 1.0f);
}

void ModulationPluginEditor::resized() {
    constexpr int pad = 16;
    constexpr int knobW = 100;
    constexpr int knobH = 122;

    // Top row: 4 knobs (Speed, Depth, Mix, Tone)
    for (int i = 0; i < 4; ++i) {
        const int x = pad + i * (knobW + 12);
        knobs_[(size_t)i].slider.setBounds(x, 64, knobW, knobH - 24);
        knobs_[(size_t)i].label.setBounds(x, 42, knobW, 20);
    }

    // Bottom row: 3 knobs (P1, P2, Level)
    for (int i = 0; i < 3; ++i) {
        const int x = pad + i * (knobW + 12);
        knobs_[(size_t)(i + 4)].slider.setBounds(x, 202, knobW, knobH - 24);
        knobs_[(size_t)(i + 4)].label.setBounds(x, 180, knobW, 20);
    }

    mode_label_.setBounds(500, 72, 70, 20);
    mode_box_.setBounds(570, 68, 160, 28);

    bypass_btn_.setBounds(570, 112, 120, 28);

    sync_btn_.setBounds(500, 156, 230, 28);

    subdiv_label_.setBounds(500, 200, 60, 20);
    subdiv_box_.setBounds(565, 196, 165, 28);
}
