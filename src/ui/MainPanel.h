#pragma once

#include "core/Program.h"
#include "core/ProgramMode.h"
#include "ui/PanelControls.h"
#include "ui/PanelLookAndFeel.h"
#include "ui/SevenSegmentDisplay.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>

namespace fivea
{
class PluginProcessor;
}

namespace fivea::ui
{

// The replica front panel (docs/panel-specification.md): the original unit's layout, proportions,
// colour scheme and Manual/Edit-mode workflow, with this plugin's own name and artwork.
//
// Everything is laid out in the design space of PanelLayout.h and scaled to the window as one
// canvas, so proportions hold at any size.
//
// Footswitch 6 toggles the two modes (SRC-001 pp. 4–9; docs/panel-specification.md):
//
//   Program mode      the slide switch shows a bank (entered when a footswitch picks one of its
//                     programs); footswitches 1–5 select programs and light for the selected one;
//                     the display shows the bank, with the dot when no bank is pending; knobs A–E
//                     do nothing.
//   Manual/Edit mode  the slide switch picks the grid row knobs A–E edit; the display shows the
//                     value being changed, "--" on stand-by, with the dot when the value (or, on
//                     stand-by, the effects' on/off states) equals the stored program's;
//                     footswitches 1–5 switch the effects, with their LEDs.
//
// WRITE, in either mode: the display flashes "1"; a footswitch picks the bank 1 slot; WRITE again
// stores the program and both mode LEDs light for about a second; footswitch 6 cancels. BYPASS
// makes the lit mode LED blink.
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel(PluginProcessor& processor);
    ~MainPanel() override;

    void resized() override;

    // For tests: the controls, and the timer's work done once.
    void selectRow(int row);
    [[nodiscard]] int getSelectedRow() const noexcept { return slide.getPosition(); }
    [[nodiscard]] Knob& getParameterKnob(int index) noexcept { return parameterKnobs[static_cast<std::size_t>(index)]; }
    [[nodiscard]] FootSwitch& getFootSwitch(int index) noexcept
    {
        return footSwitches[static_cast<std::size_t>(index)];
    }
    [[nodiscard]] RoundKey& getBypassKey() noexcept { return bypassKey; }
    [[nodiscard]] RoundKey& getWriteKey() noexcept { return writeKey; }
    [[nodiscard]] const SevenSegmentDisplay& getDisplay() const noexcept { return display; }
    [[nodiscard]] const Led& getEffectLed(int index) const noexcept
    {
        return effectLeds[static_cast<std::size_t>(index)];
    }
    [[nodiscard]] const Led& getPeakLed() const noexcept { return peakLed; }
    [[nodiscard]] const Led& getEditModeLed() const noexcept { return editModeLed; }
    [[nodiscard]] const Led& getProgramModeLed() const noexcept { return programModeLed; }
    [[nodiscard]] bool isWritePending() const noexcept { return writePending; }
    void pressFootSwitch(int index); // as a click does
    void pressWriteKey();
    void updateIndicators();

    // Program Write's confirmation: both mode LEDs light for about a second (SRC-001 p. 8).
    static constexpr int writeConfirmationTicks = 30;

    // The PEAK LED lights at and above this input peak (−3 dBFS). A placeholder: the original's
    // threshold is not documented, only that it should light "occasionally, but not constantly".
    static constexpr float peakThreshold = 0.708f;

private:
    // The design-space canvas: every control is its child, and it is scaled to fit.
    class Canvas final : public juce::Component
    {
    public:
        explicit Canvas(MainPanel& owner) : panel(owner) {}
        void paint(juce::Graphics& g) override { panel.paintArtwork(g); }

    private:
        MainPanel& panel;
    };

    void timerCallback() override { updateIndicators(); }
    void paintArtwork(juce::Graphics& g);
    void layOutControls();
    void bindRow(int row);
    void showKnobValue(int index);
    void slideMoved(int position);
    void footSwitchClicked(int index);
    void writeKeyClicked();
    void setMode(ProgramMode mode);
    void configureForMode(); // the footswitches' and knobs' roles for the mode and the write state
    void updateDisplay(bool blinkOn);

    PluginProcessor& processor;
    juce::AudioProcessorValueTreeState& parameters;
    std::atomic<int>& selectedRowStore;

    PanelLookAndFeel lookAndFeel;
    Canvas canvas{*this};

    Knob inputKnob{"Input level"};
    Knob outputKnob{"Output level"};
    Led peakLed;
    SlideSelector slide;
    std::array<Knob, 5> parameterKnobs{Knob{"Parameter A"}, Knob{"Parameter B"}, Knob{"Parameter C"},
                                       Knob{"Parameter D"}, Knob{"Parameter E"}};
    SevenSegmentDisplay display;
    RoundKey writeKey{"Write"};
    RoundKey bypassKey{"Bypass"};
    std::array<FootSwitch, 6> footSwitches{FootSwitch{"Compressor"},   FootSwitch{"Distortion/Overdrive"},
                                           FootSwitch{"3 Band EQ"},    FootSwitch{"Chorus/Flanger"},
                                           FootSwitch{"Reverb/Delay"}, FootSwitch{"Mode select"}};
    std::array<Led, 5> effectLeds{Led{true}, Led{true}, Led{true}, Led{true}, Led{true}};
    Led programModeLed{true};
    Led editModeLed{true};

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 5> knobAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, 5> footSwitchAttachments;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    bool rebinding = false;
    int peakHoldTicks = 0;
    int blinkTicks = 0;

    int knobShown = -1; // the knob whose value the display shows in Edit mode; −1 on stand-by
    bool writePending = false;
    int writeDestination = 0; // the bank 1 program picked during a write; 0 before one is picked
    int writeConfirmTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace fivea::ui
