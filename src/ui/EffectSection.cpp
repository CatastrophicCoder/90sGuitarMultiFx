#include "ui/EffectSection.h"

namespace fivea::ui
{

EffectSection::EffectSection(juce::AudioProcessorValueTreeState& state, const char* enableParameterId,
                             const juce::String& sectionTitle)
    : title(sectionTitle)
    , enableAttachment(state, enableParameterId, enableButton)
{
    addAndMakeVisible(enableButton);
    enableButton.setEnabled(false);

    statusLabel.setText("Not implemented", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(statusLabel);

    setTitle(sectionTitle);
    setEnabled(false);
}

void EffectSection::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(juce::Colour{0xff2a2d31});
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour{0xff474b52});
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions{15.0f, juce::Font::bold});
    g.drawFittedText(title, getLocalBounds().reduced(8).removeFromTop(24), juce::Justification::centred, 2);
}

void EffectSection::resized()
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(32);
    enableButton.setBounds(area.removeFromTop(24).withSizeKeepingCentre(60, 24));
    statusLabel.setBounds(area.removeFromTop(24));
}

} // namespace fivea::ui
