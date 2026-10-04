#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace a5
{

[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace a5
