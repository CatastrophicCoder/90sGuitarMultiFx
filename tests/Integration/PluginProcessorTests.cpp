#include "core/ParameterIds.h"
#include "core/PresetState.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <random>

using a5::PluginProcessor;
namespace ParameterIds = a5::ParameterIds;

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
#define A5_JUCE_TEST_SETUP const juce::ScopedJuceInitialiser_GUI juceInitialiser

TEST_CASE("Stereo pass-through through the AudioProcessor at default settings")
{
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
    PluginProcessor processor;

    const auto block = saveState(processor);
    const auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));

    REQUIRE(xml != nullptr);
    CHECK(xml->hasTagName(a5::state::rootTag));
    CHECK(xml->getIntAttribute("schemaVersion") == a5::state::currentSchemaVersion);
    CHECK(xml->getStringAttribute("model") == a5::state::modelIdentifier);
}

TEST_CASE("State round trip restores every parameter and the schema version")
{
    A5_JUCE_TEST_SETUP;
    PluginProcessor source;
    setPlainValue(source, ParameterIds::inputTrim, -7.5f);
    setPlainValue(source, ParameterIds::outputLevel, 3.2f);
    setPlainValue(source, ParameterIds::globalBypass, 1.0f);
    setPlainValue(source, ParameterIds::driveEnabled, 1.0f);
    setPlainValue(source, ParameterIds::timeEffectEnabled, 1.0f);

    const auto block = saveState(source);

    PluginProcessor restored;
    restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    CHECK(restored.getLastLoadedSchemaVersion() == a5::state::currentSchemaVersion);
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
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
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
    A5_JUCE_TEST_SETUP;
    PluginProcessor processor;

    CHECK(processor.getBypassParameter() == processor.getParameterState().getParameter(ParameterIds::globalBypass));
}

TEST_CASE("The editor opens and closes")
{
    A5_JUCE_TEST_SETUP;
    PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditorAndMakeActive()};

    REQUIRE(editor != nullptr);
    CHECK(editor->getWidth() > 0);
}
