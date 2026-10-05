#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fivea
{

[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace fivea
