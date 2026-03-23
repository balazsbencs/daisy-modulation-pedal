#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "params/param_map.h"
#include "params/param_id.h"
#include "params/param_range.h"
#include <cmath>

namespace {
float getParam(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id) {
    if (auto* p = apvts.getRawParameterValue(id)) {
        return p->load();
    }
    return 0.0f;
}

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// LFO tap-division multipliers relative to one quarter-note beat period.
constexpr int kNumSubdivisions = 8;
constexpr float kSubdivisionFactors[kNumSubdivisions] = {
    4.0f,           // 1/1   whole note
    2.0f,           // 1/2   half note
    1.5f,           // 1/4.  dotted quarter
    1.0f,           // 1/4   quarter note
    0.75f,          // 1/8.  dotted eighth
    0.5f,           // 1/8   eighth note
    1.0f / 3.0f,    // 1/8T  eighth triplet
    0.25f,          // 1/16  sixteenth note
};
} // namespace

ModulationPluginProcessor::ModulationPluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts_(*this, nullptr, "PARAMS", createParameterLayout()) {
    mode_registry_.Init();
    active_mode_ = mode_registry_.get(current_mode_);
    active_mode_->Reset();

    // Initialise per-mode P1/P2 to sensible defaults.
    for (int m = 0; m < 12; ++m) {
        const auto& si1 = kSubmodeInfo[m][0];
        const auto& si2 = kSubmodeInfo[m][1];
        p1_per_mode_[m] = si1.is_discrete
            ? (1.0f / (2.0f * si1.num_choices))   // midpoint of choice 0
            : 0.5f;
        p2_per_mode_[m] = si2.is_discrete
            ? (1.0f / (2.0f * si2.num_choices))
            : 0.5f;
    }
}

void ModulationPluginProcessor::prepareToPlay(double, int) {
    ensureModeFromParam();
    if (active_mode_ != nullptr) {
        active_mode_->Reset();
    }
}

void ModulationPluginProcessor::releaseResources() {}

bool ModulationPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

pedal::ParamSet ModulationPluginProcessor::buildParamsFromState(float host_period_s) const {
    using namespace pedal;

    // Read normalized [0,1] values from the APVTS using modulation param names.
    const float n_speed = clamp01(getParam(apvts_, "speed"));
    const float n_depth = clamp01(getParam(apvts_, "depth"));
    const float n_mix   = clamp01(getParam(apvts_, "mix"));
    const float n_tone  = clamp01(getParam(apvts_, "tone"));
    const float n_p1    = clamp01(getParam(apvts_, "p1"));
    const float n_p2    = clamp01(getParam(apvts_, "p2"));
    const float n_level = clamp01(getParam(apvts_, "level"));

    ParamSet ps;
    // AutoSwell uses speed as attack time (seconds), not Hz — skip tempo sync.
    // All other modes: clamp to the mode-specific speed range max.
    if (host_period_s > 0.0f && current_mode_ != ModModeId::AutoSwell) {
        const float max_hz = get_param_range(current_mode_, ParamId::Speed).max;
        ps.speed = juce::jlimit(0.05f, max_hz, 1.0f / host_period_s);
    } else {
        ps.speed = map_param(n_speed, get_param_range(current_mode_, ParamId::Speed));
    }
    ps.depth = map_param(n_depth, get_param_range(current_mode_, ParamId::Depth));
    ps.mix   = map_param(n_mix,   get_param_range(current_mode_, ParamId::Mix));
    ps.tone  = map_param(n_tone,  get_param_range(current_mode_, ParamId::Tone));
    ps.p1    = map_param(n_p1,    get_param_range(current_mode_, ParamId::P1));
    ps.p2    = map_param(n_p2,    get_param_range(current_mode_, ParamId::P2));
    ps.level = map_param(n_level, get_param_range(current_mode_, ParamId::Level));
    return ps;
}

void ModulationPluginProcessor::ensureModeFromParam() {
    const int next_mode = juce::jlimit(0, pedal::NUM_MODES - 1,
        (int)std::lround(getParam(apvts_, "mode")));
    const auto next_id = static_cast<pedal::ModModeId>(next_mode);
    if (next_id == current_mode_) return;

    current_mode_ = next_id;
    active_mode_  = mode_registry_.get(current_mode_);
    if (active_mode_ != nullptr) active_mode_->Reset();
}

void ModulationPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    ensureModeFromParam();
    if (active_mode_ == nullptr) {
        buffer.clear();
        return;
    }

    const bool bypassed = getParam(apvts_, "bypass") >= 0.5f;

    // Compute tempo-synced LFO period when requested.
    float host_period_s = -1.0f;
    if (getParam(apvts_, "tempo_sync") >= 0.5f) {
        if (auto* ph = getPlayHead()) {
            if (auto pos = ph->getPosition()) {
                if (auto bpm = pos->getBpm()) {
                    const int idx = juce::jlimit(0, kNumSubdivisions - 1,
                        static_cast<int>(std::round(getParam(apvts_, "subdivision"))));
                    host_period_s = static_cast<float>((60.0 / *bpm) * kSubdivisionFactors[idx]);
                }
            }
        }
    }

    const auto ps = buildParamsFromState(host_period_s);
    active_mode_->Prepare(ps);

    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    if (bypassed) {
        // True bypass: pass left and right independently.
        return;
    }

    const float angle = ps.mix * 1.57079632679f;
    const float dry_g = std::cos(angle) * ps.level;
    const float wet_g = std::sin(angle) * ps.level;

    // Note: unlike the firmware, the VST has no mono detection. Stereo input
    // is passed directly; Tier 2 modes (Flanger, Vibe, etc.) fold to mono
    // internally via input.mono(), silently discarding the right channel.
    for (int i = 0; i < n; ++i) {
        const pedal::StereoFrame wet = active_mode_->Process(
            pedal::StereoFrame{left[i], right[i]}, ps);

        left[i]  = left[i]  * dry_g + wet.left  * wet_g;
        right[i] = right[i] * dry_g + wet.right * wet_g;
    }
}

juce::AudioProcessorEditor* ModulationPluginProcessor::createEditor() {
    return new ModulationPluginEditor(*this);
}

bool ModulationPluginProcessor::hasEditor() const { return true; }
const juce::String ModulationPluginProcessor::getName() const { return JucePlugin_Name; }
bool ModulationPluginProcessor::acceptsMidi() const { return false; }
bool ModulationPluginProcessor::producesMidi() const { return false; }
bool ModulationPluginProcessor::isMidiEffect() const { return false; }
double ModulationPluginProcessor::getTailLengthSeconds() const { return 0.5; }

int ModulationPluginProcessor::getNumPrograms() { return 1; }
int ModulationPluginProcessor::getCurrentProgram() { return 0; }
void ModulationPluginProcessor::setCurrentProgram(int) {}
const juce::String ModulationPluginProcessor::getProgramName(int) { return {}; }
void ModulationPluginProcessor::changeProgramName(int, const juce::String&) {}

void ModulationPluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Persist per-mode P1/P2 values as child elements.
    auto* perMode = xml->createNewChildElement("PerModeParams");
    for (int m = 0; m < 12; ++m) {
        auto* el = perMode->createNewChildElement("Mode");
        el->setAttribute("index", m);
        el->setAttribute("p1", (double)p1_per_mode_[m]);
        el->setAttribute("p2", (double)p2_per_mode_[m]);
    }

    copyXmlToBinary(*xml, destData);
}

void ModulationPluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr) return;
    if (!xmlState->hasTagName(apvts_.state.getType())) return;

    // Restore per-mode P1/P2 values (graceful fallback if absent — old state).
    if (auto* perMode = xmlState->getChildByName("PerModeParams")) {
        for (auto* el : perMode->getChildWithTagNameIterator("Mode")) {
            const int m = el->getIntAttribute("index", -1);
            if (m >= 0 && m < 12) {
                p1_per_mode_[m] = (float)el->getDoubleAttribute("p1", p1_per_mode_[m]);
                p2_per_mode_[m] = (float)el->getDoubleAttribute("p2", p2_per_mode_[m]);
            }
        }
    }

    apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
    ensureModeFromParam();
}

juce::AudioProcessorValueTreeState::ParameterLayout ModulationPluginProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addNorm = [&](const juce::String& id, const juce::String& name, float def) {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(0.0f, 1.0f), def));
    };

    // 7 modulation parameters
    addNorm("speed", "Speed",  0.3f);   // LFO rate (normalized)
    addNorm("depth", "Depth",  0.5f);   // modulation depth
    addNorm("mix",   "Mix",    0.5f);   // wet/dry
    addNorm("tone",  "Tone",   0.5f);   // brightness (0.5 = flat)
    addNorm("p1",    "P1",     0.0f);   // mode-specific parameter 1
    addNorm("p2",    "P2",     0.5f);   // mode-specific parameter 2
    addNorm("level", "Level",  0.5f);   // output level (0.5 norm = unity)

    // Mode selector — 12 modulation modes
    juce::StringArray modeNames;
    modeNames.add("Chorus");
    modeNames.add("Flanger");
    modeNames.add("Rotary");
    modeNames.add("Vibe");
    modeNames.add("Phaser");
    modeNames.add("Filter");
    modeNames.add("Formant");
    modeNames.add("VintTrem");
    modeNames.add("PattTrem");
    modeNames.add("AutoSwell");
    modeNames.add("Destroyer");
    modeNames.add("Quadrature");
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Mode", modeNames, (int)pedal::ModModeId::Chorus));

    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"tempo_sync", 1}, "Tempo Sync", false));

    juce::StringArray subdivNames;
    subdivNames.add("1/1");
    subdivNames.add("1/2");
    subdivNames.add("1/4.");
    subdivNames.add("1/4");
    subdivNames.add("1/8.");
    subdivNames.add("1/8");
    subdivNames.add("1/8T");
    subdivNames.add("1/16");
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"subdivision", 1}, "Subdivision", subdivNames, 3)); // default: 1/4

    return {p.begin(), p.end()};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ModulationPluginProcessor();
}
