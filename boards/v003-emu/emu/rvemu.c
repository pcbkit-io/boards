/* rvemu: the guest RISC-V computer (see rvemu.h). One processor (RV32IMAC, machine mode, no
 * interrupts), a direct-mapped write-back cache of RV_LINES lines in front of the two serial
 * memories, and pcbkit-virt's devices. The guest's read-only segments (code, constants, the
 * game's data linked into them) are read where they lie in the flash; its writable segment is
 * copied into the PSRAM at boot. What the guest does that this computer does not provide (an
 * interrupt, a device it lacks, a store into its code) stops it with the reason. */
#include "rvemu.h"
#include <string.h>

#ifndef RV_LINES
#define RV_LINES 32u
#endif
#define RAM_BASE 0x80000000u
#define FLASH_KEY 0x80000000u
#define INVALID 0xffffffffu
#define HOT static inline __attribute__((always_inline))

static uint32_t tags[RV_LINES];
static uint8_t lines[RV_LINES][RV_LINE];
static uint32_t dirty[(RV_LINES + 31) / 32];
/* The read-only segments: guest address range and where it starts in the flash. */
static struct { uint32_t lo, hi, flash; } ro[2];
static uint32_t ram_size;
static uint32_t x[32], pc;
static uint32_t mstatus, mie, mtvec, mscratch, mepc, mcause, mtval, reservation = INVALID;
static uint64_t mtimecmp = ~0ull;
static uint32_t instret, instret_high;
/* The line the program counter runs in: its first guest address and its bytes (INVALID when that
 * line was evicted), so most fetches need no lookup. */
static uint32_t fetch_base = INVALID, fetch_index;
static uint8_t *fetch_bytes;
static struct { uint32_t ctrl, addr, width, height, stride, format, frames; } fb;

static void stop(const char *why) { rv_host_stop(why, 0, pc); }

static void evict(uint32_t i) {
    if (i == fetch_index) fetch_base = INVALID;
    if (dirty[i / 32] >> (i % 32) & 1u) {
        rv_host_write(tags[i], lines[i]);
        dirty[i / 32] &= ~(1u << (i % 32));
    }
}

/* The cache line holding memory key `k` (a flash or PSRAM byte offset, FLASH_KEY set for flash). */
HOT uint8_t *line(uint32_t k) {
    uint32_t base = k & ~(RV_LINE - 1), i = (base / RV_LINE) % RV_LINES;
    if (tags[i] != base) {
        evict(i);
        rv_host_read(base & FLASH_KEY ? 1 : 0, base & ~FLASH_KEY, lines[i]);
        tags[i] = base;
    }
    return lines[i] + (k & (RV_LINE - 1));
}

/* The memory key of guest address `a`; 0 with *mmio set for a device address. */
HOT uint32_t key(uint32_t a, int store, int *mmio) {
    *mmio = 0;
    if (a - RAM_BASE >= ram_size) {
        *mmio = 1;
        return 0;
    }
    for (int s = 0; s < 2; s++) {
        if (a - ro[s].lo < ro[s].hi - ro[s].lo) {
            if (store) stop("a store into the program's read-only memory (its code or constants)");
            return FLASH_KEY | (ro[s].flash + (a - ro[s].lo));
        }
    }
    return a - RAM_BASE;
}

HOT void mark_dirty(uint32_t k) {
    uint32_t i = ((k & ~(RV_LINE - 1)) / RV_LINE) % RV_LINES;
    dirty[i / 32] |= 1u << (i % 32);
}

static uint32_t mmio_load(uint32_t a, uint32_t size);
static void mmio_store(uint32_t a, uint32_t v, uint32_t size);

HOT uint32_t load(uint32_t a, uint32_t size) {
    int io;
    uint32_t k = key(a, 0, &io);
    if (io) return mmio_load(a, size);
    if ((k & (RV_LINE - 1)) + size <= RV_LINE) {
        uint8_t *p = line(k);
        if (size == 4) return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
        if (size == 2) return (uint32_t)p[0] | (uint32_t)p[1] << 8;
        return p[0];
    }
    /* Across two lines: byte by byte. */
    uint32_t v = 0;
    for (uint32_t n = 0; n < size; n++) v |= (uint32_t)*line(key(a + n, 0, &io)) << (8 * n);
    return v;
}

static void store(uint32_t a, uint32_t v, uint32_t size) {
    int io;
    uint32_t k = key(a, 1, &io);
    if (io) {
        mmio_store(a, v, size);
        return;
    }
    if (reservation == (a & ~3u)) reservation = INVALID;
    for (uint32_t n = 0; n < size; n++) {
        uint32_t kn = n && ((k & (RV_LINE - 1)) + n >= RV_LINE) ? key(a + n, 1, &io) : k + n;
        *line(kn) = (uint8_t)(v >> (8 * n));
        mark_dirty(kn);
    }
}

void rv_read(uint32_t address, uint8_t *to, uint32_t n) {
    while (n--) *to++ = (uint8_t)load(address++, 1);
}

static uint64_t mtime(void) { return rv_host_time_us() * 10u; }

static uint32_t mmio_load(uint32_t a, uint32_t size) {
    switch (a & ~0xffffu) {
    case 0x02000000u:
        switch (a & 0xffffu) {
        case 0x0000: return 0;
        case 0x4000: return (uint32_t)mtimecmp;
        case 0x4004: return (uint32_t)(mtimecmp >> 32);
        case 0xbff8: return (uint32_t)mtime();
        case 0xbffc: return (uint32_t)(mtime() >> 32);
        }
        break;
    case 0x10000000u:
        if (size == 1) return (a & 7u) == 5 ? 0x60u : 0; /* LSR: transmitter empty; nothing received */
        break;
    case 0x10010000u:
        if ((a & 0xffffu) == 0) return rv_host_inputs();
        if ((a & 0xffffu) < 0x48u) return 0;
        break;
    case 0x10020000u:
        switch (a & 0xfffu) {
        case 0x00: return fb.ctrl;
        case 0x04: return fb.addr;
        case 0x08: return fb.width;
        case 0x0c: return fb.height;
        case 0x10: return fb.stride;
        case 0x14: return fb.format;
        case 0x1c: return fb.frames;
        case 0x20: return 0;
        }
        break;
    }
    stop("a load from a device this computer does not have");
    return 0;
}

static void mmio_store(uint32_t a, uint32_t v, uint32_t size) {
    switch (a & ~0xffffu) {
    case 0x00100000u:
        if ((v & 0xffffu) == 0x5555u) rv_host_stop(0, 0, pc);
        if ((v & 0xffffu) == 0x3333u) rv_host_stop(0, v >> 16 ? v >> 16 : 1, pc);
        stop("a reset through the syscon is not provided");
        return;
    case 0x02000000u:
        switch (a & 0xffffu) {
        case 0x0000:
            if (v & 1u) stop("software interrupts are not provided");
            return;
        case 0x4000: mtimecmp = (mtimecmp & ~0xffffffffull) | v; return;
        case 0x4004: mtimecmp = (mtimecmp & 0xffffffffull) | (uint64_t)v << 32; return;
        }
        break;
    case 0x10000000u:
        if (size == 1) {
            if ((a & 7u) == 0) rv_host_console((uint8_t)v);
            return; /* line control, divisors, FIFO control: nothing to configure */
        }
        break;
    case 0x10010000u:
        if ((a & 0xffffu) == 0x08 && v) stop("GPIO outputs are not provided");
        if ((a & 0xffffu) < 0x48u) return;
        break;
    case 0x10020000u: {
        uint32_t o = a & 0xfffu;
        if (o >= 0x400 && o < 0x800) {
            /* The colour table: the host keeps what its screen needs of it. */
            rv_host_colour((o - 0x400) / 4, v & 0xffffffu);
            return;
        }
        switch (o) {
        case 0x00: fb.ctrl = v; return;
        case 0x04: fb.addr = v; return;
        case 0x08: fb.width = v; return;
        case 0x0c: fb.height = v; return;
        case 0x10: fb.stride = v; return;
        case 0x14:
            if (v != 2) stop("only the 8-bit indexed (L8) frame format is provided");
            fb.format = v;
            return;
        case 0x18:
            if (fb.ctrl & 1u) {
                rv_host_present(fb.addr, fb.width, fb.height, fb.stride);
                fb.frames++;
            }
            return;
        }
        break;
    }
    }
    stop("a store to a device this computer does not have");
}

static void trap(uint32_t cause, uint32_t value) {
    if (!mtvec) stop("an exception with no trap handler");
    mepc = pc;
    mcause = cause;
    mtval = value;
    mstatus = (mstatus & ~0x88u) | (mstatus & 8u) << 4 | 0x1800u;
    pc = mtvec & ~3u;
}

static int csr(uint32_t n, uint32_t *v, uint32_t write, int writes) {
    uint32_t old;
    switch (n) {
    case 0x300: old = mstatus; break;
    case 0x301: old = 0x40001105u; break;
    case 0x304: old = mie; break;
    case 0x305: old = mtvec; break;
    case 0x340: old = mscratch; break;
    case 0x341: old = mepc; break;
    case 0x342: old = mcause; break;
    case 0x343: old = mtval; break;
    case 0x344: old = 0; break;
    case 0xf11: case 0xf12: case 0xf13: case 0xf14: old = 0; break;
    case 0xc00: case 0xc02: case 0xb00: case 0xb02: old = instret; break;
    case 0xc80: case 0xc82: case 0xb80: case 0xb82: old = instret_high; break;
    case 0xc01: old = (uint32_t)mtime(); break;
    case 0xc81: old = (uint32_t)(mtime() >> 32); break;
    default: return 0;
    }
    *v = old;
    if (writes) {
        switch (n) {
        case 0x300: mstatus = (write & 0x88u) | 0x1800u; break;
        case 0x304: mie = write & 0x888u; break;
        case 0x305: mtvec = write & ~2u; break;
        case 0x340: mscratch = write; break;
        case 0x341: mepc = write & ~1u; break;
        case 0x342: mcause = write; break;
        case 0x343: mtval = write; break;
        default:
            if ((n >> 10) == 3) return 0; /* read-only */
            break;
        }
        if ((mstatus & 8u) && mie) stop("interrupts are not provided by this computer");
    }
    return 1;
}

/* A compressed instruction as the 32-bit instruction it stands for (0: illegal). */
static uint32_t expand(uint32_t c) {
#define R(n) (8u + ((c >> (n)) & 7u))
#define B(hi, lo) ((c >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1u))
    uint32_t rd = B(11, 7), rs2 = B(6, 2);
    switch ((c & 3u) << 3 | B(15, 13)) {
    case 000: { /* c.addi4spn */
        uint32_t imm = B(10, 7) << 6 | B(12, 11) << 4 | B(5, 5) << 3 | B(6, 6) << 2;
        return imm ? imm << 20 | 2u << 15 | R(2) << 7 | 0x13u : 0;
    }
    case 002: { /* c.lw */
        uint32_t imm = B(5, 5) << 6 | B(12, 10) << 3 | B(6, 6) << 2;
        return imm << 20 | R(7) << 15 | 2u << 12 | R(2) << 7 | 0x03u;
    }
    case 006: { /* c.sw */
        uint32_t imm = B(5, 5) << 6 | B(12, 10) << 3 | B(6, 6) << 2;
        return (imm >> 5) << 25 | R(2) << 20 | R(7) << 15 | 2u << 12 | (imm & 31u) << 7 | 0x23u;
    }
    case 010: case 012: { /* c.addi, c.li */
        uint32_t imm = (uint32_t)((int32_t)(B(12, 12) << 31 | B(6, 2) << 26) >> 26);
        return imm << 20 | ((c >> 14 & 1u) ? 0 : rd) << 15 | rd << 7 | 0x13u;
    }
    case 011: case 015: { /* c.jal, c.j */
        uint32_t imm = B(12, 12) << 11 | B(8, 8) << 10 | B(10, 9) << 8 | B(6, 6) << 7 | B(7, 7) << 6 | B(2, 2) << 5 | B(11, 11) << 4 | B(5, 3) << 1;
        int32_t s = (int32_t)(imm << 20) >> 20;
        uint32_t u = (uint32_t)s;
        return (u >> 20 & 1u) << 31 | (u >> 1 & 0x3ffu) << 21 | (u >> 11 & 1u) << 20 | (u >> 12 & 0xffu) << 12 | ((c >> 13 & 4u) ? 0 : 1u) << 7 | 0x6fu;
    }
    case 013: {
        if (rd == 2) { /* c.addi16sp */
            uint32_t imm = B(12, 12) << 9 | B(4, 3) << 7 | B(5, 5) << 6 | B(2, 2) << 5 | B(6, 6) << 4;
            int32_t s = (int32_t)(imm << 22) >> 22;
            return s ? (uint32_t)s << 20 | 2u << 15 | 2u << 7 | 0x13u : 0;
        }
        uint32_t imm = (uint32_t)((int32_t)(B(12, 12) << 31 | B(6, 2) << 26) >> 14); /* c.lui */
        return imm ? (imm & 0xfffff000u) | rd << 7 | 0x37u : 0;
    }
    case 014: {
        uint32_t r = R(7), sh = B(12, 12) << 5 | B(6, 2);
        switch (B(11, 10)) {
        case 0: return sh >> 5 ? 0 : sh << 20 | r << 15 | 5u << 12 | r << 7 | 0x13u;
        case 1: return sh >> 5 ? 0 : (0x400u | sh) << 20 | r << 15 | 5u << 12 | r << 7 | 0x13u;
        case 2: return (uint32_t)((int32_t)(B(12, 12) << 31 | B(6, 2) << 26) >> 6 & ~0xfffffu) | r << 15 | 7u << 12 | r << 7 | 0x13u;
        default: {
            static const uint32_t ops[4] = {0x40000033u, 0x4033u, 0x6033u, 0x7033u}; /* sub, xor, or, and */
            return B(12, 12) ? 0 : ops[B(6, 5)] | R(2) << 20 | r << 15 | r << 7;
        }
        }
    }
    case 016: case 017: { /* c.beqz, c.bnez */
        uint32_t imm = B(12, 12) << 8 | B(6, 5) << 6 | B(2, 2) << 5 | B(11, 10) << 3 | B(4, 3) << 1;
        uint32_t u = (uint32_t)((int32_t)(imm << 23) >> 23);
        return (u >> 12 & 1u) << 31 | (u >> 5 & 0x3fu) << 25 | R(7) << 15 | (c >> 13 & 1u) << 12 | (u >> 1 & 0xfu) << 8 | (u >> 11 & 1u) << 7 | 0x63u;
    }
    case 020: { /* c.slli */
        uint32_t sh = B(12, 12) << 5 | B(6, 2);
        return sh >> 5 ? 0 : sh << 20 | rd << 15 | 1u << 12 | rd << 7 | 0x13u;
    }
    case 022: { /* c.lwsp */
        uint32_t imm = B(3, 2) << 6 | B(12, 12) << 5 | B(6, 4) << 2;
        return rd ? imm << 20 | 2u << 15 | 2u << 12 | rd << 7 | 0x03u : 0;
    }
    case 024:
        if (!B(12, 12)) {
            if (!rs2) return rd ? rd << 15 | 0x67u : 0;    /* c.jr */
            return rs2 << 20 | rd << 7 | 0x33u;             /* c.mv */
        }
        if (!rs2) return rd ? rd << 15 | 1u << 7 | 0x67u : 0x00100073u; /* c.jalr, c.ebreak */
        return rs2 << 20 | rd << 15 | rd << 7 | 0x33u;      /* c.add */
    case 026: { /* c.swsp */
        uint32_t imm = B(8, 7) << 6 | B(12, 9) << 2;
        return (imm >> 5) << 25 | rs2 << 20 | 2u << 15 | 2u << 12 | (imm & 31u) << 7 | 0x23u;
    }
    }
    return 0;
#undef R
#undef B
}

static uint32_t mulh(int32_t a, int32_t b) { return (uint32_t)((int64_t)a * b >> 32); }
static uint32_t mulhsu(int32_t a, uint32_t b) { return (uint32_t)((int64_t)a * (int64_t)b >> 32); }
static uint32_t mulhu(uint32_t a, uint32_t b) { return (uint32_t)((uint64_t)a * b >> 32); }

/* The halfword at the program counter and the one after it, from the fetch line when they lie
 * in it. */
HOT uint32_t fetch(uint32_t at) {
    uint32_t o = at - fetch_base;
    if (o <= RV_LINE - 2) {
        uint8_t *p = fetch_bytes + o;
        return (uint32_t)p[0] | (uint32_t)p[1] << 8;
    }
    int io;
    uint32_t k = key(at, 0, &io);
    if (io) stop("an instruction fetch from a device");
    uint8_t *p = line(k);
    fetch_index = ((k & ~(RV_LINE - 1)) / RV_LINE) % RV_LINES;
    fetch_bytes = p - (k & (RV_LINE - 1));
    fetch_base = at - (k & (RV_LINE - 1));
    if ((k & (RV_LINE - 1)) == RV_LINE - 1) return load(at, 2);
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static void execute(void) {
    for (;;) {
        uint32_t i = fetch(pc), len = 4;
        if ((i & 3u) != 3u) {
            len = 2;
            uint32_t c = i;
            i = expand(c);
            if (!i) {
                trap(2, c);
                continue;
            }
        } else {
            i |= fetch(pc + 2) << 16;
        }
        uint32_t rd = i >> 7 & 31u, r1 = x[i >> 15 & 31u], r2 = x[i >> 20 & 31u], f3 = i >> 12 & 7u;
        int32_t imm = (int32_t)i >> 20;
        uint32_t next = pc + len, v = 0;
        int write = 1;
        if (!++instret) instret_high++;
        switch (i & 0x7fu) {
        case 0x37: v = i & 0xfffff000u; break;
        case 0x17: v = pc + (i & 0xfffff000u); break;
        case 0x6f: {
            uint32_t off = (i >> 31) << 20 | (i & 0xff000u) | (i >> 20 & 1u) << 11 | (i >> 21 & 0x3ffu) << 1;
            v = next;
            next = pc + (uint32_t)((int32_t)(off << 11) >> 11);
            break;
        }
        case 0x67:
            v = next;
            next = (r1 + (uint32_t)imm) & ~1u;
            break;
        case 0x63: {
            uint32_t off = (i >> 31) << 12 | (i >> 7 & 1u) << 11 | (i >> 25 & 0x3fu) << 5 | (i >> 8 & 0xfu) << 1;
            int take;
            switch (f3) {
            case 0: take = r1 == r2; break;
            case 1: take = r1 != r2; break;
            case 4: take = (int32_t)r1 < (int32_t)r2; break;
            case 5: take = (int32_t)r1 >= (int32_t)r2; break;
            case 6: take = r1 < r2; break;
            case 7: take = r1 >= r2; break;
            default: trap(2, i); continue;
            }
            if (take) next = pc + (uint32_t)((int32_t)(off << 19) >> 19);
            write = 0;
            break;
        }
        case 0x03: {
            uint32_t a = r1 + (uint32_t)imm;
            switch (f3) {
            case 0: v = (uint32_t)(int32_t)(int8_t)load(a, 1); break;
            case 1: v = (uint32_t)(int32_t)(int16_t)load(a, 2); break;
            case 2: v = load(a, 4); break;
            case 4: v = load(a, 1); break;
            case 5: v = load(a, 2); break;
            default: trap(2, i); continue;
            }
            break;
        }
        case 0x23: {
            uint32_t a = r1 + (uint32_t)((int32_t)(i & 0xfe000000u) >> 20 | (int32_t)(i >> 7 & 31u));
            if (f3 > 2) {
                trap(2, i);
                continue;
            }
            store(a, r2, 1u << f3);
            write = 0;
            break;
        }
        case 0x13: {
            uint32_t sh = i >> 20 & 31u;
            switch (f3) {
            case 0: v = r1 + (uint32_t)imm; break;
            case 1: v = r1 << sh; break;
            case 2: v = (int32_t)r1 < imm; break;
            case 3: v = r1 < (uint32_t)imm; break;
            case 4: v = r1 ^ (uint32_t)imm; break;
            case 5: v = (i >> 30 & 1u) ? (uint32_t)((int32_t)r1 >> sh) : r1 >> sh; break;
            case 6: v = r1 | (uint32_t)imm; break;
            default: v = r1 & (uint32_t)imm; break;
            }
            break;
        }
        case 0x33:
            if (i >> 25 == 1) {
                switch (f3) {
                case 0: v = r1 * r2; break;
                case 1: v = mulh((int32_t)r1, (int32_t)r2); break;
                case 2: v = mulhsu((int32_t)r1, r2); break;
                case 3: v = mulhu(r1, r2); break;
                case 4: v = !r2 ? ~0u : (r1 == 0x80000000u && r2 == ~0u) ? r1 : (uint32_t)((int32_t)r1 / (int32_t)r2); break;
                case 5: v = r2 ? r1 / r2 : ~0u; break;
                case 6: v = !r2 ? r1 : (r1 == 0x80000000u && r2 == ~0u) ? 0 : (uint32_t)((int32_t)r1 % (int32_t)r2); break;
                default: v = r2 ? r1 % r2 : r1; break;
                }
            } else {
                switch (f3 | (i >> 30 & 1u) << 3) {
                case 0: v = r1 + r2; break;
                case 8: v = r1 - r2; break;
                case 1: v = r1 << (r2 & 31u); break;
                case 2: v = (int32_t)r1 < (int32_t)r2; break;
                case 3: v = r1 < r2; break;
                case 4: v = r1 ^ r2; break;
                case 5: v = r1 >> (r2 & 31u); break;
                case 13: v = (uint32_t)((int32_t)r1 >> (r2 & 31u)); break;
                case 6: v = r1 | r2; break;
                case 7: v = r1 & r2; break;
                default: trap(2, i); continue;
                }
            }
            break;
        case 0x0f: write = 0; break; /* fence, fence.i: one memory, no decode cache */
        case 0x2f: {
            uint32_t op = i >> 27;
            if (f3 != 2 || (r1 & 3u)) {
                trap(f3 != 2 ? 2 : 6, f3 != 2 ? i : r1);
                continue;
            }
            if (op == 2) {
                v = load(r1, 4);
                reservation = r1;
            } else if (op == 3) {
                if (reservation == r1) {
                    store(r1, r2, 4);
                    v = 0;
                } else {
                    v = 1;
                }
                reservation = INVALID;
            } else {
                uint32_t old = load(r1, 4), nv;
                switch (op) {
                case 0x01: nv = r2; break;
                case 0x00: nv = old + r2; break;
                case 0x04: nv = old ^ r2; break;
                case 0x0c: nv = old & r2; break;
                case 0x08: nv = old | r2; break;
                case 0x10: nv = (int32_t)old < (int32_t)r2 ? old : r2; break;
                case 0x14: nv = (int32_t)old > (int32_t)r2 ? old : r2; break;
                case 0x18: nv = old < r2 ? old : r2; break;
                case 0x1c: nv = old > r2 ? old : r2; break;
                default: trap(2, i); continue;
                }
                store(r1, nv, 4);
                v = old;
            }
            break;
        }
        case 0x73:
            if (f3 == 0) {
                write = 0;
                if (i == 0x00000073u) { trap(11, 0); continue; }
                if (i == 0x00100073u) { trap(3, pc); continue; }
                if (i == 0x30200073u) {
                    mstatus = (mstatus & ~8u) | (mstatus >> 4 & 8u) | 0x80u;
                    next = mepc;
                    break;
                }
                if (i == 0x10500073u) break; /* wfi: no interrupt can come; carry on */
                trap(2, i);
                continue;
            } else {
                uint32_t src = (f3 & 4u) ? (i >> 15 & 31u) : r1, old, nv;
                int writes = (f3 & 3u) == 1 || (i >> 15 & 31u);
                if (!csr(i >> 20, &old, 0, 0)) {
                    trap(2, i);
                    continue;
                }
                nv = (f3 & 3u) == 1 ? src : (f3 & 3u) == 2 ? old | src : old & ~src;
                if (!csr(i >> 20, &old, nv, writes)) {
                    trap(2, i);
                    continue;
                }
                v = old;
            }
            break;
        default:
            trap(2, i);
            continue;
        }
        if (write && rd) x[rd] = v;
        pc = next;
    }
}

void rv_run(uint32_t ram_bytes) {
    for (uint32_t i = 0; i < RV_LINES; i++) tags[i] = INVALID;
    ram_size = ram_bytes;
    /* The ELF file at the start of the flash: its header and program headers. */
    uint8_t h[RV_LINE * 4];
    for (uint32_t n = 0; n < sizeof h; n += RV_LINE) rv_host_read(1, n, h + n);
    if (h[0] != 0x7f || h[1] != 'E' || h[2] != 'L' || h[3] != 'F' || h[4] != 1 || h[18] != 243) rv_host_stop("the flash holds no 32-bit RISC-V ELF program", 0, 0);
#define W32(p) ((uint32_t)(p)[0] | (uint32_t)(p)[1] << 8 | (uint32_t)(p)[2] << 16 | (uint32_t)(p)[3] << 24)
    uint32_t entry = W32(h + 24), phoff = W32(h + 28), phnum = (uint32_t)h[44] | (uint32_t)h[45] << 8;
    unsigned readonly = 0;
    for (uint32_t n = 0; n < phnum; n++) {
        uint8_t p[32];
        uint32_t at = phoff + n * 32;
        for (uint32_t k = 0; k < 32; k++) {
            uint8_t b[RV_LINE];
            rv_host_read(1, (at + k) & ~(RV_LINE - 1), b);
            p[k] = b[(at + k) & (RV_LINE - 1)];
        }
        uint32_t type = W32(p), offset = W32(p + 4), vaddr = W32(p + 8), filesz = W32(p + 16), memsz = W32(p + 20), flags = W32(p + 24);
        if (type != 1 || !memsz) continue;
        if (vaddr - RAM_BASE >= ram_size || memsz > ram_size - (vaddr - RAM_BASE)) rv_host_stop("the program does not fit this computer's RAM", 0, vaddr);
        if (!(flags & 2u) && filesz == memsz && readonly < 2 && (offset & (RV_LINE - 1)) == (vaddr & (RV_LINE - 1))) {
            /* Code and constants: read in place from the flash. */
            ro[readonly].lo = vaddr;
            ro[readonly].hi = vaddr + memsz;
            ro[readonly].flash = offset;
            readonly++;
            continue;
        }
        /* Data: copied into the PSRAM, the rest of the segment cleared. */
        uint8_t l[RV_LINE];
        uint32_t held = INVALID;
        for (uint32_t k = 0; k < memsz; k++) {
            uint8_t b = 0;
            if (k < filesz) {
                uint32_t at = (offset + k) & ~(RV_LINE - 1);
                if (at != held) rv_host_read(1, held = at, l);
                b = l[(offset + k) & (RV_LINE - 1)];
            }
            store(vaddr + k, b, 1);
        }
    }
#undef W32
    pc = entry;
    execute();
}
