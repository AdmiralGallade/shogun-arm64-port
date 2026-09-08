/*
 * bench.c -- trap round-trip cost for the Shogun ARM32 runtime, in C.
 *
 * A faithful reproduction of guest.py's trap mechanism with the Python
 * removed, so the two sets of numbers are directly comparable on the same
 * machine.  This isolates one variable: host language.
 *
 *   zig cc bench.c -I<unicorn>/include -L<unicorn>/lib -lunicorn -o bench.exe
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unicorn/unicorn.h>

#ifdef _WIN32
#include <windows.h>
static double now(void) {
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}
#else
#include <time.h>
static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
#endif

#define CODE_BASE 0x50000000u
#define TRAP_BASE 0x7F000000u
#define HEAP_BASE 0x40000000u
#define STACK_TOP 0x70000000u
#define STACK_SZ  (8u * 1024u * 1024u)
#define RET_MAGIC 0x7FFF0000u

/* the same loops the Python benchmark used, assembled by keystone */
static const unsigned char CODE_THUMB[30] = {
0xf0,0xb5,0x04,0x46,0x0d,0x46,0x16,0x46,0x40,0xf2,0x00,0x07,0xc7,0xf6,0x00,0x77,
0x28,0x46,0x31,0x46,0x10,0x22,0xb8,0x47,0x64,0x1e,0xf9,0xd1,0xf0,0xbd};
static const unsigned char CODE_ARM[52] = {
0xf0,0x40,0x2d,0xe9,0x00,0x40,0xa0,0xe1,0x01,0x50,0xa0,0xe1,0x02,0x60,0xa0,0xe1,
0x00,0x70,0x00,0xe3,0x00,0x7f,0x47,0xe3,0x05,0x00,0xa0,0xe1,0x06,0x10,0xa0,0xe1,
0x10,0x20,0xb0,0xe3,0x37,0xff,0x2f,0xe1,0x01,0x40,0x54,0xe2,0xf9,0xff,0xff,0x1a,
0xf0,0x80,0xbd,0xe8};

static uc_engine *uc;
static int   g_force_slow = 0;   /* always take the stop/restart path */
static int   g_null_shim  = 0;   /* skip the shim body, measure dispatch only */
static long  g_traps      = 0;
static int   g_pending    = 0;

/* Same work as the Python memcpy shim: read three args, move guest bytes,
   set the return value. */
static void shim_body(void)
{
    uint32_t r0, r1, r2;
    static uint8_t buf[4096];
    g_traps++;
    if (g_null_shim) return;
    uc_reg_read(uc, UC_ARM_REG_R0, &r0);
    uc_reg_read(uc, UC_ARM_REG_R1, &r1);
    uc_reg_read(uc, UC_ARM_REG_R2, &r2);
    if (r2 && r2 <= sizeof buf) {
        uc_mem_read(uc, r1, buf, r2);
        uc_mem_write(uc, r0, buf, r2);
    }
    uc_reg_write(uc, UC_ARM_REG_R0, &r0);
}

static void hook_trap(uc_engine *u, uint64_t addr, uint32_t size, void *ud)
{
    uint32_t lr, cpsr, pc;
    (void)addr; (void)size; (void)ud;
    uc_reg_read(u, UC_ARM_REG_LR, &lr);
    uc_reg_read(u, UC_ARM_REG_CPSR, &cpsr);
    if (!g_force_slow && ((lr & 1u) == ((cpsr >> 5) & 1u))) {
        shim_body();                       /* fast path: stay inside the CPU */
        pc = lr & ~1u;
        uc_reg_write(u, UC_ARM_REG_PC, &pc);
        return;
    }
    g_pending = 1;                         /* slow path: leave and be resumed */
    uc_emu_stop(u);
}

static int setup(void)
{
    uc_hook h;
    if (uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc) != UC_ERR_OK) return 0;
    uc_mem_map(uc, CODE_BASE, 0x1000, UC_PROT_ALL);
    uc_mem_map(uc, TRAP_BASE, 0x1000, UC_PROT_ALL);
    uc_mem_map(uc, HEAP_BASE, 0x100000, UC_PROT_READ | UC_PROT_WRITE);
    uc_mem_map(uc, STACK_TOP - STACK_SZ, STACK_SZ, UC_PROT_READ | UC_PROT_WRITE);
    uc_mem_map(uc, RET_MAGIC & ~0xFFFu, 0x1000, UC_PROT_ALL);
    uc_hook_add(uc, &h, UC_HOOK_CODE, (void *)hook_trap, NULL,
                TRAP_BASE, TRAP_BASE + 0xFFF);
    {   /* enable VFP: cores come out of reset with the FPU off */
        uint32_t c1 = 0, fpexc = 0x40000000u;
        uc_reg_read(uc, UC_ARM_REG_C1_C0_2, &c1);
        c1 |= (0xFu << 20);
        uc_reg_write(uc, UC_ARM_REG_C1_C0_2, &c1);
        uc_reg_write(uc, UC_ARM_REG_FPEXC, &fpexc);
    }
    return 1;
}

/* mirrors guest.py's call(): emu_start, service any pending trap, resume */
static double run(int iters, int thumb, int slow, int null_shim)
{
    uint32_t sp = STACK_TOP - 0x1000, lr = RET_MAGIC, cpsr;
    uint32_t dst = HEAP_BASE, src = HEAP_BASE + 0x1000;
    uint32_t r0 = (uint32_t)iters, addr;
    uint8_t seed[64];
    double t0;
    int i;

    g_force_slow = slow; g_null_shim = null_shim; g_traps = 0; g_pending = 0;
    for (i = 0; i < 64; i++) seed[i] = (uint8_t)i;
    uc_mem_write(uc, src, seed, sizeof seed);
    uc_mem_write(uc, CODE_BASE, thumb ? CODE_THUMB : CODE_ARM,
                 thumb ? sizeof CODE_THUMB : sizeof CODE_ARM);

    uc_reg_write(uc, UC_ARM_REG_R0, &r0);
    uc_reg_write(uc, UC_ARM_REG_R1, &dst);
    uc_reg_write(uc, UC_ARM_REG_R2, &src);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);
    uc_reg_write(uc, UC_ARM_REG_LR, &lr);
    uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
    if (thumb) cpsr |= 0x20u; else cpsr &= ~0x20u;
    uc_reg_write(uc, UC_ARM_REG_CPSR, &cpsr);

    addr = CODE_BASE | (thumb ? 1u : 0u);
    t0 = now();
    for (;;) {
        uc_err e = uc_emu_start(uc, addr, RET_MAGIC, 0, 0);
        if (e != UC_ERR_OK) { printf("   uc error: %s\n", uc_strerror(e)); break; }
        if (!g_pending) break;
        g_pending = 0;
        shim_body();
        uc_reg_read(uc, UC_ARM_REG_LR, &lr);
        addr = lr;                        /* LSB carries the return mode */
    }
    return now() - t0;
}

int main(void)
{
    const int N = 200000;
    struct { const char *name; int thumb, slow, null_shim; } cases[] = {
        { "ARM caller   in-place, memcpy shim", 0, 0, 0 },
        { "ARM caller   in-place, null shim  ", 0, 0, 1 },
        { "ARM caller   stop/restart          ", 0, 1, 0 },
        { "Thumb caller mode switch (forced)  ", 1, 0, 0 },
    };
    int i;
    if (!setup()) { printf("unicorn init failed\n"); return 1; }
    printf("=== C trap round-trip cost (%d traps per case) ===\n", N);
    printf("   %-36s %12s %10s\n", "configuration", "traps/sec", "ns/trap");
    for (i = 0; i < (int)(sizeof cases / sizeof cases[0]); i++) {
        double dt = run(N, cases[i].thumb, cases[i].slow, cases[i].null_shim);
        if (g_traps != N) printf("   (serviced %ld of %d)\n", g_traps, N);
        printf("   %-36s %12.0f %10.0f\n", cases[i].name,
               g_traps / dt, dt / (double)g_traps * 1e9);
    }
    uc_close(uc);
    return 0;
}
