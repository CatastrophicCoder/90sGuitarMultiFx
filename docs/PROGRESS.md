# Progress

Current status, decisions made, and a short log of each working session. The plan and milestone
definitions are in `docs/implementation-plan.md` (§21); what is known about the original unit
versus assumed is in `docs/evidence-register.md`.

## Milestone status

| Milestone | Status | Notes |
|-----------|--------|-------|
| 0 Evidence and skeleton | Done (2026-10-05) | Pass-through plugin, versioned state, evidence register. CI green on macOS, Windows and Linux (`ec6afd2`). Outstanding: listen to it once in a DAW, in front of an amp sim. |
| 1 Complete functional chain | In progress | Steps 1–3 of 9 done (2026-10-05): infrastructure; 3 Band EQ; compressor. |
| 2 Programs and workflow | Not started | |
| 3 Hardware-rate mode | Not started | |
| 4 Measurement tooling | Not started | |
| 5 Measurement-driven calibration | Not started | Needs access to a physical unit. |
| 6 Release hardening | Not started | Packaging (`.pkg`/`.dmg`) and `CHANGELOG.md` arrive here, as in the other Catastrophic Audio projects. |

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
