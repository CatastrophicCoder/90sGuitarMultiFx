#pragma once

#include "dsp/Compressor.h"
#include "dsp/Drive.h"
#include "dsp/Modulation.h"
#include "dsp/ThreeBandEq.h"
#include "dsp/TimeEffects.h"

#include <array>
#include <cstddef>

namespace fivea
{

// The five major blocks in the documented serial order (docs/evidence-register.md, EV-001).
enum class EffectBlock : std::size_t
{
    Compressor,
    Drive,
    Equaliser,
    Modulation,
    TimeEffect
};

inline constexpr std::size_t numEffectBlocks = 5;

// Every control, as plain values: the documented steps of the original unit's controls (SRC-001
// pp. 7, 10–12) and the plugin's own gains. Copied by value into the audio thread once per block;
// holding no pointers or strings keeps the copy allocation-free.
struct ParameterSnapshot
{
    float inputTrimDb = 0.0f;
    float outputLevelDb = 0.0f;
    bool globalBypass = false;
    std::array<bool, numEffectBlocks> effectEnabled{};

    dsp::Compressor::Settings compressor;
    dsp::Drive::Settings drive;
    dsp::ThreeBandEq::Settings equaliser;
    dsp::Modulation::Settings modulation;
    dsp::TimeEffects::Settings timeEffects;
    int noiseReductionLevel = 0; // Utility NR LEVEL: 0 is off
    int master = 12;             // Utility MASTER: 12 is unity (placeholder mapping)

    [[nodiscard]] bool isEnabled(EffectBlock block) const noexcept
    {
        return effectEnabled[static_cast<std::size_t>(block)];
    }
};

} // namespace fivea
