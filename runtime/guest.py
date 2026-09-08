"""
guest.py -- ARM32 guest runtime for libHAL.Android.so  (milestones M0 + M1)

Maps the 32-bit library into an emulated address space, applies its
relocations, and points every imported symbol at a trap address that calls
back into host Python.  No Android, no NDK: this is the desktop harness the
plan calls for, where the edit cycle is seconds instead of minutes.
"""
import struct
from unicorn import *
from unicorn.arm_const import *

PAGE = 0x1000
align_dn = lambda x: x & ~(PAGE - 1)
align_up = lambda x: (x + PAGE - 1) & ~(PAGE - 1)

# ---- guest address map -------------------------------------------------
IMAGE_BASE  = 0x10000000
HEAP_BASE   = 0x40000000
HEAP_SIZE   = 256 * 1024 * 1024
STACK_TOP   = 0x70000000
STACK_SIZE  = 8 * 1024 * 1024
DATA_BASE   = 0x7E000000          # host-provided data symbols live here
DATA_SIZE   = 0x10000
TRAP_BASE   = 0x7F000000          # one 4-byte slot per imported function
TRAP_SIZE   = 0x1000
RET_MAGIC   = 0x7FFF0000          # emu_start stops when PC reaches this

R_RELATIVE, R_ABS32, R_GLOB_DAT, R_JUMP_SLOT = 23, 2, 21, 22


class GuestError(Exception):
    pass


class J(int):
    """Marks an argument as a 64-bit jlong/int64.

    AAPCS puts 64-bit values in an even-aligned register pair, so a jlong
    arriving after an odd number of words forces a padding slot.  onArchiveInit
    takes two of them."""
    pass


class Guest:
    def __init__(self, path, verbose=False):
        self.verbose = verbose
        self.blob = open(path, 'rb').read()
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.shims = {}          # name -> python callable(guest) -> None
        self.imports = []        # index -> name
        self.trap_of = {}        # name -> guest address
        self.calls = {}          # name -> call count (tracing)
        self.unimplemented = []
        self._pending = None
        self.force_slow_traps = False   # benchmarking switch
        self._parse()
        self._map()
        self._data_symbols()
        self._traps()
        self._relocate()
        self._heap_init()
        self._enable_vfp()
        self.last_fault = None
        self.uc.hook_add(UC_HOOK_MEM_INVALID, self._on_bad_mem)

    def _on_bad_mem(self, uc, access, address, size, value, user):
        kind = {UC_MEM_READ_UNMAPPED: 'read', UC_MEM_WRITE_UNMAPPED: 'write',
                UC_MEM_FETCH_UNMAPPED: 'fetch'}.get(access, 'access')
        self.last_fault = (kind, address, size, value)
        return False

    def _enable_vfp(self):
        """ARM cores come out of reset with the FPU off, and Unicorn is
        faithful about it.  The ABI here is soft-float, but libgcc's own
        double-precision helpers still contain VFP instructions, so those
        paths fault as UC_ERR_INSN_INVALID unless coprocessor access is
        granted.  Grant full access to cp10/cp11 and set FPEXC.EN."""
        self.uc.reg_write(UC_ARM_REG_C1_C0_2,
                          self.uc.reg_read(UC_ARM_REG_C1_C0_2) | (0xf << 20))
        self.uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)

    # ------------------------------------------------------------ parsing
    def _parse(self):
        d = self.blob
        u32 = lambda o: struct.unpack_from('<I', d, o)[0]
        u16 = lambda o: struct.unpack_from('<H', d, o)[0]
        self.u32f, self.u16f = u32, u16
        phoff, phent, phnum = u32(28), u16(42), u16(44)
        self.loads = []
        for i in range(phnum):
            o = phoff + i * phent
            if u32(o) == 1:                                  # PT_LOAD
                self.loads.append(dict(off=u32(o + 4), vaddr=u32(o + 8),
                                       filesz=u32(o + 16), memsz=u32(o + 20),
                                       flags=u32(o + 24)))
        shoff, shnum, shent, shstr = u32(32), u16(48), u16(46), u16(50)
        sh = lambda i: dict(nameoff=u32(shoff + i * shent),
                            off=u32(shoff + i * shent + 16),
                            size=u32(shoff + i * shent + 20),
                            addr=u32(shoff + i * shent + 12))
        base = sh(shstr)['off']
        self.S = {}
        for i in range(shnum):
            s = sh(i)
            e = d.index(b'\x00', base + s['nameoff'])
            self.S[d[base + s['nameoff']:e].decode()] = s

        # dynamic symbols (import resolution)
        dy, ds = self.S['.dynsym'], self.S['.dynstr']['off']
        self.dynsym = []
        for o in range(dy['off'], dy['off'] + dy['size'], 16):
            nmo, val, shndx, info = u32(o), u32(o + 4), u16(o + 14), d[o + 12]
            name = ''
            if nmo:
                e = d.index(b'\x00', ds + nmo)
                name = d[ds + nmo:e].decode('utf-8', 'replace')
            self.dynsym.append(dict(name=name, value=val, shndx=shndx,
                                    type=info & 0xf))
        # full symbol table (debugging + calling functions by name)
        sy, st = self.S['.symtab'], self.S['.strtab']['off']
        self.sym, self.byaddr = {}, {}
        for o in range(sy['off'], sy['off'] + sy['size'], 16):
            nmo = u32(o)
            if nmo == 0:
                continue
            e = d.index(b'\x00', st + nmo)
            n = d[st + nmo:e].decode('utf-8', 'replace')
            if (d[o + 12] & 0xf) == 2 and u16(o + 14) != 0:   # STT_FUNC, defined
                self.sym[n] = (u32(o + 4), u32(o + 8))
                self.byaddr.setdefault(u32(o + 4) & ~1, n)

    # ------------------------------------------------------------ mapping
    def _map(self):
        uc = self.uc
        lo = min(align_dn(l['vaddr']) for l in self.loads)
        hi = max(align_up(l['vaddr'] + l['memsz']) for l in self.loads)
        self.base = IMAGE_BASE
        self.span = hi - lo
        uc.mem_map(IMAGE_BASE + lo, self.span, UC_PROT_ALL)
        for l in self.loads:
            uc.mem_write(IMAGE_BASE + l['vaddr'],
                         self.blob[l['off']:l['off'] + l['filesz']])
        uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        uc.mem_map(DATA_BASE, DATA_SIZE, UC_PROT_READ | UC_PROT_WRITE)
        uc.mem_map(TRAP_BASE, TRAP_SIZE, UC_PROT_ALL)
        uc.mem_map(align_dn(RET_MAGIC), PAGE, UC_PROT_ALL)
        # exidx bounds, for __gnu_Unwind_Find_exidx
        ex = self.S.get('.ARM.exidx')
        self.exidx = (IMAGE_BASE + ex['addr'], ex['size'] // 8) if ex else (0, 0)

    def _data_symbols(self):
        """Host-provided OBJECT/NOTYPE imports need real guest memory."""
        self.data_syms = {}
        p = DATA_BASE
        # __sF: three FILE structs.  Only their addresses are ever compared,
        # so 0x100 of zeroed space each is plenty for stdin/stdout/stderr.
        self.data_syms['__sF'] = p; p += 0x300
        self.data_syms['__stack_chk_guard'] = p
        self.uc.mem_write(p, struct.pack('<I', 0xDEADC0DE)); p += 4
        self.errno_addr = p; p += 4
        self.scratch = p                                     # shim scratch space
        self.scratch_end = DATA_BASE + DATA_SIZE

    # -------------------------------------------------------------- traps
    def _traps(self):
        names = []
        for s in self.dynsym:
            if s['name'] and s['shndx'] == 0 and s['name'] not in self.data_syms:
                if s['name'] not in names:
                    names.append(s['name'])
        self.imports = names
        for i, n in enumerate(names):
            self.trap_of[n] = TRAP_BASE + i * 4
        self.trap_regions = []
        self.add_trap_region(TRAP_BASE, TRAP_SIZE, self._shim_body, mapped=True)

    def add_trap_region(self, base, size, handler, mapped=False):
        """Register a page of addresses that re-enter host code when executed.

        The PLT import table uses one.  The fake JNIEnv vtable uses another:
        both are just tables of addresses the guest branches to, and both need
        to come back out to Python.
        """
        if not mapped:
            self.uc.mem_map(base, size, UC_PROT_ALL)
        self.trap_regions.append((base, base + size, handler))
        self.uc.hook_add(UC_HOOK_CODE, self._on_trap,
                         begin=base, end=base + size - 1)

    def _handler_for(self, addr):
        for lo, hi, fn in self.trap_regions:
            if lo <= addr < hi:
                return fn
        raise GuestError("trap at unregistered address 0x%x" % addr)

    def _on_trap(self, uc, address, size, user):
        """Service the trap, or step outside the CPU to do it.

        Servicing in place cannot switch instruction sets: Unicorn fixes ARM
        vs Thumb when it *translates* a block, so writing PC and CPSR from
        inside a code hook resumes in the old mode and decodes garbage.  When
        the return mode matches the current one that does not matter and we
        stay in; otherwise we must stop and let the caller restart us.
        """
        lr = uc.reg_read(UC_ARM_REG_LR)
        cur_thumb = bool(uc.reg_read(UC_ARM_REG_CPSR) & 0x20)
        if bool(lr & 1) == cur_thumb and not self.force_slow_traps:
            self._handler_for(address)(address)
            uc.reg_write(UC_ARM_REG_PC, lr & ~1)
            return
        self._pending = address
        uc.emu_stop()

    def _service(self, trap):
        self._handler_for(trap)(trap)
        lr = self.uc.reg_read(UC_ARM_REG_LR)
        return lr & ~1, bool(lr & 1)

    def _shim_body(self, trap_addr):
        """Handler for the PLT import region."""
        idx = (trap_addr - TRAP_BASE) // 4
        if idx >= len(self.imports):
            raise GuestError("trap at unassigned import slot 0x%x" % trap_addr)
        name = self.imports[idx]
        self.calls[name] = self.calls.get(name, 0) + 1
        fn = self.shims.get(name)
        if fn is None:
            if name not in self.unimplemented:
                self.unimplemented.append(name)
            if self.verbose:
                print("   [UNIMPLEMENTED IMPORT] %s" % name)
            self.uc.reg_write(UC_ARM_REG_R0, 0)
        else:
            fn(self)
        return name

    # --------------------------------------------------------- relocation
    def _relocate(self):
        uc, d, base = self.uc, self.blob, IMAGE_BASE
        self.reloc_stats = {}
        self.missing_syms = []
        for sec in ('.rel.dyn', '.rel.plt'):
            s = self.S.get(sec)
            if not s:
                continue
            for o in range(s['off'], s['off'] + s['size'], 8):
                where, info = self.u32f(o), self.u32f(o + 4)
                rtype, symidx = info & 0xff, info >> 8
                p = base + where
                self.reloc_stats[rtype] = self.reloc_stats.get(rtype, 0) + 1
                if rtype == R_RELATIVE:
                    cur = struct.unpack('<I', uc.mem_read(p, 4))[0]
                    uc.mem_write(p, struct.pack('<I', (cur + base) & 0xffffffff))
                elif rtype in (R_GLOB_DAT, R_JUMP_SLOT, R_ABS32):
                    val = self._resolve(symidx)
                    if rtype == R_ABS32:
                        cur = struct.unpack('<I', uc.mem_read(p, 4))[0]
                        val = (val + cur) & 0xffffffff
                    uc.mem_write(p, struct.pack('<I', val & 0xffffffff))
                else:
                    raise GuestError("unhandled reloc type %d" % rtype)

    def _resolve(self, symidx):
        s = self.dynsym[symidx]
        if s['shndx'] != 0:                                  # defined locally
            return IMAGE_BASE + s['value']
        n = s['name']
        if n in self.data_syms:
            return self.data_syms[n]
        if n in self.trap_of:
            return self.trap_of[n]
        self.missing_syms.append(n)
        return 0

    # --------------------------------------------------------------- heap
    def _heap_init(self):
        # first-fit free list; 8-byte header {size, free}
        self.heap_ptr = HEAP_BASE
        self.blocks = {}                                     # addr -> [size, free]
        self.peak = 0

    def malloc(self, n):
        n = (n + 7) & ~7
        if n == 0:
            n = 8
        for a, b in self.blocks.items():                     # first fit
            if b[1] and b[0] >= n:
                b[1] = False
                return a
        a = self.heap_ptr
        if a + n > HEAP_BASE + HEAP_SIZE:
            raise GuestError("guest heap exhausted")
        self.heap_ptr += n
        self.peak = max(self.peak, self.heap_ptr - HEAP_BASE)
        self.blocks[a] = [n, False]
        return a

    def free(self, a):
        if a and a in self.blocks:
            self.blocks[a][1] = True

    def alloc_scratch(self, n):
        a = self.scratch
        self.scratch = (self.scratch + n + 7) & ~7
        if self.scratch >= self.scratch_end:
            raise GuestError("scratch exhausted")
        return a

    # -------------------------------------------------- register plumbing
    def arg(self, i):
        """Integer argument i under AAPCS (r0-r3, then stack)."""
        if i < 4:
            return self.uc.reg_read(UC_ARM_REG_R0 + i)
        sp = self.uc.reg_read(UC_ARM_REG_SP)
        return struct.unpack('<I', self.uc.mem_read(sp + (i - 4) * 4, 4))[0]

    def argd(self, i):
        """Soft-float double occupying an even-aligned register pair."""
        lo, hi = self.arg(i), self.arg(i + 1)
        return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

    def ret(self, v):
        self.uc.reg_write(UC_ARM_REG_R0, v & 0xffffffff)

    def ret64(self, v):
        self.uc.reg_write(UC_ARM_REG_R0, v & 0xffffffff)
        self.uc.reg_write(UC_ARM_REG_R1, (v >> 32) & 0xffffffff)

    def retd(self, x):
        lo, hi = struct.unpack('<II', struct.pack('<d', x))
        self.uc.reg_write(UC_ARM_REG_R0, lo)
        self.uc.reg_write(UC_ARM_REG_R1, hi)

    def cstr(self, addr, limit=4096):
        if not addr:
            return None
        out = bytearray()
        while len(out) < limit:
            b = self.uc.mem_read(addr + len(out), 1)[0]
            if b == 0:
                break
            out.append(b)
        return bytes(out).decode('utf-8', 'replace')

    def read(self, a, n):
        return bytes(self.uc.mem_read(a, n))

    def write(self, a, b):
        self.uc.mem_write(a, bytes(b))

    # ------------------------------------------------------------ calling
    def call(self, name_or_addr, *args, thumb=None, doubles=()):
        """Call a guest function and return r0 (or the r0:r1 double)."""
        uc = self.uc
        if isinstance(name_or_addr, str):
            if name_or_addr not in self.sym:
                raise GuestError("no symbol %s" % name_or_addr)
            val, _ = self.sym[name_or_addr]
            addr, is_thumb = IMAGE_BASE + (val & ~1), bool(val & 1)
        else:
            addr, is_thumb = name_or_addr, bool(thumb)

        regs = []
        for a in args:
            if isinstance(a, J):
                if len(regs) % 2:
                    regs.append(0)
                regs += [a & 0xffffffff, (a >> 32) & 0xffffffff]
            elif isinstance(a, float):
                lo, hi = struct.unpack('<II', struct.pack('<d', a))
                if len(regs) % 2:
                    regs.append(0)
                regs += [lo, hi]
            else:
                regs.append(a & 0xffffffff)
        sp = STACK_TOP - 0x1000
        if len(regs) > 4:
            extra = regs[4:]
            sp -= len(extra) * 4
            sp &= ~7
            uc.mem_write(sp, b''.join(struct.pack('<I', x) for x in extra))
        for i in range(min(4, len(regs))):
            uc.reg_write(UC_ARM_REG_R0 + i, regs[i])
        uc.reg_write(UC_ARM_REG_SP, sp)
        uc.reg_write(UC_ARM_REG_LR, RET_MAGIC)
        cpsr = uc.reg_read(UC_ARM_REG_CPSR)
        uc.reg_write(UC_ARM_REG_CPSR, (cpsr | 0x20) if is_thumb else (cpsr & ~0x20))
        self._pending = None
        steps = 0
        while True:
            try:
                uc.emu_start(addr | (1 if is_thumb else 0), RET_MAGIC)
            except UcError as e:
                raise GuestError("%s while running %s -- %s"
                                 % (e, name_or_addr, self.where())) from None
            if self._pending is None:
                break
            trap, self._pending = self._pending, None
            addr, is_thumb = self._service(trap)
            steps += 1
            if steps > 5_000_000:
                raise GuestError("runaway shim loop in %s" % name_or_addr)
        return uc.reg_read(UC_ARM_REG_R0)

    def where(self):
        """Symbolicate the current PC (milestone M8, useful from day one)."""
        pc = self.uc.reg_read(UC_ARM_REG_PC)
        off = pc - IMAGE_BASE
        best, bestva = None, -1
        for n, (v, sz) in self.sym.items():
            va = v & ~1
            if va <= off < va + max(sz, 4) and va > bestva:
                best, bestva = n, va
        loc = "%s+0x%x" % (best, off - bestva) if best else "unmapped"
        out = "pc=0x%08x (%s)" % (pc, loc)
        if self.last_fault:
            k, a, sz, v = self.last_fault
            out += "  faulting %s of %d bytes at 0x%08x" % (k, sz, a)
        return out

    def call_d(self, name, *args):
        self.call(name, *args)
        lo = self.uc.reg_read(UC_ARM_REG_R0)
        hi = self.uc.reg_read(UC_ARM_REG_R1)
        return struct.unpack('<d', struct.pack('<II', lo, hi))[0]

    # --------------------------------------------------------------- info
    def describe(self):
        RT = {23: 'R_ARM_RELATIVE', 2: 'R_ARM_ABS32',
              21: 'R_ARM_GLOB_DAT', 22: 'R_ARM_JUMP_SLOT'}
        print("   image      0x%08x .. 0x%08x  (%d KB)"
              % (IMAGE_BASE, IMAGE_BASE + self.span, self.span // 1024))
        print("   heap       0x%08x  %d MB" % (HEAP_BASE, HEAP_SIZE // 1048576))
        print("   stack top  0x%08x  %d MB" % (STACK_TOP, STACK_SIZE // 1048576))
        print("   traps      0x%08x  %d imports" % (TRAP_BASE, len(self.imports)))
        print("   exidx      0x%08x  %d entries" % self.exidx)
        print("   relocs     %s" % {RT.get(k, k): v for k, v in self.reloc_stats.items()})
        print("   symbols    %d functions" % len(self.sym))
        if self.missing_syms:
            print("   UNRESOLVED %s" % sorted(set(self.missing_syms)))
