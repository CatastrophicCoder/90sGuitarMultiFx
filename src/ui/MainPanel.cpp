#include "ui/MainPanel.h"

#include "core/ParameterIds.h"

namespace a5::ui
{

MainPanel::GainControl::GainControl(juce::AudioProcessorValueTreeState& state, const char* parameterId,
                                    const juce::String& name)
    : attachment(state, parameterId, slider)
{
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setTextValueSuffix(" dB");
    slider.setTitle(name);
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
}

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& state)
    : inputControl(state, ParameterIds::inputTrim, "Input")
    , outputControl(state, ParameterIds::outputLevel, "Output")
    , bypassAttachment(state, ParameterIds::globalBypass, bypassButton)
{
    titleLabel.setText("Nineties Multi-FX  (development build, no effects yet)", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions{20.0f, juce::Font::bold});
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(bypassButton);

    addGainControl(inputControl);
    addGainControl(outputControl);

    // Block names and order are from the documented serial chain (evidence register EV-001).
    const std::array<juce::String, 5> titles{"Compressor", "Distortion / Overdrive", "3-Band EQ", "Chorus / Flanger",
                                             "Reverb / Delay"};

    for (std::size_t block = 0; block < effectSections.size(); ++block)
    {
        effectSections[block] =
            std::make_unique<EffectSection>(state, ParameterIds::effectEnabled[block], titles[block]);
        addAndMakeVisible(*effectSections[block]);
    }
}

void MainPanel::addGainControl(GainControl& control)
{
    addAndMakeVisible(control.slider);
    addAndMakeVisible(control.label);
}

void MainPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour{0xff1d1f22});
}

void MainPanel::layOutGainControl(GainControl& control, juce::Rectangle<int> area)
{
    control.label.setBounds(area.removeFromTop(24));
    control.slider.setBounds(area.reduced(4));
}

void MainPanel::resized()
{
    auto area = getLocalBounds().reduced(12);

    auto header = area.removeFromTop(36);
    bypassButton.setBounds(header.removeFromRight(90));
    titleLabel.setBounds(header);
    area.removeFromTop(8);

    constexpr int gainWidth = 100;
    layOutGainControl(inputControl, area.removeFromLeft(gainWidth));
    layOutGainControl(outputControl, area.removeFromRight(gainWidth));

    const int sectionWidth = area.getWidth() / static_cast<int>(effectSections.size());
    for (auto& section : effectSections)
        section->setBounds(area.removeFromLeft(sectionWidth).reduced(4, 0));
}

} // namespace a5::ui
