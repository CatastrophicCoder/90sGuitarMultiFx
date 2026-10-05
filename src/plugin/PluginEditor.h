#pragma once

#include "ui/MainPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fivea
{

class PluginProcessor;

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor& pluginProcessor);

    void resized() override;

private:
    ui::MainPanel mainPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fivea
