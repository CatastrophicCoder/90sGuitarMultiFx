#include "AllocationGuard.h"

#include <cstdlib>
#include <new>

// Replacing the global allocation functions is allowed once per program ([replacement.functions]).
// The array and nothrow forms default to calling these, so they are counted too.

namespace
{
thread_local bool counting = false;
thread_local std::size_t allocationCount = 0;

void* allocate(std::size_t size)
{
    if (counting)
        ++allocationCount;

    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;

    throw std::bad_alloc{};
}

void* allocateAligned(std::size_t size, std::align_val_t alignment)
{
    if (counting)
        ++allocationCount;

    const auto align = static_cast<std::size_t>(alignment);
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, align))
        return memory;
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, align < sizeof(void*) ? sizeof(void*) : align, size == 0 ? 1 : size) == 0)
        return memory;
#endif
    throw std::bad_alloc{};
}

void freeAligned(void* memory) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}
} // namespace

void* operator new(std::size_t size)
{
    return allocate(size);
}

void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocateAligned(size, alignment);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::align_val_t) noexcept
{
    freeAligned(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept
{
    freeAligned(memory);
}

namespace fivea::test
{

ScopedAllocationCounter::ScopedAllocationCounter() noexcept : startCount(allocationCount), wasCounting(counting)
{
    counting = true;
}

ScopedAllocationCounter::~ScopedAllocationCounter()
{
    counting = wasCounting;
}

std::size_t ScopedAllocationCounter::count() const noexcept
{
    return allocationCount - startCount;
}

namespace
{
const void* volatile observedPointer = nullptr;
}

void ScopedAllocationCounter::keepAlive(const void* pointer) noexcept
{
    observedPointer = pointer;
}

} // namespace fivea::test
