#pragma once

#include "ui/EffectSection.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

namespace fivea::ui
{

// Functional layout in signal order: input, the five documented blocks, output. A placeholder
// until the panel described in docs/panel-specification.md is built (Milestone 1, step 8).
class MainPanel final : public juce::Component
{
public:
    explicit MainPanel(juce::AudioProcessorValueTreeState& state);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct GainControl
    {
        GainControl(juce::AudioProcessorValueTreeState& state, const char* parameterId, const juce::String& name);

        juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    void addGainControl(GainControl& control);
    static void layOutGainControl(GainControl& control, juce::Rectangle<int> area);

    juce::Label titleLabel;
    GainControl inputControl;
    GainControl outputControl;
    juce::ToggleButton bypassButton{"Bypass"};
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    std::array<std::unique_ptr<EffectSection>, 5> effectSections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace fivea::ui
