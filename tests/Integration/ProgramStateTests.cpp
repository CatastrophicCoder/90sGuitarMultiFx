#include "core/FactoryPrograms.h"
#include "core/ParameterIds.h"
#include "core/PresetState.h"
#include "plugin/PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using fivea::PluginProcessor;
using fivea::Program;
using fivea::ProgramControl;
using fivea::ProgramLocation;
using fivea::ProgramMode;
using fivea::ProgramName;
namespace ParameterIds = fivea::ParameterIds;

namespace
{
struct JuceSetup
{
    juce::ScopedJuceInitialiser_GUI juce;
};

juce::MemoryBlock saveState(PluginProcessor& processor)
{
    juce::MemoryBlock block;
    processor.getStateInformation(block);
    return block;
}

void loadState(PluginProcessor& processor, const juce::MemoryBlock& block)
{
    processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
}

void loadXml(PluginProcessor& processor, const juce::String& xmlText)
{
    const auto xml = juce::parseXML(xmlText);
    REQUIRE(xml != nullptr);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(*xml, block);
    loadState(processor, block);
}

std::unique_ptr<juce::XmlElement> stateXml(PluginProcessor& processor)
{
    const auto block = saveState(processor);
    return juce::AudioProcessor::getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
}

const Program& factory(ProgramLocation location)
{
    static const auto slots = fivea::factoryPrograms();
    return slots[static_cast<std::size_t>(location.slotIndex())];
}

Program userProgram()
{
    Program program;
    program.name = ProgramName::from("My Lead");
    program.effectEnabled = {true, true, false, false, true};
    program.compressor = {.sens = 11, .attack = 2, .level = 13};
    program.drive = {.mode = 1, .drive = 15, .tone = 3, .level = 10};
    program.equaliser = {.bass = -7, .midFrequency = 8, .mid = 7, .treble = -3, .trim = 9};
    program.modulation = {.mode = 5, .speed = 15, .depth = 1, .feedback = 14, .mix = 2};
    program.timeEffects = {.mode = 7, .time = 4, .fine = 9, .feedback = 15, .mix = 15};
    program.noiseReductionLevel = 15;
    program.master = 0;
    return program;
}
} // namespace

TEST_CASE("A new instance has the factory programs, 1-1 selected, in Program mode")
{
    JuceSetup setup;
    PluginProcessor processor;
    const auto& programs = processor.getProgramState();
    CHECK(programs.bank.allSlots() == fivea::factoryPrograms());
    CHECK(programs.selection.selected() == ProgramLocation{1, 1});
    CHECK(programs.mode == ProgramMode::Program);
}

TEST_CASE("State saves bank 1, the selected program and the mode, and restores them")
{
    JuceSetup setup;
    PluginProcessor source;
    auto& programs = source.getProgramState();
    REQUIRE(programs.bank.write({1, 2}, userProgram()));
    REQUIRE(programs.bank.write({1, 5}, factory({4, 3})));
    programs.selection.select({5, 4});
    programs.mode = ProgramMode::ManualEdit;

    const auto block = saveState(source);

    PluginProcessor restored;
    loadState(restored, block);
    const auto& loaded = restored.getProgramState();
    CHECK(restored.getLastLoadedSchemaVersion() == fivea::state::currentSchemaVersion);
    CHECK(loaded.bank.at({1, 2}) == userProgram());
    CHECK(loaded.bank.at({1, 5}) == factory({4, 3}));
    CHECK(loaded.bank.at({1, 1}) == factory({1, 1}));
    CHECK(loaded.bank.allSlots() == programs.bank.allSlots());
    CHECK(loaded.selection.selected() == ProgramLocation{5, 4});
    CHECK_FALSE(loaded.selection.isBankPending());
    CHECK(loaded.mode == ProgramMode::ManualEdit);

    CHECK(saveState(restored) == block); // deterministic
}

TEST_CASE("Saved programs use stable parameter IDs and the documented step numbers")
{
    JuceSetup setup;
    PluginProcessor processor;
    REQUIRE(processor.getProgramState().bank.write({1, 3}, userProgram()));
    const auto xml = stateXml(processor);
    REQUIRE(xml != nullptr);

    const auto* programs = xml->getChildByName("Programs");
    REQUIRE(programs != nullptr);
    CHECK(programs->getNumChildElements() == fivea::programsPerBank); // bank 1 only

    const juce::XmlElement* slot = nullptr;
    for (const auto* child : programs->getChildIterator())
        if (child->getStringAttribute("slot") == "1-3")
            slot = child;
    REQUIRE(slot != nullptr);
    CHECK(slot->getStringAttribute("name") == "My Lead");

    auto valueOf = [slot](const char* id)
    {
        for (const auto* value : slot->getChildIterator())
            if (value->getStringAttribute("id") == id)
                return value->getStringAttribute("value");
        return juce::String{"missing"};
    };
    CHECK(valueOf(ParameterIds::timeEffectMode) == "7"); // MODE 7, not the choice index 6
    CHECK(valueOf(ParameterIds::modulationMode) == "5");
    CHECK(valueOf(ParameterIds::equaliserBass) == "-7");
    CHECK(valueOf(ParameterIds::compressorEnabled) == "1");
    CHECK(valueOf(ParameterIds::equaliserEnabled) == "0");
    CHECK(valueOf(ParameterIds::master) == "0");
}

TEST_CASE("A schema 1 state migrates: its parameters load, with the factory programs and 1-1")
{
    JuceSetup setup;
    PluginProcessor processor;
    auto& programs = processor.getProgramState();
    REQUIRE(programs.bank.write({1, 1}, userProgram()));
    programs.selection.select({3, 2});
    programs.mode = ProgramMode::ManualEdit;

    // As Milestone 0 and 1 saved it.
    loadXml(processor, R"(
        <FiveAState schemaVersion="1" model="functional-placeholder">
          <Parameters>
            <Parameter id="inputTrim" value="-3"/>
            <Parameter id="driveEnabled" value="1"/>
            <Parameter id="driveDrive" value="9"/>
          </Parameters>
        </FiveAState>)");

    CHECK(processor.getLastLoadedSchemaVersion() == 1);
    const auto snapshot = processor.readParameterSnapshot();
    CHECK_THAT(snapshot.inputTrimDb, Catch::Matchers::WithinAbs(-3.0f, 1.0e-4f)); // snapped later, in the engine
    CHECK(snapshot.isEnabled(fivea::EffectBlock::Drive));
    CHECK(snapshot.drive.drive == 9);
    CHECK(programs.bank.allSlots() == fivea::factoryPrograms());
    CHECK(programs.selection.selected() == ProgramLocation{1, 1});
    CHECK(programs.mode == ProgramMode::Program);

    // Saved again, it is a schema 2 document.
    CHECK(stateXml(processor)->getIntAttribute("schemaVersion") == 2);
}

TEST_CASE("Program values that are unknown, missing, unreadable or out of range load safely")
{
    JuceSetup setup;
    PluginProcessor processor;

    loadXml(processor, R"(
        <FiveAState schemaVersion="2">
          <Parameters/>
          <Programs bank="1" program="2" mode="manualEdit">
            <Program slot="1-2" name="Odd">
              <Value id="driveEnabled" value="1"/>
              <Value id="eqBass" value="-40"/>
              <Value id="revMode" value="9"/>
              <Value id="driveDrive" value="not a number"/>
              <Value id="modSpeed" value="3.5"/>
              <Value id="someFutureControl" value="4"/>
              <SomethingNew/>
            </Program>
          </Programs>
        </FiveAState>)");

    const auto& programs = processor.getProgramState();
    const auto& odd = programs.bank.at({1, 2});
    const Program defaults;
    CHECK(odd.name.view() == "Odd");
    CHECK(odd.effectEnabled == std::array<bool, 5>{false, true, false, false, false});
    CHECK(odd.equaliser.bass == -7);                          // clamped
    CHECK(odd.timeEffects.mode == 7);                         // clamped
    CHECK(odd.drive.drive == defaults.drive.drive);           // unreadable: default
    CHECK(odd.modulation.speed == defaults.modulation.speed); // not a whole step: default
    CHECK(odd.compressor == defaults.compressor);             // missing: default
    CHECK(programs.mode == ProgramMode::ManualEdit);

    // Slots not in the document keep their factory copies.
    CHECK(programs.bank.at({1, 1}) == factory({1, 1}));
    CHECK(programs.bank.at({1, 3}) == factory({1, 3}));
}

TEST_CASE("Unreadable slots and factory slots in a document are ignored")
{
    JuceSetup setup;
    PluginProcessor processor;

    loadXml(processor, R"(
        <FiveAState schemaVersion="2">
          <Programs bank="12" program="-4" mode="somethingElse">
            <Program slot="2-1" name="Not METAL"><Value id="driveDrive" value="1"/></Program>
            <Program slot="1-9" name="No such slot"/>
            <Program slot="one-two" name="Garbage"/>
            <Program slot="13" name="No dash"/>
            <Program name="No slot"/>
          </Programs>
        </FiveAState>)");

    const auto& programs = processor.getProgramState();
    CHECK(programs.bank.allSlots() == fivea::factoryPrograms());
    CHECK(programs.selection.selected() == ProgramLocation{6, 1}); // clamped
    CHECK(programs.mode == ProgramMode::Program);
}

TEST_CASE("A newer schema keeps the programs this build understands")
{
    JuceSetup setup;
    PluginProcessor processor;

    loadXml(processor, R"(
        <FiveAState schemaVersion="9">
          <Programs bank="1" program="4" mode="program" futureAttribute="x">
            <Program slot="1-4" name="From the future" colour="red">
              <Value id="masterVolumeCurve" value="2"/>
              <Value id="master" value="7"/>
            </Program>
            <FutureThing/>
          </Programs>
        </FiveAState>)");

    const auto& programs = processor.getProgramState();
    CHECK(processor.getLastLoadedSchemaVersion() == 9);
    CHECK(programs.bank.at({1, 4}).name.view() == "From the future");
    CHECK(programs.bank.at({1, 4}).master == 7);
    CHECK(programs.selection.selected() == ProgramLocation{1, 4});
}

TEST_CASE("A rejected document leaves the program state untouched")
{
    JuceSetup setup;
    PluginProcessor processor;
    auto& programs = processor.getProgramState();
    REQUIRE(programs.bank.write({1, 3}, userProgram()));
    programs.selection.select({4, 2});

    loadXml(processor, R"(<SomeOtherPlugin schemaVersion="2"><Programs bank="1" program="1"/></SomeOtherPlugin>)");
    loadXml(processor, R"(<FiveAState><Programs bank="1" program="1"/></FiveAState>)");
    const char garbage[] = "definitely not plugin state";
    processor.setStateInformation(garbage, static_cast<int>(sizeof(garbage)));

    CHECK(programs.bank.at({1, 3}) == userProgram());
    CHECK(programs.selection.selected() == ProgramLocation{4, 2});
}

TEST_CASE("Every program control maps to a host parameter of the same range")
{
    JuceSetup setup;
    PluginProcessor processor;
    for (int index = 0; index < fivea::numProgramControls; ++index)
    {
        const auto control = static_cast<ProgramControl>(index);
        INFO("control " << index << " " << fivea::parameterIdFor(control));
        auto* parameter = processor.getParameterState().getParameter(fivea::parameterIdFor(control));
        REQUIRE(parameter != nullptr);

        // Choices (the MODEs) hold an index from 0; the documented steps count from 1.
        const bool isChoice = dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr;
        const auto range = fivea::controlRange(control);
        CHECK(parameter->convertFrom0to1(0.0f) + (isChoice ? 1.0f : 0.0f) == static_cast<float>(range.minimum));
        CHECK(parameter->convertFrom0to1(1.0f) + (isChoice ? 1.0f : 0.0f) == static_cast<float>(range.maximum));
    }
}
