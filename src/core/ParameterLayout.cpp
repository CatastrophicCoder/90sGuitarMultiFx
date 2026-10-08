#include "core/ParameterLayout.h"

#include "core/ParameterIds.h"
#include "core/ParameterSnapshot.h"
#include "core/StepMapping.h"

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

// A documented control: its own whole steps, as on the original unit (SRC-001 pp. 7, 10–12).
std::unique_ptr<juce::AudioParameterInt> makeStepParameter(const char* id, const juce::String& name, int minimum,
                                                           int maximum, int defaultValue,
                                                           juce::AudioParameterIntAttributes attributes = {})
{
    return std::make_unique<juce::AudioParameterInt>(juce::ParameterID{id, ParameterIds::firstVersionHint}, name,
                                                     minimum, maximum, defaultValue, attributes);
}

std::unique_ptr<juce::AudioParameterChoice> makeModeParameter(const char* id, const juce::String& name,
                                                              const juce::StringArray& modes, int defaultMode)
{
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, ParameterIds::firstVersionHint}, name,
                                                        modes, defaultMode - 1);
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

    using namespace ParameterIds;
    const dsp::Compressor::Settings compressor;
    const dsp::Drive::Settings drive;
    const dsp::ThreeBandEq::Settings eq;
    const dsp::Modulation::Settings modulation;
    const dsp::TimeEffects::Settings timeEffects;

    const auto midFrequencyText = juce::AudioParameterIntAttributes{}.withStringFromValueFunction(
        [](int step, int)
        {
            return juce::String(mapping::eqMidFrequencyHz(step), 0) + " Hz";
        });

    layout.add(
        makeStepParameter(compressorSens, "Compressor SENS", 0, 15, compressor.sens),
        makeStepParameter(compressorAttack, "Compressor ATTACK", 0, 7, compressor.attack),
        makeStepParameter(compressorLevel, "Compressor LEVEL", 0, 15, compressor.level),
        makeModeParameter(driveMode, "Dist/OD MODE", {"Distortion", "Overdrive"}, drive.mode),
        makeStepParameter(driveDrive, "Dist/OD DRIVE", 0, 15, drive.drive),
        makeStepParameter(driveTone, "Dist/OD TONE", 0, 15, drive.tone),
        makeStepParameter(driveLevel, "Dist/OD LEVEL", 0, 15, drive.level),
        makeStepParameter(equaliserBass, "EQ BASS", -7, 7, eq.bass),
        makeStepParameter(equaliserMidFrequency, "EQ MID FREQ", 1, 8, eq.midFrequency, midFrequencyText),
        makeStepParameter(equaliserMid, "EQ MID", -7, 7, eq.mid),
        makeStepParameter(equaliserTreble, "EQ TREBLE", -7, 7, eq.treble),
        makeStepParameter(equaliserTrim, "EQ TRIM", 0, 15, eq.trim),
        makeModeParameter(modulationMode, "Chorus/FL MODE",
                          {"Flanger 1", "Flanger 2", "Chorus 1", "Chorus 2", "Slapback"}, modulation.mode),
        makeStepParameter(modulationSpeed, "Chorus/FL SPEED", 0, 15, modulation.speed),
        makeStepParameter(modulationDepth, "Chorus/FL DEPTH", 0, 15, modulation.depth),
        makeStepParameter(modulationFeedback, "Chorus/FL F.BACK", 0, 15, modulation.feedback),
        makeStepParameter(modulationMix, "Chorus/FL MIX", 0, 15, modulation.mix),
        makeModeParameter(timeEffectMode, "Rev/Delay MODE",
                          {"Hall Reverb", "Ensemble Hall Reverb", "Room Reverb", "Plate Reverb", "Live Stage Reverb",
                           "Echoverb", "Delay"},
                          timeEffects.mode),
        makeStepParameter(timeEffectTime, "Rev/Delay TIME", 0, 4, timeEffects.time),
        makeStepParameter(timeEffectFine, "Rev/Delay FINE", 0, 9, timeEffects.fine),
        makeStepParameter(timeEffectFeedback, "Rev/Delay F.BACK", 0, 15, timeEffects.feedback),
        makeStepParameter(timeEffectMix, "Rev/Delay MIX", 0, 15, timeEffects.mix),
        makeStepParameter(noiseReductionLevel, "NR LEVEL", 0, 15, 0), makeStepParameter(master, "MASTER", 0, 15, 12),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{driveOversampling, ParameterIds::firstVersionHint}, "Drive Oversampling",
            juce::StringArray{"Off", "2x", "4x"}, 2, juce::AudioParameterChoiceAttributes{}.withAutomatable(false)),
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{processingRate, ParameterIds::firstVersionHint},
                                                     "Processing Rate", juce::StringArray{"Host rate", "44.1 kHz"}, 0,
                                                     juce::AudioParameterChoiceAttributes{}.withAutomatable(false)));

    return layout;
}

} // namespace fivea
