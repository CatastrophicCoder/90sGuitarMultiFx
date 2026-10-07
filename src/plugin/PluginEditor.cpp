#include "plugin/PluginEditor.h"

#include "plugin/PluginProcessor.h"
#include "ui/PanelLayout.h"

#include <cmath>

namespace fivea
{

namespace
{
constexpr double aspectRatio = ui::panel::designWidth / ui::panel::designHeight;

int heightFor(int width)
{
    return static_cast<int>(std::lround(width / aspectRatio));
}
} // namespace

PluginEditor::PluginEditor(PluginProcessor& pluginProcessor)
    : AudioProcessorEditor(pluginProcessor)
    , mainPanel(pluginProcessor)
{
    addAndMakeVisible(mainPanel);
    setResizable(true, true);
    setResizeLimits(860, heightFor(860), 2580, heightFor(2580));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(aspectRatio);
    setSize(defaultWidth, heightFor(defaultWidth));
}

void PluginEditor::resized()
{
    mainPanel.setBounds(getLocalBounds());
}

} // namespace fivea
