#pragma once

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

// Plain values in internal units, copied by value into the audio thread once per block. Holding no
// pointers or strings keeps the copy allocation-free.
struct ParameterSnapshot
{
    float inputTrimDb = 0.0f;
    float outputLevelDb = 0.0f;
    bool globalBypass = false;
    std::array<bool, numEffectBlocks> effectEnabled{};

    [[nodiscard]] bool isEnabled(EffectBlock block) const noexcept
    {
        return effectEnabled[static_cast<std::size_t>(block)];
    }
};

} // namespace fivea
