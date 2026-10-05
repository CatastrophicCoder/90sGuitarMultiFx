#pragma once

#include <cstddef>

namespace fivea::test
{

// Counts heap allocations made on this thread while an instance is alive. The global operator new
// is replaced for the whole test executable (AllocationGuard.cpp), so this catches allocations
// anywhere below the code under test, including in the standard library.
//
// Keep Catch2 assertions outside the counting scope: they allocate.
class ScopedAllocationCounter
{
public:
    ScopedAllocationCounter() noexcept;
    ~ScopedAllocationCounter();

    ScopedAllocationCounter(const ScopedAllocationCounter&) = delete;
    ScopedAllocationCounter& operator=(const ScopedAllocationCounter&) = delete;

    [[nodiscard]] std::size_t count() const noexcept;

    // Makes a pointer observable, so the optimiser cannot remove the allocation that produced it
    // (C++ allows a matching new/delete pair to be elided entirely).
    static void keepAlive(const void* pointer) noexcept;

private:
    std::size_t startCount;
    bool wasCounting;
};

} // namespace fivea::test
