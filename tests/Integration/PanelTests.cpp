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

TEST_CASE("The slide switch picks the row knobs A–E edit")
{
    OpenEditor open;
    auto& p = open.panel();

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
    auto& p = open.panel();
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
    auto& p = open.panel();

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

TEST_CASE("Footswitches 1–5 switch the effects, and their LEDs follow")
{
    OpenEditor open;
    auto& p = open.panel();
    for (int index = 0; index < 5; ++index)
    {
        INFO("footswitch " << index + 1);
        p.getFootSwitch(index).setToggleState(true, juce::sendNotificationSync);
        CHECK(open.plain(ParameterIds::effectEnabled[index]) == 1.0f);
        p.updateIndicators();
        CHECK(p.getEffectLed(index).isLit());

        p.getFootSwitch(index).setToggleState(false, juce::sendNotificationSync);
        CHECK(open.plain(ParameterIds::effectEnabled[index]) == 0.0f);
        p.updateIndicators();
        CHECK_FALSE(p.getEffectLed(index).isLit());
    }
    CHECK_FALSE(p.getFootSwitch(5).isEnabled()); // mode select: Milestone 2
    CHECK_FALSE(p.getWriteKey().isEnabled());    // Program Write: Milestone 2
}

TEST_CASE("BYPASS switches the global bypass and makes the mode LEDs blink")
{
    OpenEditor open;
    auto& p = open.panel();
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

// Not a check: renders the panel to PNG files for looking at.
//   FIVEA_SNAPSHOT_DIR=<dir> build/tests/fivea_plugin_tests "[.snapshot]"
TEST_CASE("Render the panel", "[.snapshot]")
{
    OpenEditor open;
    const char* directory = std::getenv("FIVEA_SNAPSHOT_DIR");
    REQUIRE(directory != nullptr);

    auto& p = open.panel();
    open.processor.getParameterState().getParameter(ParameterIds::driveEnabled)->setValueNotifyingHost(1.0f);
    open.processor.getParameterState().getParameter(ParameterIds::timeEffectEnabled)->setValueNotifyingHost(1.0f);
    p.selectRow(2);
    turn(p.getParameterKnob(1), 14.0);
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
}
