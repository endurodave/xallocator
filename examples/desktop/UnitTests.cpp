// Unit tests for Allocator and xallocator. Checks run in every build type (not assert()), and
// failures are counted so every test runs. main() exits non-zero on failure.

#include "Allocator.h"
#include "xallocator.h"
#include <iostream>
#include <cstddef>
#include <cstdint>
#include <new>

static int s_failures = 0;

#define CHECK(cond) \
    do { if (!(cond)) { std::cout << "  FAILED: " #cond " (" __FILE__ ":" << __LINE__ << ")" << std::endl; s_failures++; } } while (0)

static bool IsMaxAligned(const void* p)
{
    return reinterpret_cast<uintptr_t>(p) % alignof(std::max_align_t) == 0;
}

// A type that needs the strictest fundamental alignment
struct MaxAligned
{
    std::max_align_t value;
    char c;
};

// A DECLARE_ALLOCATOR class with a pool of 2
class PoolClass
{
    DECLARE_ALLOCATOR
public:
    int value = 0;
};
IMPLEMENT_ALLOCATOR(PoolClass, 2, NULL)

// AllocatorPool memory is aligned for any type, so pooled objects are aligned
static void Pool_BlocksAreAligned()
{
    AllocatorPool<MaxAligned, 4> pool;
    void* blocks[4];
    for (int i = 0; i < 4; i++)
    {
        blocks[i] = pool.Allocate(sizeof(MaxAligned));
        CHECK(blocks[i] != NULL);
        CHECK(IsMaxAligned(blocks[i]));
    }
    for (int i = 0; i < 4; i++)
        pool.Deallocate(blocks[i]);
}

// An exhausted pool with no new-handler returns NULL and counts nothing
static void Pool_ExhaustedReturnsNull()
{
    std::new_handler saved = std::set_new_handler(nullptr);

    Allocator allocator(32, 2);
    void* p1 = allocator.Allocate(32);
    void* p2 = allocator.Allocate(32);
    CHECK(p1 != NULL && p2 != NULL);

    void* p3 = allocator.Allocate(32);
    CHECK(p3 == NULL);
    CHECK(allocator.GetBlocksInUse() == 2);
    CHECK(allocator.GetAllocations() == 2);

    allocator.Deallocate(p1);
    allocator.Deallocate(p2);
    CHECK(allocator.GetBlocksInUse() == 0);

    std::set_new_handler(saved);
}

// When the pool is exhausted, the new-handler is called and the allocation
// retried, so a handler that frees a block makes the allocation succeed
static Allocator* s_handlerAllocator = NULL;
static void* s_handlerBlock = NULL;
static int s_handlerCalls = 0;
static void FreeOneBlock()
{
    s_handlerCalls++;
    s_handlerAllocator->Deallocate(s_handlerBlock);
    std::set_new_handler(nullptr);
}

static void Pool_NewHandlerFreesBlockAndRetries()
{
    std::new_handler saved = std::set_new_handler(FreeOneBlock);

    Allocator allocator(32, 1);
    void* p1 = allocator.Allocate(32);
    CHECK(p1 != NULL);
    s_handlerAllocator = &allocator;
    s_handlerBlock = p1;
    s_handlerCalls = 0;

    void* p2 = allocator.Allocate(32);
    CHECK(s_handlerCalls == 1);
    CHECK(p2 == p1);    // the block the handler freed
    CHECK(allocator.GetBlocksInUse() == 1);

    allocator.Deallocate(p2);
    std::set_new_handler(saved);
}

// Deallocate(NULL) is ignored: delete of a null pointer may call it
static void Deallocate_NullIgnored()
{
    Allocator allocator(32, 2);
    allocator.Deallocate(NULL);
    CHECK(allocator.GetBlocksInUse() == 0);
    CHECK(allocator.GetDeallocations() == 0);

    // Still works afterward
    void* p = allocator.Allocate(32);
    CHECK(p != NULL);
    allocator.Deallocate(p);

    PoolClass::operator delete(nullptr);
}

// DECLARE_ALLOCATOR's operator new throws std::bad_alloc when the pool is
// exhausted, instead of returning NULL
static void DeclareAllocator_ExhaustedThrows()
{
    std::new_handler saved = std::set_new_handler(nullptr);

    PoolClass* a = new PoolClass();
    PoolClass* b = new PoolClass();
    bool threw = false;
    try
    {
        PoolClass* c = new PoolClass();
        delete c;
    }
    catch (const std::bad_alloc&)
    {
        threw = true;
    }
    CHECK(threw);
    delete a;
    delete b;

    // Space again after delete
    PoolClass* d = new PoolClass();
    CHECK(d != NULL);
    delete d;

    std::set_new_handler(saved);
}

// Heap blocks mode: blocks come from the heap and are reused after Deallocate
static void HeapBlocks_Reuse()
{
    Allocator allocator(64);
    void* p1 = allocator.Allocate(64);
    CHECK(p1 != NULL);
    CHECK(IsMaxAligned(p1));
    allocator.Deallocate(p1);
    void* p2 = allocator.Allocate(64);
    CHECK(p2 == p1);
    CHECK(allocator.GetBlockCount() == 1);
    CHECK(allocator.GetAllocations() == 2);
    allocator.Deallocate(p2);
}

// Static pool mode uses the caller's memory
static void StaticPool_UsesCallerMemory()
{
    alignas(std::max_align_t) static char memory[32 * 4];
    Allocator allocator(32, 4, memory);
    for (int i = 0; i < 4; i++)
    {
        char* p = static_cast<char*>(allocator.Allocate(32));
        CHECK(p >= memory && p < memory + sizeof(memory));
    }
}

//------------------------------------------------------------------------------
// xallocator
//------------------------------------------------------------------------------

// A class allocated with xmalloc through XALLOCATOR
class XClass
{
    XALLOCATOR
public:
    MaxAligned data;
};

// Every xmalloc pointer keeps the strictest fundamental alignment (16 bytes
// on 64-bit Linux): the block header is sized to preserve it
static void Xmalloc_Aligned()
{
    const size_t sizes[] = { 1, 7, 8, 15, 16, 24, 40, 100, 200, 1000, 3000 };
    for (size_t size : sizes)
    {
        void* p = xmalloc(size);
        CHECK(p != NULL);
        CHECK(IsMaxAligned(p));
        xfree(p);
    }

    XClass* x = new XClass();
    CHECK(IsMaxAligned(x));
    delete x;

    XClass* arr = new XClass[3];
    CHECK(IsMaxAligned(arr));
    delete[] arr;
}

// xmalloc, xrealloc and xfree keep data and handle NULL / size 0
static void Xmalloc_ReallocAndFree()
{
    char* p = static_cast<char*>(xmalloc(10));
    CHECK(p != NULL);
    for (int i = 0; i < 10; i++)
        p[i] = static_cast<char>('a' + i);

    char* q = static_cast<char*>(xrealloc(p, 500));
    CHECK(q != NULL);
    bool same = true;
    for (int i = 0; i < 10; i++)
        same = same && q[i] == static_cast<char>('a' + i);
    CHECK(same);

    CHECK(xrealloc(q, 0) == NULL);  // frees q
    void* r = xrealloc(NULL, 50);   // like xmalloc
    CHECK(r != NULL);
    xfree(r);
    xfree(NULL);
}

// A request that can't be satisfied returns NULL. A size near SIZE_MAX used to
// wrap the power-of-two rounding to 0 and return a tiny block; a heap failure
// used to write the block header through a NULL block.
static void Xmalloc_FailureReturnsNull()
{
    std::new_handler saved = std::set_new_handler(nullptr);
    const size_t sizes[] = {
        static_cast<size_t>(-1),
        static_cast<size_t>(-1) / 2,                // rounds past SIZE_MAX
        static_cast<size_t>(-1) / 2 - 64,           // too big for the heap
    };
    for (size_t size : sizes)
        CHECK(xmalloc(size) == NULL);
    std::set_new_handler(saved);
}

// Freed blocks are reused and counted per block size
static void Xmalloc_BlocksReusedAndCounted()
{
    Allocator* allocator = xallocator_get_allocator(100);
    CHECK(allocator != NULL);
    uint32_t inUse = allocator->GetBlocksInUse();

    void* p1 = xmalloc(100);
    CHECK(allocator->GetBlocksInUse() == inUse + 1);
    xfree(p1);
    CHECK(allocator->GetBlocksInUse() == inUse);

    void* p2 = xmalloc(90);     // same block size
    CHECK(p2 == p1);
    xfree(p2);
}

// When every allocator slot is taken, a new block size returns NULL instead
// of creating an allocator that is never tracked or freed
static void Xmalloc_AllocatorTableFull()
{
    std::new_handler saved = std::set_new_handler(nullptr);
    bool sawNull = false;
    for (int i = 0; i < 24 && !sawNull; i++)
    {
        void* p = xmalloc(static_cast<size_t>(16) << i);
        if (p == NULL)
            sawNull = true;
        else
            xfree(p);
    }
    CHECK(sawNull);

    // Block sizes already in the table still work
    void* p = xmalloc(100);
    CHECK(p != NULL);
    xfree(p);
    std::set_new_handler(saved);
}

int RunUnitTests()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "Pool_BlocksAreAligned",                  Pool_BlocksAreAligned },
        { "Pool_ExhaustedReturnsNull",              Pool_ExhaustedReturnsNull },
        { "Pool_NewHandlerFreesBlockAndRetries",    Pool_NewHandlerFreesBlockAndRetries },
        { "Deallocate_NullIgnored",                 Deallocate_NullIgnored },
        { "DeclareAllocator_ExhaustedThrows",       DeclareAllocator_ExhaustedThrows },
        { "HeapBlocks_Reuse",                       HeapBlocks_Reuse },
        { "StaticPool_UsesCallerMemory",            StaticPool_UsesCallerMemory },
        { "Xmalloc_Aligned",                        Xmalloc_Aligned },
        { "Xmalloc_ReallocAndFree",                 Xmalloc_ReallocAndFree },
        { "Xmalloc_FailureReturnsNull",             Xmalloc_FailureReturnsNull },
        { "Xmalloc_BlocksReusedAndCounted",         Xmalloc_BlocksReusedAndCounted },
        { "Xmalloc_AllocatorTableFull",             Xmalloc_AllocatorTableFull },
    };

    std::cout << "-- Unit tests --" << std::endl;
    s_failures = 0;
    for (const auto& test : tests)
    {
        int before = s_failures;
        test.fn();
        std::cout << (s_failures == before ? "PASS " : "FAIL ") << test.name << std::endl;
    }
    std::cout << (s_failures == 0 ? "All unit tests passed." : "Unit tests FAILED.") << std::endl;
    return s_failures;
}
