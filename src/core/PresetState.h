#pragma once

#include "core/ProgramState.h"

#include <juce_audio_processors/juce_audio_processors.h>

// Versioned plugin state. The format is ours rather than AudioProcessorValueTreeState's copyState(),
// so it can carry a schema version and survive parameter-set changes. Layout:
//
//   <FiveAState schemaVersion="2" model="functional-placeholder">
//     <Parameters>                               the edit buffer: every host parameter
//       <Parameter id="inputTrim" value="0"/>     plain units (dB, 0/1, choice index), layout order
//       ...
//     </Parameters>
//     <Programs bank="2" program="1" mode="program">     the selected program; "manualEdit"
//       <Program slot="1-1" name="METAL 1">             bank 1 only: the rest is factory content
//         <Value id="driveEnabled" value="1"/>          on/off states, then the controls, in the
//         <Value id="driveMode" value="1"/>             documented steps (MODE 1–7, not an index)
//         ...
//
// Loading rules (plan §16): unknown elements and IDs are ignored, missing or unreadable values
// take their default, out-of-range values are clamped, and a document that is not ours or has no
// schema version is rejected without touching the current state. A program slot that is missing
// or unreadable keeps its factory copy.
//
// Schema history: 1 (Milestone 0) had only <Parameters>; it loads with the factory programs and
// 1-1 selected.

namespace fivea::state
{

inline constexpr int currentSchemaVersion = 2;
inline constexpr const char* rootTag = "FiveAState";
inline constexpr const char* modelIdentifier = "functional-placeholder";

struct LoadResult
{
    bool loaded = false;
    int schemaVersion = 0;
};

[[nodiscard]] std::unique_ptr<juce::XmlElement> toXml(const juce::AudioProcessor& processor,
                                                      const ProgramState& programs);

// Message thread only: sets parameters through the host-notifying path, and replaces the program
// state. Neither changes if the document is rejected.
LoadResult fromXml(const juce::XmlElement& xml, juce::AudioProcessor& processor, ProgramState& programs);

} // namespace fivea::state
