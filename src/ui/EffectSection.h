#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace fivea::ui
{

// One of the five major effect blocks. In Milestone 0 the section is shown disabled: its enable
// switch is wired to the real parameter (so state and automation already work) but cannot be
// clicked, because no effect exists behind it yet.
class EffectSection final : public juce::Component
{
public:
    EffectSection(juce::AudioProcessorValueTreeState& state, const char* enableParameterId, const juce::String& title);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title;
    juce::ToggleButton enableButton{"On"};
    juce::Label statusLabel;
    juce::AudioProcessorValueTreeState::ButtonAttachment enableAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectSection)
};

} // namespace fivea::ui
