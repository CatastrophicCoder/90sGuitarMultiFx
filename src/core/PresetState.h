#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Versioned plugin state. The format is ours rather than AudioProcessorValueTreeState's copyState(),
// so it can carry a schema version and survive parameter-set changes. Layout:
//
//   <FiveAState schemaVersion="1" model="functional-placeholder">
//     <Parameters>
//       <Parameter id="inputTrim" value="0"/>     plain units (dB, 0/1), in layout order
//       ...
//
// Loading rules (plan §16): unknown parameters are ignored, missing or unreadable ones take their
// default, out-of-range values are clamped, and a document that is not ours or has no schema
// version is rejected without touching the current state.

namespace a5::state
{

inline constexpr int currentSchemaVersion = 1;
inline constexpr const char* rootTag = "FiveAState";
inline constexpr const char* modelIdentifier = "functional-placeholder";

struct LoadResult
{
    bool loaded = false;
    int schemaVersion = 0;
};

[[nodiscard]] std::unique_ptr<juce::XmlElement> toXml(const juce::AudioProcessor& processor);

// Message thread only: sets parameters through the host-notifying path.
LoadResult fromXml(const juce::XmlElement& xml, juce::AudioProcessor& processor);

} // namespace a5::state
