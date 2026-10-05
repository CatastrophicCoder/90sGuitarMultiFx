#include "core/ParameterLayout.h"

#include "core/ParameterIds.h"

namespace fivea
{

namespace
{

std::unique_ptr<juce::AudioParameterFloat> makeGainParameter(const char* id, const juce::String& name)
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, ParameterIds::firstVersionHint}, name,
        juce::NormalisableRange<float>{ParameterRanges::minimumGainDb, ParameterRanges::maximumGainDb,
                                       1.0f / ParameterRanges::gainStepsPerDb},
        0.0f, juce::AudioParameterFloatAttributes{}.withLabel("dB"));
}

std::unique_ptr<juce::AudioParameterBool> makeEnableParameter(const char* id, const juce::String& name)
{
    // Off by default: Milestone 0 has no effect blocks, and a default of "on" would suggest one.
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, ParameterIds::firstVersionHint}, name,
                                                      false);
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(makeGainParameter(ParameterIds::inputTrim, "Input Trim"),
               makeGainParameter(ParameterIds::outputLevel, "Output Level"),
               std::make_unique<juce::AudioParameterBool>(
                   juce::ParameterID{ParameterIds::globalBypass, ParameterIds::firstVersionHint}, "Bypass", false),
               makeEnableParameter(ParameterIds::compressorEnabled, "Compressor On"),
               makeEnableParameter(ParameterIds::driveEnabled, "Drive On"),
               makeEnableParameter(ParameterIds::equaliserEnabled, "EQ On"),
               makeEnableParameter(ParameterIds::modulationEnabled, "Chorus/Flanger On"),
               makeEnableParameter(ParameterIds::timeEffectEnabled, "Reverb/Delay On"));

    return layout;
}

} // namespace fivea
