// xallocator demo: replace malloc/free and new/delete with fixed block
// allocations. Results are printed on Serial at 115200. Needs a board with a
// full C++ standard library (not 8-bit AVR).
//
// xallocator locks with std::mutex where the core has it (ESP32) and uses no
// lock on single-threaded boards. The RTOS lock options are build flags (see
// the PlatformIO example).
//
// @see https://github.com/endurodave/xallocator

#include <xallocator.h>
#include <cstring>
#include <cstdint>

static void Log(const char* msg)
{
    Serial.println(msg);
}

// XALLOCATOR routes this class's new/delete to xmalloc/xfree
class Point
{
    XALLOCATOR
public:
    Point(int x, int y) : x(x), y(y) {}
    int x, y;
};

// Allocate, fill, check and free blocks of many sizes. Returns false if a
// block was corrupted or an allocation failed.
static bool Stress(uint8_t pattern, int loops)
{
    const int COUNT = 16;
    void* blocks[COUNT];
    for (int loop = 0; loop < loops; loop++)
    {
        for (int i = 0; i < COUNT; i++)
        {
            size_t size = 1 + (size_t)((loop * 7 + i * 13) % 200);
            blocks[i] = xmalloc(size);
            if (blocks[i] == NULL)
                return false;
            memset(blocks[i], pattern, size);
        }
        for (int i = 0; i < COUNT; i++)
        {
            size_t size = 1 + (size_t)((loop * 7 + i * 13) % 200);
            const uint8_t* p = static_cast<const uint8_t*>(blocks[i]);
            for (size_t b = 0; b < size; b++)
                if (p[b] != pattern)
                    return false;
            xfree(blocks[i]);
        }
    }
    return true;
}

static bool Demo()
{
    char buf[80];

    // xmalloc/xrealloc/xfree work like malloc/realloc/free
    char* str = static_cast<char*>(xmalloc(16));
    if (str == NULL)
        return false;
    strcpy(str, "xallocator");
    str = static_cast<char*>(xrealloc(str, 64));
    if (str == NULL)
        return false;
    strcat(str, " fixed blocks");
    snprintf(buf, sizeof(buf), "xrealloc: \"%s\"", str);
    Log(buf);
    bool ok = strcmp(str, "xallocator fixed blocks") == 0;
    xfree(str);

    // new/delete on an XALLOCATOR class
    Point* pt = new Point(3, 4);
    snprintf(buf, sizeof(buf), "new Point: (%d, %d)", pt->x, pt->y);
    Log(buf);
    ok = ok && pt->x == 3 && pt->y == 4;
    delete pt;

    // A freed block is kept for reuse, not returned to the heap
    void* a = xmalloc(40);
    xfree(a);
    void* b = xmalloc(40);
    snprintf(buf, sizeof(buf), "Block reused: %s", a == b ? "yes" : "no");
    Log(buf);
    ok = ok && a == b;
    xfree(b);

    return ok;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Log("xallocator demo");
    bool ok = Demo() && Stress(0x5A, 100);
    Log(ok ? "xallocator demo PASSED" : "xallocator demo FAILED");
}

void loop()
{
}
