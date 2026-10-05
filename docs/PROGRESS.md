# Progress

Current status, decisions made, and a short log of each working session. The plan and milestone
definitions are in `docs/A5_implementation_plan.md` (§21); what is known about the original unit
versus assumed is in `docs/evidence-register.md`.

## Milestone status

| Milestone | Status | Notes |
|-----------|--------|-------|
| 0 Evidence and skeleton | Done (2026-10-05) | Pass-through plugin, versioned state, evidence register. CI green on macOS, Windows and Linux (`ec6afd2`). Outstanding: listen to it once in a DAW, in front of an amp sim. |
| 1 Complete functional chain | Not started | |
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

## Session log

### 2026-10-04

- M0 implemented: CMake project, JUCE plugin (VST3, AU, Standalone), JUCE-free `A5Processor`
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
