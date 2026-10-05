#include "dsp/DenormalGuard.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace
{
// volatile so the compiler cannot fold the multiplication at compile time.
float multiplyAtRuntime(float a, float b)
{
    volatile float x = a;
    volatile float y = b;
    return x * y;
}
} // namespace

TEST_CASE("DenormalGuard flushes denormal results to zero, and restores the mode afterwards")
{
    if (!fivea::dsp::DenormalGuard::isSupported())
        SKIP("No flush-to-zero control on this platform");

    const float tiny = std::numeric_limits<float>::min() / 4.0f; // a denormal
    REQUIRE(multiplyAtRuntime(tiny, 1.0f) != 0.0f);

    {
        const fivea::dsp::DenormalGuard guard;
        CHECK(multiplyAtRuntime(tiny, 1.0f) == 0.0f);
        CHECK(multiplyAtRuntime(std::numeric_limits<float>::min(), 0.5f) == 0.0f);
    }

    CHECK(multiplyAtRuntime(tiny, 1.0f) != 0.0f);
}
