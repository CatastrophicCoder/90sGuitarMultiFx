# Validation plan

How each milestone is shown to work. Tests run with `ctest`; see the README for commands.

## Test suites

| Executable | Links | Purpose |
|------------|-------|---------|
| `fivea_engine_tests` | `fivea_engine` only | Engine behaviour with no JUCE and no host; proves the DSP library stands alone (plan §4). |
| `fivea_plugin_tests` | plugin shared code | The `AudioProcessor` as a host drives it: buses, `processBlock`, state, bypass parameter, editor. |

Planned directories: `tests/Golden` (rendered reference files) and `tests/TestSignals` (generated
stimuli) are empty until Milestone 1 and Milestone 4 respectively.

## Milestone 0 coverage

| Requirement (plan §22) | Test | Status |
|------------------------|------|--------|
| Pass-through | `Default settings pass audio through bit for bit` (5 rates × mono/stereo × block sizes 1, 64, 512); `Stereo pass-through through the AudioProcessor at default settings`; `Default settings stay bit-transparent after a state reload` | Passing |
| Silence | `Silence in gives finite silence out, even with gain applied` | Passing |
| Reset determinism | `Reset makes processing deterministic` | Passing |
| Mono and stereo layouts | engine pass-through (1 and 2 channels); `Mono input feeds both outputs of a stereo layout`; `Supported bus layouts` | Passing |
| State round trip | `State round trip restores every parameter and the schema version`; `State records the schema version and model` | Passing |
| State robustness (plan §16) | unknown fields ignored, missing → default, out of range → clamped, newer schema loads, foreign/unversioned/corrupt rejected | Passing |
| Zero-length blocks, one-sample blocks (plan §19.1) | `Zero-length blocks are accepted`; block size 1 in pass-through | Passing |
| Block-size invariance | `Output does not depend on how the host splits blocks` | Passing |
| Non-finite parameter input | `Out-of-range and non-finite gains are sanitised` | Passing |
| Gain accuracy and smoothing | `Gain settles at the requested level after the ramp`; `LinearSmoother` tests | Passing |
| Bypass | `Global bypass returns to exact pass-through once its ramp ends`; bypass parameter exposed to hosts | Passing |
| Editor | `The editor opens and closes` | Passing |
| No allocation in `process()` (plan §19.1, §19.4) | `FiveAProcessor does not allocate while setting parameters and processing`, with a counting replacement of the global `operator new` (`tests/Unit/AllocationGuard.cpp`) and a self-test that it sees allocations | Passing (M1 step 1) |
| Documented values come out exactly | `StepMappingTests.cpp`: EQ frequencies, chorus/flanger delays, delay times and maxima, MIX end points, mode clamping | Passing (M1 step 1) |
| Placeholder mappings | Direction from the manual's wording, symmetry, finiteness over and beyond the range | Passing (M1 step 1) |
| 3 Band EQ (plan §10.2) | `ThreeBandEqTests.cpp`: designs equal the cookbook's analog prototypes under the bilinear transform (1e-6 dB); shelf plateau and corner gains; peak gain at each documented MID FREQ; cut mirrors boost; measured response matches the design within 0.1 dB at 44.1–192 kHz; TRIM; bit-exact when flat since reset; no click on stepped changes (second-difference bound with measured margins); return to flat; block-size invariance; finite at extreme settings; no allocation | Passing (M1 step 2) |
| Compressor (plan §8.2) | `CompressorTests.cpp`: bit-exact below threshold at LEVEL 12; settled output on the static curve (0.01 dB) for four SENS values; ratio; attack time constant per ATTACK step at 44.1–192 kHz; release time constant; linked channels; no click on a LEVEL step (bound from measurement, change at a sine peak); silence, extreme levels and settings finite; block-size invariance; no allocation | Passing (M1 step 3) |
| Distortion/Overdrive (plan §9) | `DriveTests.cpp`: waveshapers bounded, continuous, never decreasing; latency 0/69/76; silence → silence; output bounded at maximum drive; more DRIVE → more harmonics (both modes); Distortion has even harmonics, Overdrive does not; no DC at the output; TONE brightens; oversampling cuts aliasing (measured 0.286 → 0.092 at 2× → 1.2e-5 at 4×); no click on MODE/DRIVE/TONE/LEVEL changes (ratio bound from measurement); MODE reversal mid-fade; all rates; block-size invariance; no allocation | Passing (M1 step 4) |
| Oversampler | `OversamplerTests.cpp`: factor 1 pass-through; impulse comes out at the reported latency; passband error below 0.002 up to 18 kHz; aliasing at least 80 dB lower; channels independent; no allocation | Passing (M1 step 4) |
| Chorus/Flanger (plan §11.5) | `ModulationTests.cpp`: delay line exact at whole samples, exact on a ramp, close on a sine, clamped; each MODE's delay equals the documented one (written out in the test) at 44.1/48/96 kHz, recovered sample by sample from a ramp input; cubic interpolation accuracy at 4 kHz (bound from measurement); DEPTH range and SPEED rate; right channel 90° ahead; mono in → stereo out; MIX 0 bit-exact dry; F.BACK repeats; maximum feedback bounded in every mode; no click on any change (ratio bound from measurement); silence, extremes, all rates; block-size invariance; no allocation | Passing (M1 step 5) |
| Switching crossfade, denormals | `BypassCrossfadeTests.cpp`, `DenormalGuardTests.cpp` | Passing (M1 step 1) |
| Builds on Windows, macOS, Linux | `.github/workflows/build.yml` | Passing: all jobs green on `ec6afd2` (macOS, Windows, Linux; Debug and Release). The first run failed on Linux (missing `libxi-dev`, then a PIC link error), fixed in `07fc0ca` |
| Plugin validation | `auval -v aufx Nmf1 Ctcd`; pluginval strictness 10 on the VST3 and the AU | Passing locally (macOS) and in CI on `ec6afd2`: auval and pluginval on macOS, pluginval on Windows |
| Plugin passes audio in a host (plan §21 M0, amended 2026-10-05) | Covered at the `processBlock` level above, and by auval and pluginval; not yet listened to in a DAW | Manual check outstanding |

## Not yet covered (planned)

| Property | Plan | Milestone |
|----------|------|-----------|
| All 32 on/off combinations of the five blocks | §19.2 | 1 |
| Per-block unit tests (time effects) | §8.2, §11.5, §19.1 | 1 |
| Program changes while running, crossfades | §19.2 | 2 |
| Sample-rate changes during a session, offline vs real-time consistency | §19.2 | 1–3 |
| Golden renders with tolerances | §19.3 | 1 |
| Performance: per-block time, peak time, denormals | §19.4 | 1 |
| Static analysis in CI | §20 | not yet configured |
| Fuzzing of state parsing | §21 M6 | 6 |
