#include "core/FactoryPrograms.h"
#include "core/ParameterIds.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/PanelLayout.h"
#include "ui/SevenSegmentDisplay.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>

using fivea::PluginEditor;
using fivea::PluginProcessor;
using fivea::ui::SevenSegmentDisplay;
namespace ParameterIds = fivea::ParameterIds;
namespace panel = fivea::ui::panel;

namespace
{
// Closes the editor the way a host does: the processor is told before the editor goes, or it keeps
// a dangling pointer to it.
struct EditorDeleter
{
    juce::AudioProcessor* processor;
    void operator()(juce::AudioProcessorEditor* editor) const
    {
        processor->editorBeingDeleted(editor);
        delete editor;
    }
};
using EditorPointer = std::unique_ptr<juce::AudioProcessorEditor, EditorDeleter>;

EditorPointer openEditor(PluginProcessor& processor)
{
    return EditorPointer{processor.createEditorAndMakeActive(), EditorDeleter{&processor}};
}

struct OpenEditor
{
    juce::ScopedJuceInitialiser_GUI juce;
    PluginProcessor processor;
    EditorPointer editor{openEditor(processor)};

    fivea::ui::MainPanel& panel() { return dynamic_cast<PluginEditor&>(*editor).getPanel(); }

    // A new instance starts in Program mode (EV-029); footswitch 6 switches to Manual/Edit.
    fivea::ui::MainPanel& editMode()
    {
        if (processor.getProgramState().mode != fivea::ProgramMode::ManualEdit)
            panel().pressFootSwitch(5);
        REQUIRE(processor.getProgramState().mode == fivea::ProgramMode::ManualEdit);
        return panel();
    }

    float plain(const char* id)
    {
        auto* parameter = processor.getParameterState().getParameter(id);
        return parameter->convertFrom0to1(parameter->getValue());
    }
};

void turn(juce::Slider& knob, double value)
{
    knob.setValue(value, juce::sendNotificationSync);
}
} // namespace

// --- Seven-segment display -----------------------------------------------------------------------

TEST_CASE("Seven-segment patterns for the digits and the minus sign")
{
    // a b c d e f g = bits 0–6.
    CHECK(SevenSegmentDisplay::segmentsFor('0') == 0b0111111);
    CHECK(SevenSegmentDisplay::segmentsFor('1') == 0b0000110);
    CHECK(SevenSegmentDisplay::segmentsFor('2') == 0b1011011);
    CHECK(SevenSegmentDisplay::segmentsFor('3') == 0b1001111);
    CHECK(SevenSegmentDisplay::segmentsFor('4') == 0b1100110);
    CHECK(SevenSegmentDisplay::segmentsFor('5') == 0b1101101);
    CHECK(SevenSegmentDisplay::segmentsFor('6') == 0b1111101);
    CHECK(SevenSegmentDisplay::segmentsFor('7') == 0b0000111);
    CHECK(SevenSegmentDisplay::segmentsFor('8') == 0b1111111);
    CHECK(SevenSegmentDisplay::segmentsFor('9') == 0b1101111);
    CHECK(SevenSegmentDisplay::segmentsFor('-') == 0b1000000);
    CHECK(SevenSegmentDisplay::segmentsFor(' ') == 0);
}

TEST_CASE("Values show as the original's display shows them")
{
    CHECK(SevenSegmentDisplay::textFor(0) == " 0");
    CHECK(SevenSegmentDisplay::textFor(7) == " 7");
    CHECK(SevenSegmentDisplay::textFor(10) == "10");
    CHECK(SevenSegmentDisplay::textFor(15) == "15");
    CHECK(SevenSegmentDisplay::textFor(-7) == "-7");
    CHECK(SevenSegmentDisplay::textFor(-1) == "-1");
}

// --- The panel -----------------------------------------------------------------------------------

TEST_CASE("The editor opens at the panel's proportions and keeps them when resized")
{
    OpenEditor open;
    const double aspect = panel::designWidth / panel::designHeight;
    CHECK(open.editor->getWidth() == PluginEditor::defaultWidth);
    CHECK(std::abs(open.editor->getWidth() / static_cast<double>(open.editor->getHeight()) - aspect) < 0.01);

    open.editor->setSize(2000, 500); // the constrainer restores the proportions
    if (auto* constrainer = open.editor->getConstrainer())
    {
        auto bounds = open.editor->getBounds();
        constrainer->checkBounds(bounds, open.editor->getBounds(), {0, 0, 10000, 10000}, false, false, false, false);
        CHECK(std::abs(bounds.getWidth() / static_cast<double>(bounds.getHeight()) - aspect) < 0.01);
    }
}

TEST_CASE("The slide switch picks the row knobs A to E edit")
{
    OpenEditor open;
    auto& p = open.editMode();

    p.selectRow(2); // DIST/OD: MODE, DRIVE, TONE, —, LEVEL
    turn(p.getParameterKnob(1), 13.0);
    CHECK(open.plain(ParameterIds::driveDrive) == 13.0f);
    turn(p.getParameterKnob(2), 4.0);
    CHECK(open.plain(ParameterIds::driveTone) == 4.0f);
    CHECK_FALSE(p.getParameterKnob(3).isEnabled()); // an empty cell

    p.selectRow(5); // REV/DELAY: MODE, TIME, FINE, F.BACK, MIX
    turn(p.getParameterKnob(2), 7.0);
    CHECK(open.plain(ParameterIds::timeEffectFine) == 7.0f);
    CHECK(open.plain(ParameterIds::driveTone) == 4.0f); // the previous row is untouched

    p.selectRow(6); // UTILITY: only D and E
    for (int index = 0; index < 3; ++index)
        CHECK_FALSE(p.getParameterKnob(index).isEnabled());
    turn(p.getParameterKnob(3), 9.0);
    CHECK(open.plain(ParameterIds::noiseReductionLevel) == 9.0f);
    turn(p.getParameterKnob(4), 11.0);
    CHECK(open.plain(ParameterIds::master) == 11.0f);
}

TEST_CASE("Every grid cell's knob reaches the parameter printed in it")
{
    OpenEditor open;
    auto& p = open.editMode();
    for (int row = 1; row <= 6; ++row)
    {
        p.selectRow(row);
        for (int column = 0; column < 5; ++column)
        {
            const auto& cell = panel::rows[static_cast<std::size_t>(row - 1)].cells[static_cast<std::size_t>(column)];
            INFO("row " << row << ", knob " << static_cast<char>('A' + column));
            CHECK(p.getParameterKnob(column).isEnabled() == (cell.parameterId != nullptr));
            if (cell.parameterId == nullptr)
                continue;

            auto& knob = p.getParameterKnob(column);
            turn(knob, knob.getMaximum());
            CHECK(open.plain(cell.parameterId) == static_cast<float>(knob.getMaximum()));
            turn(knob, knob.getMinimum());
            CHECK(open.plain(cell.parameterId) == static_cast<float>(knob.getMinimum()));
        }
    }
}

TEST_CASE("The display shows the value being turned, and stand-by on a row change")
{
    OpenEditor open;
    auto& p = open.editMode();

    p.selectRow(1);
    CHECK(p.getDisplay().getText() == "--");
    turn(p.getParameterKnob(1), 5.0); // ATTACK
    CHECK(p.getDisplay().getText() == " 5");

    p.selectRow(3);
    CHECK(p.getDisplay().getText() == "--");
    turn(p.getParameterKnob(0), -7.0); // BASS
    CHECK(p.getDisplay().getText() == "-7");

    p.selectRow(5);
    turn(p.getParameterKnob(0), 6.0); // MODE: choice index 6 = mode 7, Delay
    CHECK(p.getDisplay().getText() == " 7");
    turn(p.getParameterKnob(4), 15.0); // MIX
    CHECK(p.getDisplay().getText() == "15");
}

TEST_CASE("Footswitches 1 to 5 switch the effects, and their LEDs follow")
{
    OpenEditor open;
    auto& p = open.editMode();
    for (int index = 0; index < 5; ++index)
    {
        INFO("footswitch " << index + 1);
        const bool wasOn = open.plain(ParameterIds::effectEnabled[index]) == 1.0f;
        p.pressFootSwitch(index);
        CHECK(open.plain(ParameterIds::effectEnabled[index]) == (wasOn ? 0.0f : 1.0f));
        p.updateIndicators();
        CHECK(p.getEffectLed(index).isLit() == !wasOn);

        p.pressFootSwitch(index);
        CHECK(open.plain(ParameterIds::effectEnabled[index]) == (wasOn ? 1.0f : 0.0f));
        p.updateIndicators();
        CHECK(p.getEffectLed(index).isLit() == wasOn);
    }
}

TEST_CASE("BYPASS switches the global bypass and makes the mode LEDs blink")
{
    OpenEditor open;
    auto& p = open.editMode();
    p.getBypassKey().setToggleState(true, juce::sendNotificationSync);
    CHECK(open.plain(ParameterIds::globalBypass) == 1.0f);

    int litTicks = 0;
    for (int tick = 0; tick < 24; ++tick) // two blink periods
    {
        p.updateIndicators();
        litTicks += p.getEditModeLed().isLit() ? 1 : 0;
    }
    CHECK(litTicks > 0);
    CHECK(litTicks < 24);

    CHECK_FALSE(p.getProgramModeLed().isLit()); // only the lit mode's LED blinks

    p.getBypassKey().setToggleState(false, juce::sendNotificationSync);
    p.updateIndicators();
    CHECK(p.getEditModeLed().isLit());
}

TEST_CASE("The PEAK LED follows the input level")
{
    OpenEditor open;
    open.processor.setPlayConfigDetails(2, 2, 48000.0, 256);
    open.processor.prepareToPlay(48000.0, 256);
    auto& p = open.panel();
    juce::MidiBuffer midi;

    juce::AudioBuffer<float> quiet{2, 256};
    quiet.clear();
    quiet.setSample(0, 10, 0.25f);
    open.processor.processBlock(quiet, midi);
    p.updateIndicators();
    CHECK_FALSE(p.getPeakLed().isLit());

    juce::AudioBuffer<float> loud{2, 256};
    loud.clear();
    loud.setSample(1, 20, -0.9f);
    open.processor.processBlock(loud, midi);
    p.updateIndicators();
    CHECK(p.getPeakLed().isLit());

    for (int tick = 0; tick < 5; ++tick) // the hold runs out with no more peaks
        p.updateIndicators();
    CHECK_FALSE(p.getPeakLed().isLit());
}

TEST_CASE("The selected row survives closing and reopening the editor")
{
    juce::ScopedJuceInitialiser_GUI juce;
    PluginProcessor processor;
    {
        auto editor = openEditor(processor);
        dynamic_cast<PluginEditor&>(*editor).getPanel().selectRow(4);
    }
    auto editor = openEditor(processor);
    CHECK(dynamic_cast<PluginEditor&>(*editor).getPanel().getSelectedRow() == 4);
}

// --- Program mode and WRITE (Milestone 2) ---------------------------------------------------------

namespace
{
const fivea::Program& factory(fivea::ProgramLocation location)
{
    static const auto slots = fivea::factoryPrograms();
    return slots[static_cast<std::size_t>(location.slotIndex())];
}

bool onlyLedLit(fivea::ui::MainPanel& p, int lit) // lit: 1–5, or 0 for none
{
    for (int index = 0; index < 5; ++index)
        if (p.getEffectLed(index).isLit() != (index + 1 == lit))
            return false;
    return true;
}
} // namespace

TEST_CASE("A new panel is in Program mode, showing bank 1 with the dot")
{
    OpenEditor open;
    auto& p = open.panel();
    p.updateIndicators();
    CHECK(p.getProgramModeLed().isLit());
    CHECK_FALSE(p.getEditModeLed().isLit());
    CHECK(p.getDisplay().getText() == " 1");
    CHECK(p.getDisplay().isDotLit());
    CHECK(onlyLedLit(p, 1)); // 1-1 is playing
}

TEST_CASE("Footswitch 6 toggles Program and Manual/Edit mode")
{
    OpenEditor open;
    auto& p = open.panel();
    p.pressFootSwitch(5);
    p.updateIndicators();
    CHECK(open.processor.getProgramState().mode == fivea::ProgramMode::ManualEdit);
    CHECK(p.getEditModeLed().isLit());
    CHECK_FALSE(p.getProgramModeLed().isLit());
    CHECK(p.getDisplay().getText() == "--");

    p.pressFootSwitch(5);
    p.updateIndicators();
    CHECK(open.processor.getProgramState().mode == fivea::ProgramMode::Program);
    CHECK(p.getProgramModeLed().isLit());
}

TEST_CASE("In Program mode the slide switch shows a bank, entered when a footswitch picks a program")
{
    OpenEditor open;
    auto& p = open.panel();
    const auto& selection = open.processor.getProgramState().selection;

    p.selectRow(4);
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == " 4");
    CHECK_FALSE(p.getDisplay().isDotLit()); // bank 4 shown, not entered
    CHECK(selection.selected() == fivea::ProgramLocation{1, 1});
    CHECK(onlyLedLit(p, 0));

    p.pressFootSwitch(1);
    p.updateIndicators();
    CHECK(selection.selected() == fivea::ProgramLocation{4, 2});
    CHECK(fivea::sameSettings(open.processor.getEditedProgram(), factory({4, 2})));
    CHECK(p.getDisplay().isDotLit());
    CHECK(onlyLedLit(p, 2));
    CHECK(open.plain(ParameterIds::effectEnabled[1]) == 0.0f); // a footswitch selects, it does not switch an effect
}

TEST_CASE("Back in Program mode, the display shows the playing bank wherever the switch is")
{
    OpenEditor open;
    auto& p = open.panel();
    p.selectRow(3);
    p.pressFootSwitch(0); // 3-1
    p.pressFootSwitch(5); // Edit
    p.selectRow(5);       // REV/DELAY row
    p.pressFootSwitch(5); // Program
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == " 3");
    CHECK(p.getDisplay().isDotLit());
    CHECK(onlyLedLit(p, 1));

    // Also when a bank was pending on leaving Program mode (SRC-001 p. 7, note 2).
    p.selectRow(6);       // bank 6 shown, 3-1 still playing
    p.pressFootSwitch(5); // Edit
    p.pressFootSwitch(5); // Program
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == " 3");
    CHECK(p.getDisplay().isDotLit());
}

TEST_CASE("Knobs A to E edit nothing in Program mode")
{
    OpenEditor open;
    auto& p = open.panel();
    p.selectRow(2);
    for (int index = 0; index < 5; ++index)
        CHECK_FALSE(p.getParameterKnob(index).isEnabled());

    p.pressFootSwitch(5);
    CHECK(p.getParameterKnob(1).isEnabled()); // row 2 (DIST/OD), DRIVE
}

TEST_CASE("In Edit mode the dot shows a value, or on stand-by the on/off states, equal to the stored program")
{
    OpenEditor open;
    auto& p = open.panel();
    p.selectRow(2);
    p.pressFootSwitch(0); // 2-1 METAL 1
    p.pressFootSwitch(5); // Edit, on row 2: DIST/OD
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == "--");
    CHECK(p.getDisplay().isDotLit());

    p.pressFootSwitch(0); // compressor on: METAL 1 has it off
    p.updateIndicators();
    CHECK_FALSE(p.getDisplay().isDotLit());
    p.pressFootSwitch(0);
    p.updateIndicators();
    CHECK(p.getDisplay().isDotLit());

    turn(p.getParameterKnob(1), 13.0); // DRIVE: stored 14
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == "13");
    CHECK_FALSE(p.getDisplay().isDotLit());
    turn(p.getParameterKnob(1), 14.0);
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == "14");
    CHECK(p.getDisplay().isDotLit());
}

TEST_CASE("WRITE stores the edited program in the bank 1 slot a footswitch picks")
{
    OpenEditor open;
    auto& p = open.panel();
    auto& programs = open.processor.getProgramState();
    p.selectRow(2);
    p.pressFootSwitch(0);             // 2-1
    p.pressFootSwitch(5);             // Edit
    turn(p.getParameterKnob(1), 9.0); // DRIVE 9

    p.pressWriteKey();
    CHECK(p.isWritePending());
    bool shownOne = false;
    bool shownBlank = false;
    for (int tick = 0; tick < 24; ++tick) // the display flashes "1"
    {
        p.updateIndicators();
        shownOne = shownOne || p.getDisplay().getText() == " 1";
        shownBlank = shownBlank || p.getDisplay().getText() == "  ";
    }
    CHECK(shownOne);
    CHECK(shownBlank);

    p.pressFootSwitch(2); // destination 1-3; not the EQ switch while writing
    p.updateIndicators();
    CHECK(onlyLedLit(p, 3));
    CHECK(open.plain(ParameterIds::effectEnabled[2]) == 1.0f); // METAL 1's EQ is still on

    p.pressWriteKey();
    CHECK_FALSE(p.isWritePending());
    CHECK(programs.bank.at({1, 3}).drive.drive == 9);
    CHECK(programs.bank.at({1, 3}).name.view() == "METAL 1");
    CHECK(programs.selection.selected() == fivea::ProgramLocation{1, 3});
    CHECK(programs.mode == fivea::ProgramMode::ManualEdit);

    // Both mode LEDs for about a second, then the mode's own again.
    int bothLit = 0;
    for (int tick = 0; tick < 2 * fivea::ui::MainPanel::writeConfirmationTicks; ++tick)
    {
        p.updateIndicators();
        bothLit += p.getProgramModeLed().isLit() && p.getEditModeLed().isLit() ? 1 : 0;
    }
    CHECK(bothLit == fivea::ui::MainPanel::writeConfirmationTicks);
    CHECK(p.getEditModeLed().isLit());
    CHECK_FALSE(p.getProgramModeLed().isLit());

    p.pressFootSwitch(0); // footswitches switch effects again
    CHECK(open.plain(ParameterIds::effectEnabled[0]) == 1.0f);
}

TEST_CASE("Footswitch 6 cancels a write, staying in the same mode")
{
    OpenEditor open;
    auto& p = open.editMode();
    const auto before = open.processor.getProgramState().bank.allSlots();
    p.pressWriteKey();
    p.pressFootSwitch(1);
    p.pressFootSwitch(5);
    CHECK_FALSE(p.isWritePending());
    CHECK(open.processor.getProgramState().mode == fivea::ProgramMode::ManualEdit);
    CHECK(open.processor.getProgramState().bank.allSlots() == before);
}

TEST_CASE("WRITE works from Program mode, and waits for a destination")
{
    OpenEditor open;
    auto& p = open.panel();
    p.selectRow(5);
    p.pressFootSwitch(2); // 5-3 DEV Plate

    p.pressWriteKey();
    p.pressWriteKey(); // no destination yet: still waiting
    CHECK(p.isWritePending());

    p.pressFootSwitch(0);
    p.pressWriteKey();
    CHECK_FALSE(p.isWritePending());
    const auto& programs = open.processor.getProgramState();
    CHECK(programs.bank.at({1, 1}) == factory({5, 3}));
    CHECK(programs.mode == fivea::ProgramMode::Program);
    CHECK(programs.selection.selected() == fivea::ProgramLocation{1, 1});
    p.updateIndicators();
    CHECK(p.getDisplay().getText() == " 1");
}

TEST_CASE("The mode survives closing and reopening the editor")
{
    juce::ScopedJuceInitialiser_GUI juce;
    PluginProcessor processor;
    {
        auto editor = openEditor(processor);
        dynamic_cast<PluginEditor&>(*editor).getPanel().pressFootSwitch(5);
    }
    auto editor = openEditor(processor);
    auto& p = dynamic_cast<PluginEditor&>(*editor).getPanel();
    p.updateIndicators();
    CHECK(processor.getProgramState().mode == fivea::ProgramMode::ManualEdit);
    CHECK(p.getEditModeLed().isLit());
    CHECK(p.getParameterKnob(0).isEnabled() ==
          (panel::rows[static_cast<std::size_t>(p.getSelectedRow() - 1)].cells[0].parameterId != nullptr));
}

// Not a check: renders the panel to PNG files for looking at.
//   FIVEA_SNAPSHOT_DIR=<dir> build/tests/fivea_plugin_tests "[.snapshot]"
TEST_CASE("Render the panel", "[.snapshot]")
{
    OpenEditor open;
    const char* directory = std::getenv("FIVEA_SNAPSHOT_DIR");
    REQUIRE(directory != nullptr);

    auto& p = open.editMode();
    open.processor.getParameterState().getParameter(ParameterIds::driveEnabled)->setValueNotifyingHost(1.0f);
    open.processor.getParameterState().getParameter(ParameterIds::timeEffectEnabled)->setValueNotifyingHost(1.0f);
    p.selectRow(2);
    turn(p.getParameterKnob(1), 13.0); // 1-1 already has DRIVE 14: a change shows the value
    p.updateIndicators();

    for (const int width : {PluginEditor::defaultWidth, 1905})
    {
        open.editor->setSize(width, static_cast<int>(std::lround(width * panel::designHeight / panel::designWidth)));
        const auto image = open.editor->createComponentSnapshot(open.editor->getLocalBounds(), true, 1.0f);
        juce::File file{juce::String(directory) + "/panel-" + juce::String(width) + ".png"};
        file.deleteFile();
        juce::FileOutputStream stream{file};
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, stream));
    }

    // Program mode, playing 2-1. The switch is already at 2 from editing, so it moves away and back
    // to show bank 2 (SRC-001 p. 7, note 2).
    p.pressFootSwitch(5);
    p.selectRow(3);
    p.selectRow(2);
    p.pressFootSwitch(0);
    p.updateIndicators();
    open.editor->setSize(
        PluginEditor::defaultWidth,
        static_cast<int>(std::lround(PluginEditor::defaultWidth * panel::designHeight / panel::designWidth)));
    const auto image = open.editor->createComponentSnapshot(open.editor->getLocalBounds(), true, 1.0f);
    juce::File file{juce::String(directory) + "/panel-program-" + juce::String(PluginEditor::defaultWidth) + ".png"};
    file.deleteFile();
    juce::FileOutputStream stream{file};
    REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, stream));
}
