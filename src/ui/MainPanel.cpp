#include "ui/MainPanel.h"

#include "core/ParameterIds.h"
#include "plugin/PluginProcessor.h"
#include "ui/PanelLayout.h"

#include <cmath>
#include <optional>
#include <string_view>

namespace fivea::ui
{

namespace
{
using namespace panel;

constexpr int timerHz = 30;
constexpr int peakHoldTicksMax = 3;     // about 100 ms
constexpr int blinkHalfPeriodTicks = 6; // about 2.5 Hz

juce::Rectangle<int> around(Point centre, float halfWidth, float halfHeight)
{
    return juce::Rectangle<float>{centre.x - halfWidth, centre.y - halfHeight, 2.0f * halfWidth, 2.0f * halfHeight}
        .toNearestInt();
}

float rowCentre(std::size_t row)
{
    return gridTop + (static_cast<float>(row) + 0.5f) * gridRowHeight;
}

// The program control a grid cell's parameter belongs to, for the display's dot.
std::optional<ProgramControl> controlFor(const char* parameterId)
{
    for (int index = 0; index < numProgramControls; ++index)
        if (std::string_view{parameterIdFor(static_cast<ProgramControl>(index))} == parameterId)
            return static_cast<ProgramControl>(index);
    return std::nullopt;
}

void drawDownTriangle(juce::Graphics& g, float centreX, float top, float size)
{
    juce::Path triangle;
    triangle.addTriangle(centreX - size / 2.0f, top, centreX + size / 2.0f, top, centreX, top + size * 0.9f);
    g.fillPath(triangle);
}
} // namespace

MainPanel::MainPanel(PluginProcessor& pluginProcessor)
    : processor(pluginProcessor)
    , parameters(pluginProcessor.getParameterState())
    , selectedRowStore(pluginProcessor.selectedRow)
{
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(canvas);
    canvas.setBounds(0, 0, static_cast<int>(designWidth), static_cast<int>(designHeight));

    canvas.addAndMakeVisible(inputKnob);
    canvas.addAndMakeVisible(outputKnob);
    inputAttachment = std::make_unique<SliderAttachment>(parameters, ParameterIds::inputTrim, inputKnob);
    outputAttachment = std::make_unique<SliderAttachment>(parameters, ParameterIds::outputLevel, outputKnob);

    canvas.addAndMakeVisible(peakLed);
    canvas.addAndMakeVisible(slide);
    slide.onChange = [this](int position)
    {
        slideMoved(position);
    };

    for (std::size_t index = 0; index < parameterKnobs.size(); ++index)
    {
        canvas.addAndMakeVisible(parameterKnobs[index]);
        parameterKnobs[index].onValueChange = [this, index]
        {
            showKnobValue(static_cast<int>(index));
        };
    }

    canvas.addAndMakeVisible(display);

    canvas.addAndMakeVisible(writeKey);
    writeKey.onClick = [this]
    {
        writeKeyClicked();
    };
    canvas.addAndMakeVisible(bypassKey);
    bypassKey.setClickingTogglesState(true);
    bypassAttachment = std::make_unique<ButtonAttachment>(parameters, ParameterIds::globalBypass, bypassKey);

    for (std::size_t index = 0; index < footSwitches.size(); ++index)
    {
        canvas.addAndMakeVisible(footSwitches[index]);
        footSwitches[index].onClick = [this, index]
        {
            footSwitchClicked(static_cast<int>(index));
        };
    }

    for (auto& led : effectLeds)
        canvas.addAndMakeVisible(led);
    canvas.addAndMakeVisible(programModeLed);
    canvas.addAndMakeVisible(editModeLed);

    layOutControls();

    slide.setPosition(juce::jlimit(1, 6, selectedRowStore.load()), juce::dontSendNotification);
    configureForMode();
    updateIndicators();
    startTimerHz(timerHz);
}

MainPanel::~MainPanel()
{
    stopTimer();
    for (auto& attachment : knobAttachments)
        attachment.reset();
    setLookAndFeel(nullptr);
}

void MainPanel::layOutControls()
{
    // Positions, all in design units (PanelLayout.h).
    inputKnob.setBounds(around(panel::inputKnob, smallKnobRadius, smallKnobRadius));
    outputKnob.setBounds(around(panel::outputKnob, smallKnobRadius, smallKnobRadius));
    peakLed.setBounds(around(panel::peakLed, 8.0f, 8.0f));

    slide.setBounds(
        juce::Rectangle<float>{slideLeft, slideTop, slideRight - slideLeft, slideBottom - slideTop}.toNearestInt());
    std::array<float, 6> centres{};
    for (std::size_t row = 0; row < centres.size(); ++row)
        centres[row] = rowCentre(row) - slideTop; // the lever lines up with the row it selects
    slide.setPositionCentres(centres);

    for (std::size_t index = 0; index < parameterKnobs.size(); ++index)
        parameterKnobs[index].setBounds(
            around({parameterKnobX[index], parameterKnobY}, parameterKnobRadius, parameterKnobRadius));

    display.setBounds(juce::Rectangle<float>{digitsLeft, digitsTop, digitsRight - digitsLeft, digitsBottom - digitsTop}
                          .toNearestInt());
    writeKey.setBounds(around(panel::writeKey, keyRadius, keyRadius));
    bypassKey.setBounds(around(panel::bypassKey, keyRadius, keyRadius));

    for (std::size_t index = 0; index < footSwitches.size(); ++index)
        footSwitches[index].setBounds(juce::Rectangle<float>{footswitchX[index] - footswitchWidth / 2.0f, footswitchTop,
                                                             footswitchWidth, footswitchBottom - footswitchTop}
                                          .toNearestInt());
    for (std::size_t index = 0; index < effectLeds.size(); ++index)
        effectLeds[index].setBounds(around({footswitchX[index], footswitchLedY}, 11.0f, 6.0f));
    programModeLed.setBounds(around({footswitchX[5] - 25.0f, footswitchLedY}, 11.0f, 6.0f));
    editModeLed.setBounds(around({footswitchX[5] + 25.0f, footswitchLedY}, 11.0f, 6.0f));
}

void MainPanel::resized()
{
    // Fit the design space to the window, keeping its proportions (the editor keeps the window at
    // the same aspect ratio, so this fills it).
    const float scale =
        std::min(static_cast<float>(getWidth()) / designWidth, static_cast<float>(getHeight()) / designHeight);
    canvas.setTransform(juce::AffineTransform::scale(scale));
}

void MainPanel::selectRow(int row)
{
    slide.setPosition(row, juce::sendNotificationSync);
}

void MainPanel::pressFootSwitch(int index)
{
    auto& button = footSwitches[static_cast<std::size_t>(index)];
    if (button.getClickingTogglesState())
        button.setToggleState(!button.getToggleState(), juce::sendNotificationSync);
    else if (button.onClick)
        button.onClick();
}

void MainPanel::pressWriteKey()
{
    writeKeyClicked();
}

void MainPanel::slideMoved(int position)
{
    selectedRowStore.store(position);
    if (processor.getProgramState().mode == ProgramMode::Program)
        processor.getProgramState().selection.showBank(position); // shown, not yet entered
    else
        bindRow(position);
}

void MainPanel::footSwitchClicked(int index)
{
    auto& programs = processor.getProgramState();
    if (index == 5) // mode select: cancels a write, otherwise toggles the mode
    {
        if (writePending)
        {
            writePending = false;
            configureForMode();
        }
        else
            setMode(programs.mode == ProgramMode::Program ? ProgramMode::ManualEdit : ProgramMode::Program);
        return;
    }

    if (writePending)
        writeDestination = index + 1;
    else if (programs.mode == ProgramMode::Program)
        processor.selectProgram({programs.selection.shownBank(), index + 1});
    // In Edit mode the footswitch's attachment has already switched the effect.
}

void MainPanel::writeKeyClicked()
{
    if (!writePending)
    {
        writePending = true;
        writeDestination = 0;
        configureForMode();
        return;
    }
    if (writeDestination == 0)
        return; // no destination picked yet

    processor.writeProgram({userBank, writeDestination});
    writePending = false;
    writeConfirmTicks = writeConfirmationTicks;
    configureForMode();
}

void MainPanel::setMode(ProgramMode mode)
{
    auto& programs = processor.getProgramState();
    programs.mode = mode;
    if (mode == ProgramMode::Program)
        programs.selection.showBank(programs.selection.selected().bank); // whatever the switch says (p. 7)
    configureForMode();
}

void MainPanel::configureForMode()
{
    const bool editing = processor.getProgramState().mode == ProgramMode::ManualEdit;
    const bool switchesEffects = editing && !writePending;

    for (std::size_t index = 0; index < footSwitchAttachments.size(); ++index)
    {
        auto& button = footSwitches[index];
        if (switchesEffects && footSwitchAttachments[index] == nullptr)
        {
            button.setClickingTogglesState(true);
            footSwitchAttachments[index] =
                std::make_unique<ButtonAttachment>(parameters, ParameterIds::effectEnabled[index], button);
        }
        else if (!switchesEffects)
        {
            footSwitchAttachments[index].reset();
            button.setClickingTogglesState(false);
            button.setToggleState(false, juce::dontSendNotification);
        }
    }

    if (editing)
        bindRow(slide.getPosition());
    else
    {
        // Knobs A–E edit nothing in Program mode (EV-124).
        rebinding = true;
        for (std::size_t index = 0; index < parameterKnobs.size(); ++index)
        {
            knobAttachments[index].reset();
            parameterKnobs[index].setEnabled(false);
        }
        rebinding = false;
        knobShown = -1;
    }
    updateDisplay(true);
}

void MainPanel::bindRow(int row)
{
    selectedRowStore.store(row);
    const auto& cells = rows[static_cast<std::size_t>(row - 1)].cells;

    rebinding = true;
    for (std::size_t index = 0; index < parameterKnobs.size(); ++index)
    {
        knobAttachments[index].reset();
        auto& knob = parameterKnobs[index];
        if (const char* id = cells[index].parameterId)
        {
            knobAttachments[index] = std::make_unique<SliderAttachment>(parameters, id, knob);
            knob.setEnabled(true);
        }
        else
        {
            // An empty cell: on the original the knob turns but does nothing in this row.
            knob.setRange(0.0, 1.0, 1.0);
            knob.setValue(0.0, juce::dontSendNotification);
            knob.setEnabled(false);
        }
    }
    rebinding = false;

    knobShown = -1;
    display.showStandBy(); // "Edit stand-by" (SRC-001 p. 5)
}

void MainPanel::showKnobValue(int index)
{
    if (rebinding)
        return;

    const auto& cell = rows[static_cast<std::size_t>(slide.getPosition() - 1)].cells[static_cast<std::size_t>(index)];
    if (cell.parameterId == nullptr)
        return;

    // Shown as the original's display does: the control's own number. MODE choices count from 1.
    auto* parameter = parameters.getParameter(cell.parameterId);
    const int value = static_cast<int>(std::lround(parameter->convertFrom0to1(parameter->getValue())));
    display.showValue(dynamic_cast<juce::AudioParameterChoice*>(parameter) != nullptr ? value + 1 : value);
    knobShown = index;
}

void MainPanel::updateDisplay(bool blinkOn)
{
    const auto& programs = processor.getProgramState();
    if (writePending)
    {
        // Flashing "1": the program is about to be stored in bank 1 (SRC-001 p. 8).
        if (blinkOn)
            display.showValue(userBank);
        else
            display.showBlank();
        display.setDot(false);
        return;
    }

    if (programs.mode == ProgramMode::Program)
    {
        display.showValue(programs.selection.shownBank());
        display.setDot(!programs.selection.isBankPending());
        return;
    }

    // Edit mode: the value shown (set as the knob turns) or stand-by, and the dot (SRC-001 p. 9).
    const auto edited = processor.getEditedProgram();
    const auto& stored = processor.getStoredProgram();
    if (knobShown < 0)
    {
        display.showStandBy();
        display.setDot(sameEffectSwitches(edited, stored));
        return;
    }
    const auto& cell =
        rows[static_cast<std::size_t>(slide.getPosition() - 1)].cells[static_cast<std::size_t>(knobShown)];
    const auto control = cell.parameterId != nullptr ? controlFor(cell.parameterId) : std::nullopt;
    display.setDot(control.has_value() && sameControl(edited, stored, *control));
}

void MainPanel::updateIndicators()
{
    const auto& programs = processor.getProgramState();
    const bool editing = programs.mode == ProgramMode::ManualEdit;
    blinkTicks = (blinkTicks + 1) % (2 * blinkHalfPeriodTicks);
    const bool blinkOn = blinkTicks < blinkHalfPeriodTicks;

    // EFCT/PROG LEDs: the destination while writing; the selected program in Program mode (unlit
    // while another bank is pending, EV-124); the effects' on/off states in Edit mode.
    const auto selected = programs.selection.selected();
    for (std::size_t index = 0; index < effectLeds.size(); ++index)
    {
        const int number = static_cast<int>(index) + 1;
        if (writePending)
            effectLeds[index].setLit(number == writeDestination);
        else if (!editing)
            effectLeds[index].setLit(!programs.selection.isBankPending() && number == selected.program);
        else
            effectLeds[index].setLit(parameters.getRawParameterValue(ParameterIds::effectEnabled[index])->load() >=
                                     0.5f);
    }

    // The mode LEDs: the mode's own, blinking in bypass (SRC-001 p. 4); both for about a second
    // after a write (p. 8).
    const bool bypassed = parameters.getRawParameterValue(ParameterIds::globalBypass)->load() >= 0.5f;
    if (writeConfirmTicks > 0)
    {
        --writeConfirmTicks;
        programModeLed.setLit(true);
        editModeLed.setLit(true);
    }
    else
    {
        const bool modeLedOn = !bypassed || blinkOn;
        programModeLed.setLit(!editing && modeLedOn);
        editModeLed.setLit(editing && modeLedOn);
    }

    updateDisplay(blinkOn);

    if (processor.takeInputPeak() >= peakThreshold)
        peakHoldTicks = peakHoldTicksMax;
    else if (peakHoldTicks > 0)
        --peakHoldTicks;
    peakLed.setLit(peakHoldTicks > 0);
}

void MainPanel::paintArtwork(juce::Graphics& g)
{
    auto text = [&](const juce::String& s, float x, float y, float w, float h, float size, juce::Colour colour,
                    juce::Justification justification = juce::Justification::centredLeft)
    {
        g.setColour(colour);
        g.setFont(lookAndFeel.font(size, true));
        g.drawText(s, juce::Rectangle<float>{x, y, w, h}, justification, false);
    };
    const auto& ink = colours::printing;

    // Housing, the raised lip, the recessed face and the deck.
    g.fillAll(colours::housing);
    g.setGradientFill(
        juce::ColourGradient{colours::faceEdge, 0.0f, 0.0f, colours::housing, 0.0f, upperPanelTop, false});
    g.fillRect(0.0f, 0.0f, designWidth, upperPanelTop);
    g.setColour(colours::face);
    g.fillRect(25.0f, upperPanelTop, designWidth - 50.0f, upperPanelBottom - upperPanelTop);
    g.setColour(colours::faceEdge);
    g.drawRect(25.0f, upperPanelTop, designWidth - 50.0f, upperPanelBottom - upperPanelTop, 1.5f);
    g.setColour(colours::deck);
    g.fillRect(0.0f, deckTop, designWidth, designHeight - deckTop);
    g.setColour(colours::faceEdge);
    g.drawHorizontalLine(static_cast<int>(deckTop), 0.0f, designWidth);

    // Our wordmark where the original has its maker's logo.
    text("CATASTROPHIC AUDIO", wordmark.x, wordmark.y - 18.0f, 230.0f, 36.0f, 24.0f, ink);

    // Input section.
    text("PEAK", panel::peakLed.x - 30.0f, panel::peakLed.y - 30.0f, 60.0f, 16.0f, 13.0f, ink,
         juce::Justification::centred);
    text("INPUT", panel::inputKnob.x - 40.0f, panel::inputKnob.y - 52.0f, 80.0f, 16.0f, 13.0f, ink,
         juce::Justification::centred);
    text("MIN", panel::inputKnob.x - 58.0f, panel::inputKnob.y + 26.0f, 40.0f, 14.0f, 12.0f, ink);
    text("MAX", panel::inputKnob.x + 26.0f, panel::inputKnob.y + 26.0f, 40.0f, 14.0f, 12.0f, ink);

    // Slide switch labels.
    g.setColour(ink);
    g.drawRect(juce::Rectangle<float>{slideLeft + 18.0f, slideBottom + 12.0f, 46.0f, 17.0f}, 1.0f);
    text("BANK", slideLeft + 18.0f, slideBottom + 12.0f, 46.0f, 17.0f, 13.0f, ink, juce::Justification::centred);
    g.setColour(colours::red);
    g.fillRect(juce::Rectangle<float>{slideLeft + 66.0f, slideBottom + 12.0f, 42.0f, 17.0f});
    text("EDIT", slideLeft + 66.0f, slideBottom + 12.0f, 42.0f, 17.0f, 13.0f, ink, juce::Justification::centred);
    text("SELECT", slideLeft + 18.0f, slideBottom + 30.0f, 90.0f, 16.0f, 13.0f, ink, juce::Justification::centred);
    g.setColour(ink);
    g.drawRect(juce::Rectangle<float>{gridLeft + 2.0f, slideBottom + 10.0f, 16.0f, 17.0f}, 1.0f);
    text("1", gridLeft + 2.0f, slideBottom + 10.0f, 16.0f, 17.0f, 13.0f, ink, juce::Justification::centred);
    text("USER", gridLeft + 22.0f, slideBottom + 10.0f, 60.0f, 17.0f, 13.0f, ink);
    g.drawRect(juce::Rectangle<float>{gridLeft + 2.0f, slideBottom + 32.0f, 40.0f, 17.0f}, 1.0f);
    text("2~6", gridLeft + 2.0f, slideBottom + 32.0f, 40.0f, 17.0f, 13.0f, ink, juce::Justification::centred);
    text("PRESET", gridLeft + 46.0f, slideBottom + 32.0f, 70.0f, 17.0f, 13.0f, ink);

    // The parameter grid.
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        const float top = gridTop + static_cast<float>(row) * gridRowHeight;
        g.setColour(colours::gridLine);
        g.drawHorizontalLine(static_cast<int>(top + gridRowHeight), gridLeft, gridRight);

        g.setColour(ink);
        g.fillRect(gridLeft + 2.0f, top + 2.0f, 16.0f, gridRowHeight - 4.0f);
        text(juce::String(static_cast<int>(row) + 1), gridLeft + 2.0f, top, 16.0f, gridRowHeight, 15.0f, colours::face,
             juce::Justification::centred);

        g.setColour(colours::red);
        g.fillRect(rowNameLeft, top + 2.0f, rowNameRight - rowNameLeft, gridRowHeight - 4.0f);
        text(rows[row].name, rowNameLeft + 6.0f, top, rowNameRight - rowNameLeft - 6.0f, gridRowHeight, 15.0f, ink);

        for (std::size_t column = 0; column < 5; ++column)
        {
            const float left = columnEdges[column];
            const float right = columnEdges[column + 1];
            g.setColour(colours::gridLine);
            g.drawVerticalLine(static_cast<int>(right), top + 4.0f, top + gridRowHeight - 2.0f);

            const auto& cell = rows[row].cells[column];
            if (cell.label == nullptr)
                continue;
            text(cell.label, left + 5.0f, top, right - left - 10.0f, gridRowHeight, 15.0f, ink);
            text(juce::String::fromUTF8(cell.range), left + 5.0f, top, right - left - 10.0f, gridRowHeight, 15.0f, ink,
                 juce::Justification::centredRight);
        }
    }

    // Knobs A–E: dot scales and letters.
    for (std::size_t index = 0; index < parameterKnobX.size(); ++index)
    {
        const Point centre{parameterKnobX[index], parameterKnobY};
        g.setColour(ink);
        for (int dot = 0; dot <= 10; ++dot)
        {
            const float angle = juce::MathConstants<float>::pi * (1.2f + 1.6f * static_cast<float>(dot) / 10.0f);
            const float r = parameterKnobRadius + 8.0f;
            g.fillEllipse(centre.x + std::sin(angle) * r - 1.6f, centre.y - std::cos(angle) * r - 1.6f, 3.2f, 3.2f);
        }
        text(juce::String::charToString(static_cast<juce::juce_wchar>('A' + static_cast<int>(index))), centre.x + 22.0f,
             centre.y - 40.0f, 20.0f, 16.0f, 13.0f, ink);
    }

    // Display window: our product name where the original has its model name.
    g.setColour(colours::displayWindow);
    g.fillRoundedRectangle(
        juce::Rectangle<float>{displayLeft, displayTop, displayRight - displayLeft, displayBottom - displayTop}, 26.0f);
    text("BANK/VALUE", digitsLeft - 10.0f, displayTop - 22.0f, 130.0f, 18.0f, 13.0f, ink, juce::Justification::centred);
    text("Five-A", displayLeft + 40.0f, displayTop + 14.0f, 200.0f, 62.0f, 60.0f, ink, juce::Justification::centred);
    g.setColour(colours::red);
    g.fillRoundedRectangle(juce::Rectangle<float>{displayLeft + 52.0f, displayTop + 80.0f, 176.0f, 19.0f}, 9.5f);
    text("MULTIFX", displayLeft + 52.0f, displayTop + 80.0f, 176.0f, 19.0f, 14.0f, ink, juce::Justification::centred);
    text("M U L T I F X     P R O C E S S O R", displayLeft + 30.0f, displayBottom - 30.0f, 360.0f, 18.0f, 13.0f,
         colours::printingDim, juce::Justification::centred);

    // MODE LIST.
    text(juce::String::fromUTF8("MODE LIST ▶"), 1100.0f, modeListTop, 96.0f, 16.0f, 13.0f, ink);
    struct Column
    {
        float x;
        const char* heading;
        std::array<const char*, 4> items;
    };
    const std::array<Column, 4> columns{{
        {1200.0f, "DIST/OD", {"1 : DISTORTION", "2 : OVERDRIVE", nullptr, nullptr}},
        {1320.0f, "CHORUS/FLANGER", {"1,2 : FLANGER", "3,4 : CHORUS", "5 : SLAP BACK", nullptr}},
        {1455.0f, "REVERB/DELAY", {"1 : HALL", "2 : ENS HALL", "3 : ROOM", "4 : PLATE"}},
        {1570.0f, "", {"5 : LIVE STAGE", "6 : ECHOVERB", "7 : DELAY", nullptr}},
    }};
    for (const auto& column : columns)
    {
        if (column.heading[0] != '\0')
        {
            text(column.heading, column.x, modeListTop, 130.0f, 16.0f, 13.0f, ink);
            g.setColour(ink);
            g.drawHorizontalLine(static_cast<int>(modeListTop + 16.0f), column.x, column.x + 108.0f);
        }
        for (std::size_t item = 0; item < column.items.size(); ++item)
            if (column.items[item] != nullptr)
                text(column.items[item], column.x, modeListTop + 16.0f + 12.0f * static_cast<float>(item), 130.0f,
                     12.0f, 11.0f, ink);
    }

    // Output, Write, Bypass.
    text("OUTPUT", panel::outputKnob.x - 40.0f, panel::outputKnob.y - 52.0f, 80.0f, 16.0f, 13.0f, ink,
         juce::Justification::centred);
    text("MIN", panel::outputKnob.x - 58.0f, panel::outputKnob.y + 26.0f, 40.0f, 14.0f, 12.0f, ink);
    text("MAX", panel::outputKnob.x + 26.0f, panel::outputKnob.y + 26.0f, 40.0f, 14.0f, 12.0f, ink);
    text("WRITE", panel::writeKey.x - 40.0f, panel::writeKey.y - 46.0f, 80.0f, 16.0f, 13.0f, ink,
         juce::Justification::centred);
    text("BYPASS", panel::bypassKey.x - 40.0f, panel::bypassKey.y - 46.0f, 80.0f, 16.0f, 13.0f, ink,
         juce::Justification::centred);

    // The red stripe: effect names above footswitches 1–5, the mode switch's labels above the sixth.
    g.setColour(colours::red);
    g.fillRect(25.0f, stripeTop, designWidth - 50.0f, upperPanelBottom - stripeTop);
    for (std::size_t index = 0; index < stripeNames.size(); ++index)
    {
        const float x = footswitchX[index] - 10.0f;
        g.setColour(ink);
        drawDownTriangle(g, x, stripeTop + 7.0f, 22.0f);
        text(juce::String(static_cast<int>(index) + 1), x - 8.0f, stripeTop + 4.0f, 16.0f, 14.0f, 11.0f, colours::red,
             juce::Justification::centred);
        text(stripeNames[index], x + 16.0f, stripeTop, 320.0f, upperPanelBottom - stripeTop, 20.0f, ink);
    }
    text("PROG", footswitchX[5] - 92.0f, stripeTop, 60.0f, upperPanelBottom - stripeTop, 20.0f, ink,
         juce::Justification::centredRight);
    g.setColour(ink);
    drawDownTriangle(g, footswitchX[5] - 18.0f, stripeTop + 7.0f, 22.0f);
    drawDownTriangle(g, footswitchX[5] + 30.0f, stripeTop + 7.0f, 22.0f);
    text("MANUAL", footswitchX[5] + 46.0f, stripeTop + 2.0f, 80.0f, 16.0f, 12.0f, ink);
    text("/EDIT", footswitchX[5] + 46.0f, stripeTop + 16.0f, 80.0f, 16.0f, 12.0f, ink);
}

} // namespace fivea::ui
