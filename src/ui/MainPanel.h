#pragma once

#include "ui/PanelControls.h"
#include "ui/PanelLookAndFeel.h"
#include "ui/SevenSegmentDisplay.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>

namespace fivea::ui
{

// The replica front panel (docs/panel-specification.md): the original unit's layout, proportions,
// colour scheme and Manual/Edit-mode workflow, with this plugin's own name and artwork.
//
// Everything is laid out in the design space of PanelLayout.h and scaled to the window as one
// canvas, so proportions hold at any size.
//
// Manual/Edit mode (SRC-001 pp. 5–7): the slide switch picks the grid row knobs A–E edit; the
// display shows the value being changed, "--" on stand-by; footswitches 1–5 switch the effects,
// with their LEDs; BYPASS makes the mode LEDs blink. Program mode, banks and WRITE are Milestone 2:
// the controls are drawn, and inactive.
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    MainPanel(juce::AudioProcessorValueTreeState& state, std::atomic<int>& selectedRow,
              std::function<float()> takeInputPeak);
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
    void updateIndicators();

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

    juce::AudioProcessorValueTreeState& parameters;
    std::atomic<int>& selectedRowStore;
    std::function<float()> takePeak;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace fivea::ui
