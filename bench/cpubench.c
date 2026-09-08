/*
 * cpubench.c -- host-CPU proxy benchmark, for scaling the C trap-cost figure
 * from a laptop to the phone.
 *
 * The real trap benchmark (bench.c) needs unicorn built for Android, which
 * needs the NDK.  This one needs nothing: it is a static binary that runs
 * anywhere, and it measures the two kinds of work a trap actually does --
 * a tight integer loop, and a call-heavy loop with small memory moves.
 * Running it on both machines turns "assume the phone is 2-5x slower" into
 * a measured ratio.
 *
 *   zig cc -target aarch64-linux-musl -static -O2 cpubench.c -o cpubench-arm64
 *   adb push cpubench-arm64 /data/local/tmp/ && adb shell chmod +x ...
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
static double now(void) {
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#else
static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
#endif

/* ---- A: tight integer loop, the shape of emulated game logic ---------- */
static uint16_t crc16(const uint8_t *p, size_t n)
{
    uint16_t c = 0xFFFF;
    size_t i; int b;
    for (i = 0; i < n; i++) {
        c ^= (uint16_t)p[i] << 8;
        for (b = 0; b < 8; b++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

/* ---- B: call + small-copy loop, the shape of a trap shim -------------- */
typedef struct { uint32_t r[16]; uint8_t mem[8192]; } ctx_t;
static ctx_t g_ctx;

__attribute__((noinline))
static uint32_t shim_like(ctx_t *c, uint32_t dst, uint32_t src, uint32_t n)
{
    /* read three "registers", move a few bytes, write one back --
       deliberately the same shape as the memcpy shim in the trap benchmark */
    uint32_t a = c->r[0], b = c->r[1], d = c->r[2];
    (void)a; (void)b; (void)d;
    if (n && dst + n < sizeof c->mem && src + n < sizeof c->mem)
        memmove(c->mem + dst, c->mem + src, n);
    c->r[0] = dst;
    return dst;
}

int main(void)
{
    static uint8_t buf[65536];
    double t0, dt;
    size_t i;
    volatile uint16_t sink = 0;
    volatile uint32_t sink2 = 0;
    const int ITER_A = 200, ITER_B = 20000000;

    for (i = 0; i < sizeof buf; i++) buf[i] = (uint8_t)(i * 7 + 3);
    for (i = 0; i < sizeof g_ctx.mem; i++) g_ctx.mem[i] = (uint8_t)i;

    printf("=== CPU proxy benchmark ===\n");

    t0 = now();
    for (i = 0; i < (size_t)ITER_A; i++) sink ^= crc16(buf, sizeof buf);
    dt = now() - t0;
    {
        double bytes = (double)ITER_A * (double)sizeof buf;
        printf("A  integer loop : %7.3f s for %.0f KB  -> %8.1f MB/s\n",
               dt, bytes / 1024.0, bytes / dt / 1e6);
    }

    t0 = now();
    for (i = 0; i < (size_t)ITER_B; i++)
        sink2 ^= shim_like(&g_ctx, (uint32_t)(i & 1023), 2048, 16);
    dt = now() - t0;
    printf("B  shim-like call: %7.3f s for %d calls -> %8.1f ns/call\n",
           dt, ITER_B, dt / ITER_B * 1e9);

    printf("(sink %u %u)\n", (unsigned)sink, (unsigned)sink2);
    return 0;
}
