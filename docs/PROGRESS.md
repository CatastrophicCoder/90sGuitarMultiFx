# Progress

Current status, decisions made, and a short log of each working session. The plan and milestone
definitions are in `docs/implementation-plan.md` (§21); what is known about the original unit
versus assumed is in `docs/evidence-register.md`.

## Milestone status

| Milestone | Status | Notes |
|-----------|--------|-------|
| 0 Evidence and skeleton | Done (2026-10-05) | Pass-through plugin, versioned state, evidence register. CI green on macOS, Windows and Linux (`ec6afd2`). Outstanding: listen to it once in a DAW, in front of an amp sim. |
| 1 Complete functional chain | Done (2026-10-07) | All nine steps done; acceptance below. CI green on all platforms (`d29e268`). Outstanding: listening in a DAW and comparing the panel with the original. |
| 2 Programs and workflow | Done (2026-10-07) | Acceptance below. CI green on all platforms (`b20456b`). Outstanding: listening to program changes in a DAW. |
| 3 Hardware-rate mode | Not started | |
| 4 Measurement tooling | Not started | |
| 5 Measurement-driven calibration | Not started | Needs access to a physical unit. |
| 6 Release hardening | Not started | Packaging (`.pkg`/`.dmg`) and `CHANGELOG.md` arrive here, as in the other Catastrophic Audio projects. |

## Milestone 1 acceptance (plan §21)

| Criterion | Evidence | Result |
|-----------|----------|--------|
| Compressor, overdrive/distortion, 3-band EQ, chorus/flanger, delay/reverb | One block each in `src/dsp`, each with its own test file | Met |
| Effect bypass | Per-effect crossfade, reset on fade-out, input fade for memory blocks (`ChainTests`) | Met |
| Native sample-rate operation | Every block and the chain tested at 44.1, 48, 88.2, 96 and 192 kHz; re-preparing at new rates | Met |
| Basic UI | The replica panel in Manual/Edit mode (beyond "basic", by decision) | Met |
| All effects process audio | 32-combination test; every effect on through `processBlock` | Met |
| Order matches the documented chain | The engine equals the documented order built by hand from the blocks, bit for bit, for all 32 combinations | Met |
| All effect combinations work | Same test | Met |
| No audio-thread allocation | Counting `operator new` around the full chain while switching everything; every block on its own | Met |
| Automated tests pass | 144 tests, macOS Debug and Release locally; ASan/UBSan clean; pluginval strictness 10, auval. CI on `710a5e5`: macOS and Linux green (Debug, Release); Windows failed only the three non-ASCII-named tests | Met: CI green on macOS, Windows and Linux, Debug and Release (`d29e268`) |

Outstanding manual checks (the owner's): listening in a DAW, and comparing the panel with the
original.

## Milestone 2 acceptance (plan §21)

| Criterion | Evidence | Result |
|-----------|----------|--------|
| Six banks, five programs per bank; 30 program slots represented | `ProgramBank` (30 slots); the host's program list (30 named programs); the panel's Program mode (`ProgramBankTests`, `ProgramChangeTests`, `PanelTests`) | Met |
| User bank behaviour | Only bank 1 is writable, by WRITE on the panel or `writeProgram()`; banks 2–6 refuse it; bank 1 starts as copies of five presets | Met |
| User programs persist | State schema 2 saves bank 1, the selected program and the mode; a program written on the panel survives saving and reloading the session (`PanelTests`, `ProgramStateTests`) | Met |
| Host preset support | `getNumPrograms()` 30, names "2-1 METAL 1", `setCurrentProgram()` selects, bank 1 renamable; the session state for the host's own presets | Met |
| State migration framework | `currentSchemaVersion` 2 with a migration step; schema 1 states load with the factory programs; unknown, missing, unreadable and out-of-range values handled per plan §16 | Met |
| Program change crossfading; no severe clicks | The output dip (owner's decision): click ratio 0.04–8.6 against a bound of 20, into and out of every factory program; no allocation; skipped under bypass | Met |
| Factory slots clearly identified as development presets | 24 slots named "DEV …"; 2-1 is METAL 1 from the owner's manual, the one documented preset (`FactoryProgramsTests`) | Met |
| Automated tests pass | 204 tests, macOS Debug, Release and the hardened build; ASan/UBSan engine tests clean; auval, pluginval strictness 10 (VST3, AU) | Met: CI green on macOS, Windows and Linux, Debug and Release (`b20456b`) |

Outstanding manual checks (the owner's): listening to program changes in a DAW, and comparing the
Program-mode panel with the original.

## Carried over to Milestone 3 and later

- The volume pedal input (EV-016), left out of Milestones 1–2 by decision.
- The original's 24 other factory presets and METAL 1's Utility values, if the Effect Parameter
  List is found (EV-010, EV-120).
- L/Mono routing mode (plan §3.3) and authenticity mode (plan §7.2).
- Hardware-rate mode (Milestone 3); measurement tooling (Milestone 4); golden renders (4–5).
- CI: static analysis; the GitHub actions still target the deprecated Node.js 20.

## Decisions

Owner's decisions, with dates. Engineering decisions made while implementing are recorded in
`docs/architecture.md`, and hardware-related ones in `docs/evidence-register.md`.

- 2026-10-04: Company **Catastrophic Audio**; manufacturer code `Ctcd`, shared with the other
  plugins; plugin code `Nmf1`. Do not change the codes: hosts use them to recall saved sessions.
- 2026-10-04: Public GitHub repository `CatastrophicCoder/90sGuitarMultiFx`, default branch `main`.
- 2026-10-04: Development practice follows the other Catastrophic Audio projects (ampsim, eq):
  AGPLv3 licence; JUCE and Catch2 as pinned submodules in `external/` (same commits as ampsim);
  only Release builds install into the plugin folders; static MSVC runtime on Windows; auval and
  pluginval locally and in CI; `CLAUDE.md`; this file; `M<n>:` commit messages.
- 2026-10-05: **Intended use: in front of an amp, as the original was used** (guitar → plugin →
  amp → cabinet). The plugin is the effects unit only; it contains no amp or cabinet and is not
  meant to be heard on its own. Consequently **no standalone**: plan §4, §20 and §21 (Milestone 0)
  amended. Effects are designed for an instrument-level guitar signal, with output suited to an
  amp's input (evidence register EV-107). First recorded the same day as "between amp and
  cabinet"; corrected by the owner before any effect was designed. That the original was used in
  front of an amp is the owner's statement, pending a catalogued source (EV-012).
- 2026-10-05: Product name **Five-A MultiFX Processor**, replacing the working title "Nineties
  Multi-FX". Every identifier follows it: bundle ID `com.catastrophicaudio.fivea`, saved-state
  tag `FiveAState`, CMake project `FiveAMultiFx`. Renamed before any release, so no saved session
  or preset uses the old ones. Plugin codes unchanged. The GitHub repository keeps its name.

- 2026-10-05 (Milestone 1 plan): DSP building blocks are **our own code**, so `fivea_engine` stays
  free of JUCE (filters, delay line and oversampler written from published sources and cited).
  Placeholder reverb is a **compact FDN** (Jot-style, Hadamard matrix). Drive oversampling is
  **off by default** (zero latency by default; 2x/4x selectable). The five effect enables stay
  **off by default**.

- 2026-10-05 (after reading SRC-001): the five effects' host parameters are **the documented set,
  stepped** (e.g. SENS 0–15, REV/DELAY MODE 1–7), matching the unit control for control; step
  mappings are placeholders until measured. **Noise reduction (NR LEVEL)** and **master volume
  (MASTER)** join Milestone 1; the volume pedal does not.

- 2026-10-05: **Visual design: close replica of the original panel** (option b): layout,
  proportions, colours, label styling and workflow, from the owner's manual drawing (SRC-001 p. 2)
  and a reference photo (SRC-003). The original's logo and model name are replaced by our own;
  no traced or photographic artwork. Plan §17.2 and `CLAUDE.md` amended.
- 2026-10-05: **No trademarked brand or model names** anywhere in the repository or the UI; the
  original is "the original unit". Code identifiers renamed (`FiveAProcessor`, namespace
  `fivea`, target `FiveAPlugin`, `FIVEA_*` options); the plan file renamed to
  `docs/implementation-plan.md`. Git history is left as it is.

- 2026-10-06 (Milestone 1 step 8): the 2-digit LED display is **drawn in code** (vector
  seven-segment shapes, no font); panel lettering uses **Barlow Condensed** (SIL Open Font
  License), embedded with its licence text.

- 2026-10-07 (Milestone 2 plan):
  - **Factory banks 2–6:** slot 2-1 is METAL 1 with the owner's manual's documented values
    (SRC-001 p. 6; its NR LEVEL and MASTER are not documented, so PLACEHOLDER). The other 24 slots
    are our own programs, named as development presets.
  - **Bank 1 starts with copies of five presets**, as the original's factory did (SRC-001 p. 8).
    Which five the original copied is not documented; ours are our choice.
  - **User programs persist in the session** (the plugin state saved with the project); each
    instance has its own bank 1. No shared file.
  - **Program changes dip the output:** fade out, apply the new program and reset the effects,
    fade in. Tails are cut. How the original sounds during a change is not documented
    (PLACEHOLDER, design choice).
- 2026-10-07 (after Milestone 2, from the owner's listening test): **drive oversampling 4× by
  default** (was off; saved sessions keep theirs), and **the drive's output level recalibrated**:
  at LEVEL 12 a drive program is about as loud as the dry guitar.
- 2026-10-07 (Milestone 2 step 4): **a new instance loads and plays program 1-1**, as the original
  does at power-on (a copy of METAL 1). A host's "reset to default" still sets the parameter
  defaults, every effect off.

## Session log

### 2026-10-04

- M0 implemented: CMake project, JUCE plugin (VST3, AU, Standalone), JUCE-free `FiveAProcessor`
  engine with input trim, output level and global bypass, versioned XML state, editor with five
  disabled effect sections, 22 Catch2 tests, architecture/evidence/source/parameter/validation
  documents.
- Found while testing: 0 dB reloads from saved state as 3.6×10⁻⁷ dB after JUCE's range
  normalisation, which broke bit-exact pass-through. Gains are now snapped to the 0.1 dB step.
- Published to GitHub. First CI run: macOS and Windows passed; Linux failed on a missing
  `libxi-dev`, then (found in an Ubuntu 24.04 container) on the engine library not being built
  position-independent. Both fixed.
- Aligned with ampsim/eq practice (see Decisions). auval and pluginval (strictness 10, VST3 and
  AU) pass locally.

### 2026-10-05

- Product renamed to Five-A MultiFX Processor (see Decisions), including bundle ID and state tag.
- Standalone removed (see Decisions). Placement first set as between amp and cabinet, then
  corrected to in front of an amp, as the original was used.
- Pushed. CI green on all seven jobs (`ec6afd2`), including auval and pluginval on macOS and
  pluginval on Windows: the first CI confirmation of the Linux fixes and the validators.
- Primary sources catalogued: SRC-001 the original unit's owner's manual and SRC-002 its
  service manual, both kept outside the repository. Parameter tables transcribed
  into `parameter-specification.md`; evidence register re-cited (EV-001–023). Front-of-amp use is
  now CONFIRMED. New findings that change Milestone 1: the documented parameter set differs from
  the planned engineering one; reverb/delay has 7 fixed modes with a 490 ms maximum delay;
  chorus/flanger has 5 modes including slapback; there is a noise reduction stage, a per-program
  master volume and a volume pedal input that the plan's chain does not list.
- M1 step 1: `ModelProfile.h` (documented values and the placeholder profile), `StepMapping`
  (documented step → algorithm value), `BypassCrossfade`, engine-side `DenormalGuard`, and an
  allocation-counting test harness. 41 tests; four of the new ones checked by breaking the code
  they guard (a documented delay, flush-to-zero, an allocation in `process()`, the crossfade).
- M1 step 2: 3 Band EQ (TRIM → low shelf 100 Hz → peaking at the MID FREQ table → high shelf
  3 kHz), RBJ cookbook designs in double precision, 20 ms ramps with coefficients redesigned every
  16 samples. 55 tests. Breaking the code exposed three weak tests, all fixed: the click test now
  uses the second difference with a bound set from measurement (0.017–0.026 smoothed vs 0.29–0.35
  unsmoothed), and a new test checks every design against the cookbook's analog prototypes, which
  catches Q and slope mistakes the self-consistent response test cannot. A planned "skip flat
  bands" special case was dropped: the designs are exact identities at 0 dB.
- M1 step 3: compressor (feed-forward peak, linked, gain computer in dB, smooth branching
  detector; Giannoulis et al. 2012). 65 tests; seven checked by breaking the code. One breakage
  (LEVEL without its ramp) first went uncaught because the test changed LEVEL at a sine zero
  crossing, where a gain jump leaves no trace; the compressor and EQ click tests now change at
  sine peaks, and their bounds were re-measured (EQ 0.049 vs 0.61 → bound 0.15; compressor
  0.0001 vs 0.096 → bound 0.005).
- M1 step 4: Distortion/Overdrive (per-mode pipeline, five interchangeable waveshapers, MODE
  crossfade) and an own FIR half-band oversampler (latency 69 at 2×, 76 at 4×). The first
  oversampler version had a half-sample latency, caught by the impulse test and fixed by keeping
  the decimated output aligned with the first of each sample pair. Click metric for the drive
  compares the transition with both settled states, since DRIVE legitimately sharpens the
  waveform. 85 tests; eight checked by breaking the code (a DC test was added first, since no
  test covered the DC blocker). CPU, stereo at 48 kHz: 0.05 % (1×), 0.9 % (2×), 1.4 % (4×) of one
  core.
- M1 step 5: chorus/flanger (five documented delays, sine LFO, cubic delay line, feedback,
  stereo phase). Measuring before setting bounds found two click sources in the MODE switch:
  clearing the delay line made the effect restart with a step one delay later, and feedback wrote
  the read-position jump back into the line. Fixed by keeping the line's history and ducking the
  feedback with the effect. Breaking the code showed the interpolation choice was only caught by
  accident; a deliberate 4 kHz accuracy test now separates cubic (0.28 %) from linear (3.8 %).
  100 tests.
- M1 step 6: reverb/delay (FDN reverb with five voicings, documented delay times, Echoverb, tail
  crossfade). Measured first: each voicing's decay within 3.5 % of its setting, L/R correlation
  0.05–0.09. Measuring also found two real faults before any test was written: a one-sample
  spike when a delay-time crossfade ended (the last sample read the old position), and clicks on
  MODE changes (the incoming engine was fed mid-signal from silence; its input now fades in). The
  50 Hz test signal could hide delay changes that are whole numbers of its period; 43 Hz cannot.
  Two test mistakes of mine were caught by their results (a reused output buffer fed back as
  input; an over-strict tail floor). 114 tests; breaking the code found the high-frequency
  damping untested, now covered. CPU 0.07–0.11 % of one core.
- M1 step 7: noise reduction (placeholder expander), the full chain with per-effect crossfades,
  latency compensation for oversampling, every documented control as a host parameter,
  oversampling applied live with the latency reported. The 32-combination test builds the
  documented order by hand from the blocks and compares bit for bit. Measuring switching found
  the memory blocks replaying a step when switched on (10 667, 2 193); their input now fades in
  (34, 66). Two test-side faults: copied test buffers still pointed at the original's samples
  (made every combination fail), and a latency check on a 100 Hz sine was fooled by the drive's
  phase shift. A macro with the old model name was caught and renamed. 133 tests; eight checked
  by breaking the code; engine tests clean under ASan and UBSan.

### 2026-10-06

- M1 step 8: the replica panel. Drawn entirely in code in one design space measured from the
  reference photo (1905 × 883), scaled to the window; Barlow Condensed embedded; the LED display
  drawn as seven-segment shapes. Manual/Edit-mode workflow: slide switch re-targets knobs A–E,
  display shows the value or stand-by, footswitches switch effects with LEDs, BYPASS blinks the
  mode LEDs, PEAK LED from the input. Program mode, banks and WRITE drawn but inactive
  (Milestone 2). Found while testing: the tests had been closing editors without telling the
  processor first (as a host does), which JUCE asserted on silently since Milestone 0 and which
  became a crash when reopening the editor; fixed in all editor tests. 143 tests; pluginval's
  editor tests pass at strictness 10.
- M1 step 9: docs brought in line (architecture rewritten for the Milestone 1 design, with every
  departure from the plan; parameter layers; validation plan; this file). Added a sample-rate and
  block-size re-prepare test and a CPU benchmark (0.31 / 1.17 / 1.51 % of one core for the full
  chain at 1× / 2× / 4×, worst block under 2 %). CI had failed on Windows since step 6 without my
  noticing: three test names held "×" or "–", which CTest's filter mangles on Windows; renamed,
  and CI now rejects non-ASCII test names. Golden renders moved to Milestones 4–5 (with
  placeholder algorithms they would only freeze placeholder sound).
- After the step 9 push: Windows Debug had not finished its tests since step 7 (runs cancelled
  after 13–22 minutes, then over an hour). Cause, found with libc++'s hardened (bounds-checking)
  mode on macOS: `Reverb::setVoicing` called `std::clamp` with an empty buffer's size as the upper
  bound when settings arrived before `prepare()`, which the plugin's own `prepareToPlay()` does.
  Undefined behaviour; harmless in practice elsewhere, but MSVC's Debug runtime stops on it with a
  dialog that nobody answers on CI. Fixed in `Reverb`, with a regression test. To make such
  failures visible: Windows test executables now print CRT errors instead of opening dialogs,
  every test has a 300 s CTest timeout, the slowest test was cut from 10 s to 2 s, and the Linux
  Debug CI job builds with libstdc++ assertions. Engine and plugin tests pass in the hardened
  build.
- The first push of that fix broke the Windows build: the new Windows-only test file looped over a
  braced list without including `<initializer_list>`, and no other platform compiles it. Replaced
  with a plain array.

### 2026-10-07: Milestone 2 step 1, the program model

- `core/Program` and `core/ProgramBank`, engine side: a program's contents, 30 slots with bank 1
  the only writable one, the selected program versus the bank shown while one is pending, and the
  three comparisons behind the display's dot. Programs copy without allocating (names are stored
  inline). The five blocks' settings structs gained `operator==`.
- 11 tests in `ProgramBankTests`; nine deliberate breakages (writing to a factory bank, the
  shown bank changing the selection, a field copied into the wrong place, the dot ignoring a
  change, and so on) were each caught.
- Evidence: EV-024–029 from the owner's manual pp. 4–9, EV-119 for what is not program data.

### 2026-10-07: Milestone 2 step 2, factory content

- `core/FactoryPrograms`: METAL 1 in 2-1 from the manual's values (now in the model profile as
  `documented::Metal1`), 24 development presets named "DEV …" across banks 2–6 (drive; clean and
  compressed; modulation; reverb and delay; combinations), and bank 1 as copies of 2-1, 3-1, 4-1,
  5-1, 6-1. Listed in `parameter-specification.md`, generated from the code.
- 7 tests: METAL 1 checked against the manual's numbers typed in separately; names and the "DEV"
  marking; bank 1's copies; every value within its documented range (`controlRange()`, new);
  Echoverb's TIME limit; every mode of every effect used; each of the 30 programs through the
  chain. Ten deliberate breakages each caught.
- Found while mutation testing: restoring a file from its backup kept the backup's older
  timestamp, so the build kept the broken objects. The source was right; the scripts now touch
  restored files, and the build was redone from scratch.

### 2026-10-07: Milestone 2 step 3, state schema 2

- State schema 2: `<Programs>` holds the selected program, the panel's mode and bank 1's five
  programs (stable parameter IDs, documented step numbers). Factory slots are never saved.
  Schema 1 states load as before, with the factory programs and 1-1. `PluginProcessor` owns a
  `ProgramState`; nothing uses it yet but the state.
- 9 tests in `ProgramStateTests`: round trip (and identical bytes), the saved format, migration
  from schema 1, unknown/missing/unreadable/out-of-range program values, unreadable and factory
  slots, a newer schema, a rejected document leaving programs alone, and every program control
  matching its host parameter's range. Ten deliberate breakages each caught.

### 2026-10-07: Milestone 2 step 4, programs in the plugin

- `PluginProcessor`: `selectProgram()` (loads a program into the parameters, discarding edits),
  `writeProgram()` (bank 1 only; the written slot becomes the selected program), the stored and
  edited program for the dot, and the 30 slots as host programs ("2-1 METAL 1"; bank 1 renamable).
  A new instance plays 1-1 (owner's decision).
- `core/ProgramTransition`: the program-change dip and a handshake with the message thread. The
  first version (linear fade on the output) measured click ratios up to 22 164 on the chorus,
  reverb and delay programs: a restarted delay line recorded the cut-in signal as a step. Fading
  the effects' input in instead brought that to 164, and a raised-cosine fade to 8.6; the bound is
  20.
- Found and fixed: `PluginProcessor::reset()` settled the engine on the last block's parameters,
  not the current ones, so a reset after a parameter change still ramped. New test.
- Milestone 1's pass-through tests now reset every parameter to its default first, since a new
  instance plays 1-1.
- 21 new tests (9 transition unit tests, 11 program-change tests, the reset test). Twelve
  deliberate breakages; one survived (the switch slipping to the next block) until a test of the
  gap's length was added.

### 2026-10-07: Milestone 2 step 5, the panel's Program mode and WRITE

- Footswitch 6 toggles the modes, with the mode LEDs. Program mode: the slide switch shows a bank,
  footswitches 1–5 pick a program in it, their LEDs show the playing program, the display shows
  the bank with the dot unless one is pending, knobs A–E are inactive. Edit mode as in
  Milestone 1, plus the dot. WRITE: flashing "1", footswitch picks the slot, WRITE stores (both
  mode LEDs for a second), footswitch 6 cancels; from either mode.
- The display repaints only on a change, now that the panel refreshes it on every tick.
- 11 new panel tests; Milestone 1's panel tests now switch to Edit mode first. Thirteen deliberate
  breakages; one survived (the playing bank not shown again after leaving Program mode with a
  bank pending) until that case was added.
- Evidence: EV-124 for what the manual leaves open on the panel.

### 2026-10-07: Milestone 2 sign-off

- End-to-end test: a program written on the panel survives saving and reloading the session
  (bank 1, selection, mode, and the panel showing them).
- Acceptance table above. Milestone 2 done; CI green on all platforms (`b20456b`).

### 2026-10-07: the owner's listening test, in front of an amp sim

- Heard: 2-2 good; most others "a crazy feedback tone". Measured: no program self-oscillates
  (every tail decays), but every Distortion-mode program aliased at −23 to −30 dB without
  oversampling (the sharp-cornered placeholder curve behind up to 55 dB of drive), while the
  Overdrive programs, 2-2 among them, were clean (below −79 dB). The drive programs were also 17 to
  25 dB louder than the guitar, METAL 1 peaking at 1.8.
- By decision: 4× oversampling by default, which brings the Distortion programs to −39 to −60 dB
  (exact, bin-aligned measurement; an earlier windowed estimate of −53 dB for METAL 1 was wrong).
  METAL 1 (2-1) remains the worst at −39 dB.
- The drive's output trim now follows DRIVE (a table per mode, measured), so LEVEL 12 is within
  ±0.2 dB of the dry guitar at a nominal input; every factory program now peaks below 0.7.
  Pre-gain and trim ramp in dB: ramped as linear gains they swelled mid-ramp, a click on a large
  DRIVE change that the existing test caught.
- New tests: drive loudness at every DRIVE step (two rates, with and without oversampling), the
  Distortion programs' aliasing at 4×, no factory program over full scale. `fillPluck` added to the
  test signals. Three deliberate breakages each caught.

