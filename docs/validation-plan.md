# Validation plan

How each milestone is shown to work. Tests run with `ctest`; see the README for commands.

## Test suites

| Executable | Links | Purpose |
|------------|-------|---------|
| `a5_engine_tests` | `a5_engine` only | Engine behaviour with no JUCE and no host; proves the DSP library stands alone (plan §4). |
| `a5_plugin_tests` | plugin shared code | The `AudioProcessor` as a host drives it: buses, `processBlock`, state, bypass parameter, editor. |

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
| Builds on Windows, macOS, Linux | `.github/workflows/build.yml` | Passing: all jobs green on `ec6afd2` (macOS, Windows, Linux; Debug and Release). The first run failed on Linux (missing `libxi-dev`, then a PIC link error), fixed in `07fc0ca` |
| Plugin validation | `auval -v aufx Nmf1 Ctcd`; pluginval strictness 10 on the VST3 and the AU | Passing locally (macOS) and in CI on `ec6afd2`: auval and pluginval on macOS, pluginval on Windows |
| Standalone passes audio | Covered at the `processBlock` level above; the standalone application itself has not been run with an audio device | Manual check outstanding |

## Not yet covered (planned)

| Property | Plan | Milestone |
|----------|------|-----------|
| No allocation in `process()` | §19.1, §19.4 | 1 (needs an allocation-counting harness) |
| All 32 on/off combinations of the five blocks | §19.2 | 1 |
| Per-block unit tests (compressor, drive, EQ, modulation, time effects) | §8.2, §11.5, §19.1 | 1 |
| Program changes while running, crossfades | §19.2 | 2 |
| Sample-rate changes during a session, offline vs real-time consistency | §19.2 | 1–3 |
| Golden renders with tolerances | §19.3 | 1 |
| Performance: per-block time, peak time, denormals | §19.4 | 1 |
| Static analysis in CI | §20 | not yet configured |
| Fuzzing of state parsing | §21 M6 | 6 |
