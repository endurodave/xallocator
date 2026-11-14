// xallocator PlatformIO example: replace malloc/free and new/delete with
// fixed block allocations. Runs on Arduino boards and the desktop (native).
//
// @see https://github.com/endurodave/xallocator

#include "xallocator.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#if defined(XALLOC_LOCK_FREERTOS) && defined(ARDUINO_ARCH_STM32)
// XALLOC_LOCK_FREERTOS: xallocator guards its data with a FreeRTOS critical
// section, so tasks can share it (see platformio.ini, env:disco_f407vg)
#include <STM32FreeRTOS.h>
#define USE_FREERTOS_TASKS
#endif

static void Log(const char* msg)
{
#ifdef ARDUINO
    Serial.println(msg);
#else
    printf("%s\n", msg);
#endif
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

#ifdef USE_FREERTOS_TASKS
static volatile bool g_taskOk[2] = { true, true };
static volatile int g_taskDone = 0;

// Two of these run at once, sharing xallocator's blocks
static void StressTask(void* arg)
{
    int id = (int)(intptr_t)arg;
    for (int round = 0; round < 50; round++)
    {
        if (!Stress((uint8_t)(0xA0 + id), 10))
            g_taskOk[id] = false;
        vTaskDelay(1);
    }
    taskENTER_CRITICAL();
    g_taskDone = g_taskDone + 1;
    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}

static void MainTask(void*)
{
    bool ok = Demo();
    xTaskCreate(StressTask, "Stress0", 512, (void*)0, 1, NULL);
    xTaskCreate(StressTask, "Stress1", 512, (void*)1, 1, NULL);
    while (g_taskDone < 2)
        vTaskDelay(10);
    bool tasksOk = g_taskOk[0] && g_taskOk[1];
    Log(tasksOk ? "Two task stress test: passed" : "Two task stress test: FAILED");
    Log(ok && tasksOk ? "xallocator demo PASSED" : "xallocator demo FAILED");
    vTaskDelete(NULL);
}
#endif

#ifdef ARDUINO
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Log("xallocator demo");
#ifdef USE_FREERTOS_TASKS
    xTaskCreate(MainTask, "Main", 1024, NULL, 2, NULL);
    vTaskStartScheduler();
#else
    bool ok = Demo() && Stress(0x5A, 100);
    Log(ok ? "xallocator demo PASSED" : "xallocator demo FAILED");
#endif
}

void loop()
{
}
#else
int main()
{
    Log("xallocator demo");
    bool ok = Demo() && Stress(0x5A, 100);
    xalloc_stats();
    Log(ok ? "xallocator demo PASSED" : "xallocator demo FAILED");
    return ok ? 0 : 1;
}
#endif
