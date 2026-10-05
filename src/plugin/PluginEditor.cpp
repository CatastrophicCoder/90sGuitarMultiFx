#include "plugin/PluginEditor.h"

#include "plugin/PluginProcessor.h"

namespace fivea
{

PluginEditor::PluginEditor(PluginProcessor& pluginProcessor)
    : AudioProcessorEditor(pluginProcessor)
    , mainPanel(pluginProcessor.getParameterState())
{
    addAndMakeVisible(mainPanel);
    setSize(960, 260);
}

void PluginEditor::resized()
{
    mainPanel.setBounds(getLocalBounds());
}

} // namespace fivea
