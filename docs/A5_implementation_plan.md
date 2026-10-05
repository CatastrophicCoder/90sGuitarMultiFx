# Project: A5 Guitar-Inspired Audio Plugin

## 1. Objective

Create a cross-platform audio plugin inspired by the architecture and workflow of the red Korg A5 Guitar performance signal processor.

The initial goal is NOT to claim bit-accurate emulation.

The project must progress through three clearly separated fidelity levels:

1. Functional reconstruction
    - Reproduce the documented serial effect architecture.
    - Reproduce the documented parameter and program workflow.
    - Produce a usable guitar multi-effect plugin.

2. Measurement-assisted sound-alike
    - Add behavior inferred from controlled measurements of a physical A5 Guitar.
    - Match frequency response, nonlinear transfer curves, dynamics, modulation, delay, and reverb behavior.

3. Optional research emulation
    - Investigate fixed-point behavior and the original DSP architecture.
    - Only call this an emulation if evidence supports that wording.
    - Do not incorporate copyrighted firmware or ROM data into the repository.

For now, implement level 1 while designing the codebase so levels 2 and 3 can be added later.

---

# 2. Evidence classification

Every behavior must be classified as one of:

- CONFIRMED:
  Explicitly documented in an owner manual, service manual, schematic, parameter chart, or verified measurement.

- INFERRED:
  Strongly suggested by the architecture, but not explicitly documented.

- PLACEHOLDER:
  A reasonable initial implementation used until measurements are available.

Never present an inferred or placeholder behavior as an authentic property of the original hardware.

Maintain:

docs/evidence-register.md

For each technical decision, record:

- feature
- status: CONFIRMED / INFERRED / PLACEHOLDER / MEASURED
- source
- implementation consequence
- confidence
- unresolved questions

---

# 3. Confirmed original architecture

## 3.1 Core audio path

Implement the following serial effect structure:

Input
→ Compressor
→ Distortion / Overdrive
→ 3-band EQ
→ Chorus / Flanger
→ Reverb / Delay
→ Output

Each of the five major blocks must be independently bypassable.

The original unit processes audio at 44.1 kHz.

The original hardware contains:

- M50941-family host/controller CPU
- TMS57002 digital audio signal processor
- MN86081 ADC
- µPD6376 DAC
- 512-kbit external dynamic memory
- model-specific internal CPU program/data

The Guitar and Multi-FX models share the same broad hardware architecture, but their controller program/data differs. The Guitar version provides distortion/overdrive where the Multi-FX version provides an exciter.

Do not assume that the M50941 performs audio processing. Treat it conceptually as the host/controller responsible for user controls, program handling, DSP loading, and coefficient updates unless stronger evidence is later found.

## 3.2 Programs

The documented program structure is:

- 6 banks
- 5 programs per bank
- 30 total program locations
- Bank 1: writable user programs
- Banks 2–6: 25 factory programs

For the plugin:

- Preserve the six-bank/five-program organization in an optional "Hardware" browser.
- Also expose normal plugin-host preset support.
- Do not ship claimed factory presets until their exact values have been verified.
- Use clearly named development presets such as:
    - Clean Compression
    - Soft Drive
    - Hard Drive
    - Wide Chorus
    - Short Ambience

## 3.3 Stereo behavior

The original provides:

- left/mono output
- right output
- stereo headphone output

However, do not assume where the signal becomes stereo.

During the placeholder phase:

- Keep compressor, drive, and EQ mono-compatible.
- Permit chorus/flanger and reverb/delay to produce stereo.
- Ensure mono input produces valid stereo output.
- Make L/Mono behavior testable as a separate routing mode.

Mark this behavior as PLACEHOLDER until measured.

---

# 4. Recommended technology

Use:

- C++20
- JUCE
- CMake
- Catch2 or GoogleTest for tests
- GitHub Actions for continuous integration

Initial plugin formats:

- VST3
- Audio Unit on macOS

(amended 2026-10-05: no standalone; the plugin goes in front of an amp, as the original was used, and has no use on its own)

Architect the project so CLAP can be added later without changing DSP internals.

Avoid coupling DSP code directly to JUCE UI classes.

The DSP library must be independently testable without loading a plugin host.

---

# 5. Repository structure

Create the following structure:

/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── THIRD_PARTY_NOTICES.md
├── cmake/
├── docs/
│   ├── architecture.md
│   ├── evidence-register.md
│   ├── parameter-specification.md
│   ├── measurement-protocol.md
│   ├── validation-plan.md
│   └── source-register.md
├── resources/
│   └── presets/
├── src/
│   ├── plugin/
│   │   ├── PluginProcessor.cpp
│   │   ├── PluginProcessor.h
│   │   ├── PluginEditor.cpp
│   │   └── PluginEditor.h
│   ├── core/
│   │   ├── A5Processor.cpp
│   │   ├── A5Processor.h
│   │   ├── ParameterIds.h
│   │   ├── ParameterLayout.cpp
│   │   ├── PresetState.cpp
│   │   └── PresetState.h
│   ├── dsp/
│   │   ├── Compressor.cpp
│   │   ├── Compressor.h
│   │   ├── Drive.cpp
│   │   ├── Drive.h
│   │   ├── ThreeBandEq.cpp
│   │   ├── ThreeBandEq.h
│   │   ├── Modulation.cpp
│   │   ├── Modulation.h
│   │   ├── TimeEffects.cpp
│   │   ├── TimeEffects.h
│   │   ├── DelayLine.cpp
│   │   ├── DelayLine.h
│   │   ├── FixedPoint.cpp
│   │   ├── FixedPoint.h
│   │   ├── Smoothing.h
│   │   └── DenormalGuard.h
│   └── ui/
│       ├── MainPanel.cpp
│       ├── MainPanel.h
│       ├── EffectSection.cpp
│       ├── EffectSection.h
│       ├── ProgramDisplay.cpp
│       └── ProgramDisplay.h
├── tests/
│   ├── Unit/
│   ├── Integration/
│   ├── Golden/
│   └── TestSignals/
└── tools/
├── render_impulse_response.cpp
├── analyze_transfer_curve.py
├── analyze_frequency_response.py
└── compare_recordings.py

Do not add generated build files to source control.

---

# 6. DSP engine design

## 6.1 Top-level processing object

Create:

class A5Processor

Responsibilities:

- own all effect blocks
- prepare them for sample rate and maximum block size
- process mono and stereo layouts
- implement documented serial ordering
- apply block bypass states
- handle parameter smoothing
- reset delay lines and envelope state safely
- expose deterministic state serialization
- provide a global authenticity mode

Suggested conceptual interface:

class A5Processor
{
public:
void prepare(const ProcessSpec&);
void reset();

    void setParameters(const ParameterSnapshot&);
    void process(AudioBufferView);

    void setAuthenticityMode(AuthenticityMode);
};

The audio thread must perform:

- no memory allocation
- no file access
- no locking
- no logging
- no unbounded loops
- no object construction
- no host-state parsing

Parameter changes must be transferred to the audio thread using an allocation-free snapshot or atomic parameter values.

## 6.2 Internal sample rate

Implement two modes:

### Native mode

- Process at host sample rate.
- Convert time-based parameters to maintain equivalent durations.

### Hardware-rate mode

- Convert host audio to an internal 44.1 kHz processing domain.
- Run the complete emulation chain at 44.1 kHz.
- Resample back to host rate.
- Report any resulting latency to the host.

Do not implement hardware-rate conversion in the first commit unless the abstraction is clean.

Initial milestone:

- native host-rate processing
- all coefficient calculations sample-rate aware
- unit tests at 44.1, 48, 88.2, 96, and 192 kHz

Later milestone:

- optional internal 44.1 kHz mode using a high-quality streaming resampler

Do not assume that merely processing at 44.1 kHz recreates the original converters or DSP arithmetic.

---

# 7. Parameter system

## 7.1 Requirements

Parameters must:

- have stable string identifiers
- support automation
- serialize deterministically
- be smoothed where discontinuities could create clicks
- use normalized host values but meaningful internal units
- avoid changing IDs after release

Create a central ParameterIds.h file.

## 7.2 Global parameters

Implement:

- input trim
- output level
- global bypass
- authenticity mode
- mono/stereo routing mode
- optional input peak indicator

Initial input/output range suggestion:

- -24 dB to +24 dB

This is a plugin design choice, not a claim about the original hardware.

## 7.3 Block parameters

Do not invent original parameter labels or ranges in the user-facing "Hardware" mode until the owner manual parameter table has been transcribed and verified.

Internally, the first functional version may expose modern engineering parameters.

Keep a mapping layer:

DocumentedParameter
→ NormalizedValue
→ AlgorithmParameter

This permits later substitution of exact A5 value mappings.

---

# 8. Compressor implementation

## 8.1 Initial algorithm status

Status: PLACEHOLDER

Use a conventional feed-forward compressor.

Implement:

- sidechain detector
- envelope follower
- threshold
- ratio
- attack
- release
- optional soft knee
- makeup gain
- effect bypass

Avoid introducing lookahead because that would add behavior and latency not presently documented.

Use a stable logarithmic-domain gain computer.

Support peak detection first.

Keep detector strategy replaceable:

enum class DetectorType
{
Peak,
MeanSquare,
QuasiRms
};

Start with Peak.

## 8.2 Testing

Add tests for:

- unity below threshold
- expected static compression ratio
- attack convergence
- release convergence
- no NaN or infinity
- stable behavior with silence
- block-size invariance
- bypass transparency
- no allocation during processing

---

# 9. Distortion and overdrive implementation

## 9.1 Architecture

Status: PLACEHOLDER pending physical-unit measurement.

Create one drive block with two modes:

- Overdrive
- Distortion

Use an interchangeable model architecture:

interface DriveModel
{
prepare(...)
reset()
processSample(...)
};

Implement initial models:

### Overdrive placeholder

- input high-pass filter
- pre-emphasis filter
- asymmetric or near-symmetric soft clipping
- post-clipping low-pass filter
- output level compensation

### Distortion placeholder

- input high-pass filter
- pre-emphasis filter
- harder piecewise clipping
- post-clipping tone filter
- output level compensation

Do not hard-code tanh as the permanent A5 model.

Provide candidate waveshapers behind one interface:

- cubic soft clip
- rational soft clip
- piecewise-linear clip
- asymmetric piecewise clip
- hard clip

## 9.2 Oversampling

Provide optional 2x and 4x oversampling in the placeholder implementation to control aliasing.

However:

- modern oversampling must be disabled in strict hardware-rate research mode unless measurements justify it
- oversampling filters and latency must be accounted for
- switching oversampling modes must be click-free or occur only during reinitialization

## 9.3 Future measured model

Design for transfer-curve lookup tables:

- positive and negative branches
- level-dependent interpolation
- pre/post frequency-dependent behavior
- optional hysteresis only if measurement demonstrates it

Required future measurements:

- DC transfer curve where safe
- low-frequency sine transfer curve
- harmonic spectrum at multiple input amplitudes
- intermodulation test
- swept-sine response at several drive levels
- output level versus input level
- alias behavior at 44.1 kHz
- response with drive bypassed

---

# 10. Three-band EQ

## 10.1 Initial design

Status: PLACEHOLDER unless exact manual mappings are transcribed.

Implement:

- low shelf
- peaking mid band
- high shelf

Use stable biquad filters.

Start with fixed center/corner frequencies stored in a model profile rather than scattered constants.

Example structure:

struct EqProfile
{
double lowFrequency;
double midFrequency;
double midQ;
double highFrequency;
double maxGainDb;
};

Do not label any initial frequencies as original A5 values.

## 10.2 Requirements

- coefficients must update smoothly
- avoid unstable intermediate coefficients
- preserve correct channel state
- bypass must be phase-transparent only when completely bypassed
- effect activation must not produce a click

## 10.3 Future measurement

Measure:

- each EQ control at minimum, center, and maximum
- individual band interactions
- center detent behavior
- boost/cut symmetry
- possible shared gain normalization
- whether EQ operates before or after any drive output scaling

---

# 11. Chorus and flanger

## 11.1 Shared modulation engine

Implement chorus and flanger as alternate modes of one modulation block.

Core components:

- fractional delay line
- LFO
- wet/dry mixer
- optional feedback
- optional stereo phase relationship
- parameter smoothing

The interpolation implementation must be replaceable:

enum class Interpolation
{
Linear,
Cubic,
Lagrange3,
Allpass
};

Start with linear or cubic interpolation, but mark it as PLACEHOLDER.

## 11.2 Chorus placeholder

Provide internal engineering parameters:

- rate
- depth
- base delay
- stereo phase
- wet level
- dry level

Do not expose undocumented parameters in Hardware UI mode.

## 11.3 Flanger placeholder

Provide:

- rate
- depth
- base delay
- feedback
- polarity
- wet/dry mix

## 11.4 Measurement targets

Determine:

- LFO shape
- modulation rate mapping
- depth mapping
- minimum and maximum delay
- stereo phase relationship
- feedback amount and sign
- interpolation character
- dry/wet topology
- whether direct sound is omitted at any setting
- modulation reset behavior on program change
- whether LFO phase is shared between programs or restarted

## 11.5 Tests

- no delay-buffer overrun
- bounded feedback
- no NaN during maximum feedback
- no clicks from ordinary parameter changes
- repeatable output with fixed initialization
- correct stereo behavior
- block-size invariance within floating-point tolerance

---

# 12. Reverb and delay

The final major block must select between Reverb and Delay behavior.

Do not initially assume both algorithms run simultaneously unless the manual explicitly verifies this.

## 12.1 Delay placeholder

Implement:

- fractional or integer delay line
- feedback
- tone filtering in feedback path
- wet/dry mix
- stereo routing
- safe feedback limiting

The original external memory is documented as 512 kbit, but do not infer the exact available delay time without knowing:

- stored sample width
- memory division
- addressing scheme
- mono/stereo allocation
- memory used by chorus, flanger, or reverb

Therefore, do not advertise a historically exact maximum delay yet.

Use a practical placeholder maximum and isolate it in a ModelProfile.

## 12.2 Reverb placeholder

Implement a replaceable algorithm interface.

First version:

- short input diffusion
- parallel comb filters or compact feedback delay network
- output diffusion
- damping
- wet/dry mix

Do not call the initial algorithm "the A5 reverb."

It is a functional placeholder.

## 12.3 Reverb measurement targets

Measure:

- impulse response for every documented reverb type
- decay time at different settings
- early-reflection structure
- frequency-dependent decay
- stereo cross-correlation
- modulation in the tail
- nonlinear or level-dependent behavior
- truncation or noise-floor behavior
- program-change tail handling

---

# 13. Fixed-point and vintage-hardware layer

Do not add arbitrary bit crushing, noise, or a 17 kHz low-pass filter merely because the original dynamic-range and frequency-response specifications are limited.

Those specifications describe system-level performance and do not by themselves identify:

- internal arithmetic width
- converter bit depth
- quantization placement
- dither
- clipping rules
- rounding rules
- saturation behavior
- exact anti-alias filter response

Create an optional FixedPointProfile abstraction, disabled by default:

struct FixedPointProfile
{
int signalFractionBits;
int coefficientFractionBits;
RoundingMode rounding;
OverflowMode overflow;
};

Possible modes:

- FloatingPoint
- ResearchFixedPoint
- MeasuredHardware

Do not enable ResearchFixedPoint in release presets until evidence supports its configuration.

Similarly, create optional:

- converter-response filter
- noise model
- DC offset model
- saturation model

All should remain disabled until measured.

---

# 14. Model profile

Centralize all potentially hardware-specific values.

Create:

struct A5ModelProfile
{
double internalSampleRate;
CompressorProfile compressor;
DriveProfile overdrive;
DriveProfile distortion;
EqProfile eq;
ModulationProfile chorus;
ModulationProfile flanger;
DelayProfile delay;
ReverbProfile reverb;
FixedPointProfile fixedPoint;
RoutingProfile routing;
};

Provide:

- FunctionalPlaceholderProfile
- later: MeasuredUnitProfile
- later: ResearchHardwareProfile

Never scatter hardware constants across processors.

---

# 15. Bypass and switching behavior

Support:

- global bypass
- individual effect bypass
- program changes
- mode changes

Initial policy:

- global bypass should be latency compensated
- individual bypass should crossfade over a short configurable interval
- program changes should use an internal transition to prevent clicks
- delay and reverb tails should follow an explicit policy

Tail policies:

enum class TailPolicy
{
CutImmediately,
Preserve,
Crossfade
};

Use Crossfade as the plugin default.

If measurement later shows that the physical unit cuts tails, implement that behavior in Hardware mode.

---

# 16. Plugin state and presets

Serialize:

- schema version
- all stable parameter IDs
- effect enabled states
- selected algorithms
- routing mode
- authenticity mode
- bank/program metadata
- preset name

Requirements:

- old states must migrate forward
- unknown fields must be ignored safely
- missing fields must receive defaults
- state loading must not allocate on the audio thread
- corrupted state must fail safely

Example conceptual schema:

{
"schemaVersion": 1,
"model": "functional-placeholder",
"bank": 1,
"program": 1,
"name": "Soft Drive",
"parameters": {
}
}

Do not include original firmware, ROM contents, copied graphics, or unlicensed factory-preset data.

---

# 17. User interface

## 17.1 Initial UI

Create a clean, functional interface rather than a pixel-perfect copy.

Sections:

- input
- compressor
- distortion/overdrive
- three-band EQ
- chorus/flanger
- reverb/delay
- output
- bank/program display

Each effect section must show:

- enable/bypass control
- algorithm selector where applicable
- parameter controls
- input or output activity where useful

## 17.2 Hardware-inspired mode

A later Hardware UI mode may reflect:

- six banks
- five programs per bank
- five major effect buttons
- compact numeric display
- direct manual/edit workflow

Do not copy Korg logos, trade dress, panel artwork, typography, or product photography.

Use a distinct project name and visual identity until trademark and product-naming issues have been reviewed.

---

# 18. Measurement application

Create a standalone measurement harness in addition to the plugin.

It should generate and export:

- impulse
- logarithmic sine sweep
- stepped sine
- amplitude ramp
- two-tone intermodulation signal
- white noise
- pink noise
- silence
- polarity test
- stereo-identification signal

It should ingest recorded A5 output and produce:

- gain
- latency
- frequency response
- phase response
- THD and harmonic spectrum
- dynamic transfer curve
- attack/release estimate
- modulation-rate estimate
- delay-time estimate
- impulse response
- stereo correlation
- null comparison against plugin output

Store raw captures outside the source repository unless redistribution rights are clear.

Store measurement metadata:

- unit serial or anonymized unit ID
- input and output interface
- interface sample rate
- gain settings
- A5 input level
- A5 output level
- selected program
- all parameter positions
- mono/stereo connection
- capture date
- calibration procedure
- clipping status

---

# 19. Test strategy

## 19.1 Unit tests

Test each DSP block independently.

Mandatory properties:

- silence produces finite output
- no NaN or infinity
- reset produces deterministic state
- bypass behaves as specified
- processing does not allocate
- unsupported sample rates do not crash
- zero-length blocks do not crash
- one-sample blocks work
- maximum expected block size works
- mono and stereo layouts work
- extreme parameter combinations remain bounded

## 19.2 Integration tests

Test the full chain:

- serial ordering
- all 32 on/off combinations of five effects
- program changes while audio is running
- state save/restore
- host automation
- sample-rate changes
- block-size changes
- mono-to-stereo behavior
- offline and real-time output consistency

## 19.3 Golden tests

Render deterministic test files and compare:

- impulse response
- sine response
- noise response
- guitar DI reference clips

Use tolerances appropriate to the algorithm.

Do not use strict sample-for-sample comparison for algorithms containing intentionally free-running modulation unless their seeds and phases are fixed in test mode.

## 19.4 Performance tests

Capture:

- processing time per block
- peak processing time
- memory usage
- allocations during prepare
- allocations during process
- denormal behavior
- performance with all effects enabled

Success criterion:

- no audio-thread allocation
- no unbounded CPU spikes
- stable operation for long-running silence and feedback tests

---

# 20. Continuous integration

Configure builds for:

- Windows
- macOS
- Linux

CI jobs:

1. configure
2. build Debug
3. build Release
4. run unit tests
5. run integration tests
6. run formatting check
7. run static analysis where available
8. package the plugins for testing (amended 2026-10-05: no standalone; the plugin goes in front of an amp, as the original was used, and has no use on its own)
9. retain test reports

Do not add code-signing or notarization secrets at this stage.

---

# 21. Development milestones

## Milestone 0: Evidence and skeleton

Deliver:

- repository skeleton
- CMake project
- JUCE plugin targets (amended 2026-10-05: no standalone; the plugin goes in front of an amp, as the original was used, and has no use on its own)
- empty A5Processor
- evidence register
- architecture document
- parameter specification template
- passing CI
- basic audio pass-through

Acceptance:

- project builds on supported platforms
- plugin passes audio in a host (amended 2026-10-05: no standalone; the plugin goes in front of an amp, as the original was used, and has no use on its own)
- plugin state can save and restore a version number
- no DSP behavior is falsely described as authentic

## Milestone 1: Complete functional chain

Deliver:

- compressor
- overdrive/distortion
- 3-band EQ
- chorus/flanger
- delay/reverb
- effect bypass
- native sample-rate operation
- basic UI

Acceptance:

- all effects process audio
- order matches documented chain
- all effect combinations work
- no audio-thread allocation
- automated tests pass

## Milestone 2: Programs and workflow

Deliver:

- six banks
- five programs per bank
- user bank behavior
- host preset support
- state migration framework
- program change crossfading

Acceptance:

- 30 program slots represented
- user programs persist
- program changes do not produce severe clicks
- factory slots are clearly identified as development presets

## Milestone 3: Hardware-rate mode

Deliver:

- internal 44.1 kHz processing option
- host-rate conversion
- latency reporting
- sample-rate tests

Acceptance:

- works at common host sample rates
- no drift in long renders
- channel alignment preserved
- latency reported correctly

## Milestone 4: Measurement tooling

Deliver:

- test-signal generator
- capture metadata format
- comparison scripts
- frequency and transfer-curve analysis
- impulse-response analysis

Acceptance:

- recorded hardware captures can be compared against plugin output
- results are reproducible from stored metadata

## Milestone 5: Measurement-driven calibration

Deliver:

- measured compressor model
- measured drive transfer behavior
- measured EQ curves
- measured modulation behavior
- measured delay/reverb characteristics

Acceptance:

- every calibrated component has evidence recorded
- placeholder constants replaced only where measurements support replacement
- before/after comparison renders available

## Milestone 6: Release hardening

Deliver:

- accessibility review
- preset migration tests
- plugin validation
- crash and fuzz testing for state parsing
- packaging
- legal and attribution review
- final documentation

---

# 22. First Claude Code task

Start with Milestone 0 only.

Perform these actions:

1. Inspect the repository before changing anything.
2. If the repository is empty, create the proposed structure.
3. Add a root CMake project using C++20.
4. Integrate JUCE in the least intrusive reproducible way supported by the existing repository.
5. Create VST3, AU where supported, and Standalone targets.
6. Implement transparent stereo pass-through.
7. Create the A5Processor façade with prepare, reset, and process methods.
8. Add stable placeholder parameter IDs for:
    - input trim
    - output level
    - five effect enable states
    - global bypass
9. Implement versioned state serialization.
10. Add a basic editor containing:
    - project title
    - input control
    - five disabled effect sections
    - output control
11. Add tests for:
    - pass-through
    - silence
    - reset determinism
    - mono and stereo layouts
    - state round trip
12. Add:
    - docs/architecture.md
    - docs/evidence-register.md
    - docs/parameter-specification.md
    - docs/validation-plan.md
13. Add build instructions to README.md.
14. Build and run tests locally.
15. Fix all compilation and test failures before stopping.
16. Summarize:
    - files created
    - architectural decisions
    - tests executed
    - unresolved issues
    - exact commands used

Do not implement speculative DSP algorithms during Milestone 0.

Do not claim that any placeholder implementation reproduces the original A5 sound.

---

# 23. Coding standards

Use:

- RAII
- explicit ownership
- const correctness
- no raw owning pointers
- small testable classes
- no audio-thread locks
- no audio-thread allocation
- no hidden mutable global state
- deterministic initialization
- sanitized parameter ranges
- centralized unit conversion
- descriptive names rather than abbreviations

Document why, not what.

Avoid premature generic frameworks. Introduce abstractions only where the implementation plan already requires replacement or calibration.

---

# 24. Definition of done

A milestone is complete only when:

- code builds
- tests pass
- documentation matches implementation
- no confirmed fact is contradicted
- assumptions are labelled
- plugin state remains backward compatible
- no real-time safety violation is known
- no copyrighted firmware or copied product artwork is included
- generated outputs have been inspected