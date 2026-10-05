# Architecture

State at **Milestone 0**: a buildable plugin (VST3, and AU on macOS) that passes audio through with
input trim, output level and a global bypass. No effect algorithm exists yet. Nothing in this
build reproduces, or claims to reproduce, the sound of the original unit.

## Layers

```
 Host (DAW)
   │  juce::AudioBuffer, parameters, state blobs
   ▼
 PluginProcessor            src/plugin      JUCE adapter: buses, parameters (APVTS), state, editor
   │  ParameterSnapshot (by value, once per block)
   │  AudioBufferView   (non-owning channel pointers)
   ▼
 FiveAProcessor                src/core        Engine façade. Pure C++20, no JUCE.
   input trim → [Compressor → Drive → EQ → Chorus/Flanger → Reverb/Delay] → output level
                 (Milestone 1; the documented order, EV-001)
```

- **`fivea_engine`** (static library): `FiveAProcessor`, `AudioBufferView`, `ParameterSnapshot`,
  `ParameterIds.h`, `src/dsp/*`. It has no JUCE dependency so it can be tested without a host and
  wrapped by another plugin format (CLAP) later without changing DSP code (plan §4).
- **`FiveAPlugin`** (JUCE shared-code target, plus VST3 and, on macOS, AU wrappers):
  `PluginProcessor`, `PluginEditor`, `ParameterLayout`, `PresetState`, `src/ui/*`.

`ParameterLayout.cpp` and `PresetState.cpp` live in `src/core` as the plan lays out, but depend on
JUCE and are compiled into the plugin target, not the engine.

The split between a host-facing controller and an audio engine mirrors, conceptually, the
original's controller CPU and DSP (EV-007, INFERRED). It is a software design choice and does not
model either chip.

## Threading and real-time rules

| Call | Thread | Allocation | Notes |
|------|--------|------------|-------|
| `FiveAProcessor::prepare`, `reset` | message / host prepare | allowed (none today) | Sets ramp lengths for the sample rate; `reset` settles every ramp on the current parameters. |
| `FiveAProcessor::setParameters` | audio | none | Copies a plain struct, sanitises values (clamp, snap to 0.1 dB, NaN → 0 dB). |
| `FiveAProcessor::process` | audio | none | No locks, I/O, logging or construction. |
| `PluginProcessor::processBlock` | audio | none | Reads parameters from APVTS atomics (relaxed loads) into a snapshot. |
| `get/setStateInformation` | message | yes | Never touches the engine directly; the audio thread sees new values through the atomics. |

Parameters reach the audio thread only through JUCE's per-parameter `std::atomic<float>`; there is
no shared mutable state between the editor and the engine.

## Signal handling at the boundary

- Supported layouts: mono → mono, mono → stereo, stereo → stereo.
- Mono → stereo: the input is copied to the right output before the engine runs. Where the
  original becomes stereo is unknown, so this is PLACEHOLDER (EV-005).
- Output channels without an input are cleared, never passed through as host garbage.
- Gains are computed once per sample and applied to every channel, keeping stereo matched.

## Parameters

IDs are centralised in `src/core/ParameterIds.h` and listed in `parameter-specification.md`.
Each `juce::ParameterID` carries version hint 1; parameters added later take the version of the
release that adds them.

## State format (schema version 1)

```xml
<FiveAState schemaVersion="1" model="functional-placeholder">
  <Parameters>
    <Parameter id="inputTrim" value="0"/>
    ...
  </Parameters>
</FiveAState>
```

Stored inside JUCE's binary XML wrapper (`copyXmlToBinary`). Values are in plain units, written in
parameter-layout order, so saving the same settings always yields the same bytes.

Loading (`fivea::state::fromXml`):

1. Root tag must match and `schemaVersion` must be an integer ≥ 1; otherwise the load is refused
   and current settings are kept.
2. Older versions pass through `migrateToCurrentSchema` (empty until version 2 exists).
3. Newer versions are read for the parameters this build knows.
4. Each known parameter: parsed strictly, clamped to range; if missing or unreadable, its default.
5. Unknown elements and attributes are ignored.

The plan's §16 shows the schema as JSON "conceptually". XML is used because JUCE hosts it
natively and it carries the same information; bank, program and preset name will be added as
attributes in Milestone 2 under a schema-version bump only if the change is not additive.

## Build and dependencies

- CMake ≥ 3.25, C++20, Ninja recommended.
- JUCE 9.0.2 and Catch2 v3.9.1 are git submodules in `external/`, pinned to their release tags
  (the same commits as the other Catastrophic Audio projects). CMake stops with a message if
  `external/JUCE` is empty.
- A Release build installs the AU and VST3 into the user plugin folders; Debug does not
  (`FIVEA_COPY_PLUGIN`). Both would install to the same place, and whichever built last would be what
  a DAW loads.
- Windows links the C++ runtime statically, so a host does not need `VCRUNTIME140.dll` beside the
  plugin to load it.
- Company: Catastrophic Audio (bundle ID `com.catastrophicaudio.fivea`, manufacturer
  code `Ctcd`, shared with its other plugins). Plugin code `Nmf1`. Product name
  "Five-A MultiFX Processor" (owner's decision 2026-10-05; it avoids the original's literal name
  and replaces its model name, plan §17.2). Changing the manufacturer or plugin code, or the
  bundle ID, after release breaks saved sessions.

## Differences from the plan's file layout (plan §5)

| Item | Reason |
|------|--------|
| Added `src/core/AudioBufferView.h`, `ParameterSnapshot.h`, `ParameterLayout.h` | Types and declarations the plan's interface sketch (§6.1) needs. |
| Added in M1 step 6: `src/dsp/Reverb.h/.cpp`, `src/dsp/TimeEffects.h/.cpp` | Plan §5, §12, §15. |
| Added in M1 step 5: `src/dsp/DelayLine.h/.cpp`, `src/dsp/Modulation.h/.cpp` | Plan §5, §11. `DelayLine` will also serve the delay and reverb. |
| Added in M1 step 4: `src/dsp/Drive.h/.cpp`, `src/dsp/Waveshapers.h`, `src/dsp/Oversampler.h/.cpp` | Plan §5, §9. The plan's `DriveModel` interface is one class configured per mode by profile data; a measured model with a different structure would replace it behind the same calls. Oversampling factor is fixed at prepare time because it changes the latency (plan §9.2). |
| Added in M1 step 3: `src/dsp/Compressor.h/.cpp` | Plan §5, §8. |
| Added in M1 step 2: `src/dsp/Biquad.h/.cpp`, `src/dsp/ThreeBandEq.h/.cpp` | `ThreeBandEq` per plan §5; `Biquad` holds the cookbook designs and the filter they run in, shared with later blocks. |
| Not created: `FixedPoint` | Plan §13: disabled until evidence supports a configuration; not needed for Milestone 1. |
| Added in M1 step 1: `src/core/ModelProfile.h`, `src/core/StepMapping.h/.cpp`, `src/dsp/BypassCrossfade.h`, `src/dsp/DenormalGuard.h` | Plan §14 (one profile for hardware values) and §7.3 (documented step → algorithm value); §15 switching; denormals inside the JUCE-free engine. |
| Not yet created: `src/ui/ProgramDisplay` | Bank/program workflow is Milestone 2. |
| Not yet created: `tools/*`, `docs/measurement-protocol.md` | Milestone 4. Directories exist. |
| Added `docs/source-register.md` early | The evidence register needs somewhere to cite sources. |
| Removed the CLion template `main.cpp` | Replaced by the plugin targets. |
