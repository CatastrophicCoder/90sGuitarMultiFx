#pragma once

#include "ui/MainPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fivea
{

class PluginProcessor;

// Hosts the replica panel. Resizable, at the panel's own proportions.
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor& pluginProcessor);

    void resized() override;

    [[nodiscard]] ui::MainPanel& getPanel() noexcept { return mainPanel; }

    static constexpr int defaultWidth = 1290;

private:
    ui::MainPanel mainPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace fivea
