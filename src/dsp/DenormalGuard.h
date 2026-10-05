#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <immintrin.h>
#define FIVEA_DENORMAL_GUARD_SSE 1
#elif defined(__aarch64__)
#define FIVEA_DENORMAL_GUARD_AARCH64 1
#endif

namespace fivea::dsp
{

// Sets flush-to-zero (and denormals-are-zero on x86) for its lifetime and restores the previous
// mode afterwards. Feedback paths (filters, delays, reverb) decaying towards silence otherwise
// produce denormal numbers, which are many times slower to compute. The engine has its own guard
// because it must not rely on the plugin wrapper's JUCE one.
class DenormalGuard
{
public:
    DenormalGuard() noexcept
    {
#if defined(FIVEA_DENORMAL_GUARD_SSE)
        previous = _mm_getcsr();
        _mm_setcsr(previous | flushToZeroSse | denormalsAreZeroSse);
#elif defined(FIVEA_DENORMAL_GUARD_AARCH64)
        previous = readFpcr();
        writeFpcr(previous | flushToZeroArm);
#endif
    }

    ~DenormalGuard()
    {
#if defined(FIVEA_DENORMAL_GUARD_SSE)
        _mm_setcsr(previous);
#elif defined(FIVEA_DENORMAL_GUARD_AARCH64)
        writeFpcr(previous);
#endif
    }

    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;

    [[nodiscard]] static constexpr bool isSupported() noexcept
    {
#if defined(FIVEA_DENORMAL_GUARD_SSE) || defined(FIVEA_DENORMAL_GUARD_AARCH64)
        return true;
#else
        return false;
#endif
    }

private:
#if defined(FIVEA_DENORMAL_GUARD_SSE)
    static constexpr unsigned int flushToZeroSse = 0x8000;      // MXCSR FTZ
    static constexpr unsigned int denormalsAreZeroSse = 0x0040; // MXCSR DAZ
    unsigned int previous = 0;
#elif defined(FIVEA_DENORMAL_GUARD_AARCH64)
    static constexpr std::uint64_t flushToZeroArm = std::uint64_t{1} << 24; // FPCR.FZ

    static std::uint64_t readFpcr() noexcept
    {
        std::uint64_t value = 0;
        asm volatile("mrs %0, fpcr" : "=r"(value));
        return value;
    }

    static void writeFpcr(std::uint64_t value) noexcept { asm volatile("msr fpcr, %0" : : "r"(value)); }

    std::uint64_t previous = 0;
#endif
};

} // namespace fivea::dsp
