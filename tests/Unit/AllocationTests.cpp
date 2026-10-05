#include "AllocationGuard.h"

#include "core/FiveAProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using fivea::test::ScopedAllocationCounter;

TEST_CASE("The allocation counter sees allocations (self-test)")
{
    std::size_t counted = 0;
    {
        const ScopedAllocationCounter counter;
        auto heap = std::make_unique<int>(1);
        std::vector<float> grown;
        grown.push_back(1.0f);
        ScopedAllocationCounter::keepAlive(heap.get());
        ScopedAllocationCounter::keepAlive(grown.data());
        counted = counter.count();
    }
    CHECK(counted >= 2);

    std::size_t none = 1;
    {
        const ScopedAllocationCounter counter;
        int onStack = 0;
        (void)onStack;
        none = counter.count();
    }
    CHECK(none == 0);
}

TEST_CASE("FiveAProcessor does not allocate while setting parameters and processing")
{
    constexpr int numChannels = 2;
    constexpr int blockSize = 256;

    fivea::FiveAProcessor processor;
    processor.prepare({48000.0, blockSize, numChannels});

    std::vector<float> left(blockSize, 0.25f);
    std::vector<float> right(blockSize, -0.25f);
    float* channels[] = {left.data(), right.data()};

    std::size_t allocations = 0;
    {
        const ScopedAllocationCounter counter;
        for (int block = 0; block < 200; ++block)
        {
            fivea::ParameterSnapshot parameters;
            parameters.inputTrimDb = static_cast<float>(block % 24) - 12.0f;
            parameters.outputLevelDb = static_cast<float>(block % 7);
            parameters.globalBypass = (block / 50) % 2 == 1;
            parameters.effectEnabled[static_cast<std::size_t>(block % 5)] = true;

            processor.setParameters(parameters);
            processor.process({channels, numChannels, blockSize});
        }
        processor.reset();
        allocations = counter.count();
    }

    CHECK(allocations == 0);
}
