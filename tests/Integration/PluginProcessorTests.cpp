#include "core/ParameterIds.h"
#include "core/PresetState.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <random>

using fivea::PluginProcessor;
namespace ParameterIds = fivea::ParameterIds;

namespace
{

float plainValue(PluginProcessor& processor, const char* id)
{
    auto* parameter = processor.getParameterState().getParameter(id);
    return parameter->convertFrom0to1(parameter->getValue());
}

void setPlainValue(PluginProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getParameterState().getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::MemoryBlock saveState(PluginProcessor& processor)
{
    juce::MemoryBlock block;
    processor.getStateInformation(block);
    return block;
}

void loadXml(PluginProcessor& processor, const juce::String& xmlText)
{
    const auto xml = juce::parseXML(xmlText);
    REQUIRE(xml != nullptr);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(*xml, block);
    processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
}

void fillWithNoise(juce::AudioBuffer<float>& buffer, int channels, unsigned int seed)
{
    std::mt19937 generator{seed};
    std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};

    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample(channel, sample, distribution(generator));
}

bool channelsEqual(const juce::AudioBuffer<float>& a, int channelA, const juce::AudioBuffer<float>& b, int channelB)
{
    for (int sample = 0; sample < a.getNumSamples(); ++sample)
        if (a.getSample(channelA, sample) != b.getSample(channelB, sample))
            return false;

    return true;
}

} // namespace

// APVTS starts a timer, which needs the message manager.
#define FIVEA_JUCE_TEST_SETUP const juce::ScopedJuceInitialiser_GUI juceInitialiser

TEST_CASE("Stereo pass-through through the AudioProcessor at default settings")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000.0, 256);
    processor.prepareToPlay(48000.0, 256);

    juce::AudioBuffer<float> buffer{2, 256};
    fillWithNoise(buffer, 2, 21);
    juce::AudioBuffer<float> input{buffer};
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    CHECK(channelsEqual(buffer, 0, input, 0));
    CHECK(channelsEqual(buffer, 1, input, 1));
}

TEST_CASE("Mono input feeds both outputs of a stereo layout")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(layout));
    processor.prepareToPlay(44100.0, 128);

    juce::AudioBuffer<float> buffer{2, 128};
    fillWithNoise(buffer, 1, 4);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        buffer.setSample(1, sample, 123.0f); // stand-in for the garbage hosts leave there
    juce::AudioBuffer<float> input{buffer};
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    CHECK(channelsEqual(buffer, 0, input, 0));
    CHECK(channelsEqual(buffer, 1, input, 0));
}

TEST_CASE("Default settings stay bit-transparent after a state reload")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor source;
    const auto block = saveState(source);

    PluginProcessor processor;
    processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    processor.setPlayConfigDetails(2, 2, 44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer{2, 512};
    fillWithNoise(buffer, 2, 9);
    juce::AudioBuffer<float> input{buffer};
    juce::MidiBuffer midi;

    processor.processBlock(buffer, midi);

    CHECK(channelsEqual(buffer, 0, input, 0));
    CHECK(channelsEqual(buffer, 1, input, 1));
}

TEST_CASE("Supported bus layouts")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;

    auto supports = [&](const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(in);
        layout.outputBuses.add(out);
        return processor.checkBusesLayoutSupported(layout);
    };

    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    CHECK(supports(mono, mono));
    CHECK(supports(mono, stereo));
    CHECK(supports(stereo, stereo));
    CHECK_FALSE(supports(stereo, mono));
    CHECK_FALSE(supports(stereo, juce::AudioChannelSet::create5point1()));
}

TEST_CASE("State records the schema version and model")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;

    const auto block = saveState(processor);
    const auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));

    REQUIRE(xml != nullptr);
    CHECK(xml->hasTagName(fivea::state::rootTag));
    CHECK(xml->getIntAttribute("schemaVersion") == fivea::state::currentSchemaVersion);
    CHECK(xml->getStringAttribute("model") == fivea::state::modelIdentifier);
}

TEST_CASE("State round trip restores every parameter and the schema version")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor source;
    setPlainValue(source, ParameterIds::inputTrim, -7.5f);
    setPlainValue(source, ParameterIds::outputLevel, 3.2f);
    setPlainValue(source, ParameterIds::globalBypass, 1.0f);
    setPlainValue(source, ParameterIds::driveEnabled, 1.0f);
    setPlainValue(source, ParameterIds::timeEffectEnabled, 1.0f);

    const auto block = saveState(source);

    PluginProcessor restored;
    restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    CHECK(restored.getLastLoadedSchemaVersion() == fivea::state::currentSchemaVersion);
    CHECK_THAT(plainValue(restored, ParameterIds::inputTrim), Catch::Matchers::WithinAbs(-7.5f, 1.0e-4f));
    CHECK_THAT(plainValue(restored, ParameterIds::outputLevel), Catch::Matchers::WithinAbs(3.2f, 1.0e-4f));
    CHECK(plainValue(restored, ParameterIds::globalBypass) == 1.0f);
    CHECK(plainValue(restored, ParameterIds::compressorEnabled) == 0.0f);
    CHECK(plainValue(restored, ParameterIds::driveEnabled) == 1.0f);
    CHECK(plainValue(restored, ParameterIds::equaliserEnabled) == 0.0f);
    CHECK(plainValue(restored, ParameterIds::modulationEnabled) == 0.0f);
    CHECK(plainValue(restored, ParameterIds::timeEffectEnabled) == 1.0f);

    // Saving again gives the same bytes: the format is deterministic.
    CHECK(saveState(restored) == block);
}

TEST_CASE("Loading ignores unknown fields, defaults missing ones and clamps out-of-range values")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    setPlainValue(processor, ParameterIds::outputLevel, 10.0f);
    setPlainValue(processor, ParameterIds::modulationEnabled, 1.0f);

    loadXml(processor, R"(
        <FiveAState schemaVersion="1" model="functional-placeholder" futureAttribute="x">
          <Parameters>
            <Parameter id="inputTrim" value="99"/>
            <Parameter id="driveEnabled" value="1"/>
            <Parameter id="equaliserEnabled" value="not a number"/>
            <Parameter id="someFutureParameter" value="5"/>
          </Parameters>
          <SomethingNew/>
        </FiveAState>)");

    CHECK(processor.getLastLoadedSchemaVersion() == 1);
    CHECK(plainValue(processor, ParameterIds::inputTrim) == 24.0f);
    CHECK(plainValue(processor, ParameterIds::driveEnabled) == 1.0f);
    CHECK(plainValue(processor, ParameterIds::equaliserEnabled) == 0.0f);
    CHECK_THAT(plainValue(processor, ParameterIds::outputLevel), Catch::Matchers::WithinAbs(0.0f, 1.0e-4f));
    CHECK(plainValue(processor, ParameterIds::modulationEnabled) == 0.0f);
}

TEST_CASE("A newer schema version still loads the parameters this build knows")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;

    loadXml(processor, R"(
        <FiveAState schemaVersion="7">
          <Parameters><Parameter id="outputLevel" value="-4"/></Parameters>
        </FiveAState>)");

    CHECK(processor.getLastLoadedSchemaVersion() == 7);
    CHECK_THAT(plainValue(processor, ParameterIds::outputLevel), Catch::Matchers::WithinAbs(-4.0f, 1.0e-4f));
}

TEST_CASE("Foreign, unversioned or corrupt state leaves the current settings untouched")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    setPlainValue(processor, ParameterIds::inputTrim, 5.0f);

    loadXml(
        processor,
        R"(<SomeOtherPlugin schemaVersion="1"><Parameters><Parameter id="inputTrim" value="-3"/></Parameters></SomeOtherPlugin>)");
    loadXml(processor, R"(<FiveAState><Parameters><Parameter id="inputTrim" value="-3"/></Parameters></FiveAState>)");
    loadXml(processor, R"(<FiveAState schemaVersion="1.5"/>)");
    loadXml(processor, R"(<FiveAState schemaVersion="0"/>)");
    loadXml(processor, R"(<FiveAState schemaVersion="1e0"/>)");
    loadXml(processor, R"(<FiveAState schemaVersion="-1"/>)");

    const char garbage[] = "definitely not plugin state";
    processor.setStateInformation(garbage, static_cast<int>(sizeof(garbage)));
    processor.setStateInformation(nullptr, 0);

    CHECK(processor.getLastLoadedSchemaVersion() == 0);
    CHECK_THAT(plainValue(processor, ParameterIds::inputTrim), Catch::Matchers::WithinAbs(5.0f, 1.0e-4f));
}

TEST_CASE("The global bypass parameter is exposed to hosts")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;

    CHECK(processor.getBypassParameter() == processor.getParameterState().getParameter(ParameterIds::globalBypass));
}

TEST_CASE("The editor opens and closes")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    auto* editor = processor.createEditorAndMakeActive();

    REQUIRE(editor != nullptr);
    CHECK(editor->getWidth() > 0);
    CHECK(processor.getActiveEditor() == editor);

    // As a host does: tell the processor before deleting the editor.
    processor.editorBeingDeleted(editor);
    delete editor;
    CHECK(processor.getActiveEditor() == nullptr);
}

// --- The documented controls (Milestone 1) -------------------------------------------------------

TEST_CASE("Every documented control is a host parameter with its documented range")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    auto& state = processor.getParameterState();

    struct Expected
    {
        const char* id;
        float minimum;
        float maximum;
    };
    using namespace ParameterIds;
    // SRC-001 pp. 7, 10–12. Choices are checked separately below.
    const Expected steps[] = {
        {compressorSens, 0, 15},  {compressorAttack, 0, 7},    {compressorLevel, 0, 15}, {driveDrive, 0, 15},
        {driveTone, 0, 15},       {driveLevel, 0, 15},         {equaliserBass, -7, 7},   {equaliserMidFrequency, 1, 8},
        {equaliserMid, -7, 7},    {equaliserTreble, -7, 7},    {equaliserTrim, 0, 15},   {modulationSpeed, 0, 15},
        {modulationDepth, 0, 15}, {modulationFeedback, 0, 15}, {modulationMix, 0, 15},   {timeEffectTime, 0, 4},
        {timeEffectFine, 0, 9},   {timeEffectFeedback, 0, 15}, {timeEffectMix, 0, 15},   {noiseReductionLevel, 0, 15},
        {master, 0, 15},
    };
    for (const auto& expected : steps)
    {
        INFO(expected.id);
        auto* parameter = dynamic_cast<juce::AudioParameterInt*>(state.getParameter(expected.id));
        REQUIRE(parameter != nullptr);
        CHECK(parameter->getRange().getStart() == static_cast<int>(expected.minimum));
        CHECK(parameter->getRange().getEnd() == static_cast<int>(expected.maximum));
    }

    auto choices = [&](const char* id)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(id));
        REQUIRE(parameter != nullptr);
        return parameter->choices;
    };
    CHECK(choices(driveMode) == juce::StringArray{"Distortion", "Overdrive"});
    CHECK(choices(modulationMode) == juce::StringArray{"Flanger 1", "Flanger 2", "Chorus 1", "Chorus 2", "Slapback"});
    CHECK(choices(timeEffectMode) == juce::StringArray{"Hall Reverb", "Ensemble Hall Reverb", "Room Reverb",
                                                       "Plate Reverb", "Live Stage Reverb", "Echoverb", "Delay"});
    CHECK(state.getParameter(ParameterIds::equaliserMidFrequency)
              ->getText(state.getParameter(ParameterIds::equaliserMidFrequency)->convertTo0to1(3.0f), 20) == "800 Hz");
    CHECK_FALSE(state.getParameter(ParameterIds::driveOversampling)->isAutomatable());
}

TEST_CASE("Host parameters reach the engine as the documented steps")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    using namespace ParameterIds;
    setPlainValue(processor, compressorSens, 11.0f);
    setPlainValue(processor, driveMode, 0.0f); // choice index 0 = MODE 1, Distortion
    setPlainValue(processor, driveDrive, 14.0f);
    setPlainValue(processor, equaliserBass, 6.0f);
    setPlainValue(processor, equaliserMidFrequency, 3.0f);
    setPlainValue(processor, modulationMode, 4.0f); // index 4 = MODE 5, Slapback
    setPlainValue(processor, timeEffectMode, 3.0f); // index 3 = MODE 4, Plate
    setPlainValue(processor, timeEffectMix, 5.0f);
    setPlainValue(processor, noiseReductionLevel, 7.0f);
    setPlainValue(processor, master, 9.0f);

    const auto snapshot = processor.readParameterSnapshot();
    CHECK(snapshot.compressor.sens == 11);
    CHECK(snapshot.drive.mode == 1);
    CHECK(snapshot.drive.drive == 14);
    CHECK(snapshot.equaliser.bass == 6);
    CHECK(snapshot.equaliser.midFrequency == 3);
    CHECK(snapshot.modulation.mode == 5);
    CHECK(snapshot.timeEffects.mode == 4);
    CHECK(snapshot.timeEffects.mix == 5);
    CHECK(snapshot.noiseReductionLevel == 7);
    CHECK(snapshot.master == 9);
}

TEST_CASE("State round trip restores every documented control")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor source;
    using namespace ParameterIds;
    // The documented "METAL 1" factory program's settings (SRC-001 p. 6), as an example of a full set.
    setPlainValue(source, driveEnabled, 1.0f);
    setPlainValue(source, driveMode, 0.0f);
    setPlainValue(source, driveDrive, 14.0f);
    setPlainValue(source, driveTone, 15.0f);
    setPlainValue(source, driveLevel, 11.0f);
    setPlainValue(source, equaliserEnabled, 1.0f);
    setPlainValue(source, equaliserBass, 6.0f);
    setPlainValue(source, equaliserMidFrequency, 3.0f);
    setPlainValue(source, equaliserMid, 5.0f);
    setPlainValue(source, equaliserTreble, 1.0f);
    setPlainValue(source, equaliserTrim, 12.0f);
    setPlainValue(source, timeEffectEnabled, 1.0f);
    setPlainValue(source, timeEffectMode, 3.0f);
    setPlainValue(source, timeEffectMix, 5.0f);
    setPlainValue(source, driveOversampling, 2.0f);

    const auto block = saveState(source);
    PluginProcessor restored;
    restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    const auto a = source.readParameterSnapshot();
    const auto b = restored.readParameterSnapshot();
    CHECK(b.effectEnabled == a.effectEnabled);
    CHECK(b.drive.mode == a.drive.mode);
    CHECK(b.drive.drive == a.drive.drive);
    CHECK(b.drive.tone == a.drive.tone);
    CHECK(b.drive.level == a.drive.level);
    CHECK(b.equaliser.bass == a.equaliser.bass);
    CHECK(b.equaliser.midFrequency == a.equaliser.midFrequency);
    CHECK(b.equaliser.mid == a.equaliser.mid);
    CHECK(b.equaliser.treble == a.equaliser.treble);
    CHECK(b.equaliser.trim == a.equaliser.trim);
    CHECK(b.timeEffects.mode == a.timeEffects.mode);
    CHECK(b.timeEffects.mix == a.timeEffects.mix);
    CHECK(plainValue(restored, driveOversampling) == 2.0f);
    CHECK(saveState(restored) == block);
}

TEST_CASE("Latency follows the oversampling setting, including a change while running")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000.0, 256);

    processor.prepareToPlay(48000.0, 256);
    CHECK(processor.getLatencySamples() == 0);

    setPlainValue(processor, ParameterIds::driveOversampling, 2.0f); // 4x
    processor.applyOversamplingSetting();
    CHECK(processor.getLatencySamples() == 76);

    setPlainValue(processor, ParameterIds::driveOversampling, 1.0f); // 2x
    processor.prepareToPlay(48000.0, 256);
    CHECK(processor.getLatencySamples() == 69);
}

TEST_CASE("Every effect on, through processBlock: finite, and a mono guitar comes out stereo")
{
    FIVEA_JUCE_TEST_SETUP;
    PluginProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(layout));

    for (const auto* id : ParameterIds::effectEnabled)
        setPlainValue(processor, id, 1.0f);
    setPlainValue(processor, ParameterIds::noiseReductionLevel, 3.0f);
    processor.prepareToPlay(44100.0, 512);

    juce::MidiBuffer midi;
    bool differs = false;
    for (int block = 0; block < 200; ++block)
    {
        juce::AudioBuffer<float> buffer{2, 512};
        fillWithNoise(buffer, 1, static_cast<unsigned int>(block));
        buffer.applyGain(0.25f);
        processor.processBlock(buffer, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int n = 0; n < 512; ++n)
                REQUIRE(std::isfinite(buffer.getSample(channel, n)));
        if (!channelsEqual(buffer, 0, buffer, 1))
            differs = true;
    }
    CHECK(differs);
}
