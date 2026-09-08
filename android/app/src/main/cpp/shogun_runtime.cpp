// shogun_runtime.cpp -- see shogun_runtime.h.
//
// Ported from runtime/guest.py. Two behaviours here are load-bearing and were
// each found the hard way on the desktop harness:
//
//  1. VFP must be enabled at reset. The ABI is soft-float, but libgcc's own
//     double-precision helpers contain VFP instructions, and an ARM core comes
//     out of reset with the FPU off -- those paths fault as INSN_INVALID until
//     cp10/cp11 access and FPEXC.EN are set.
//
//  2. A trap cannot be serviced in place across an ARM/Thumb switch. Unicorn
//     fixes the instruction set when it *translates* a block, so writing PC and
//     CPSR from inside a code hook resumes in the old mode and decodes garbage.
//     Same-mode returns stay in; mode changes must stop the CPU and be resumed
//     by the caller.
#include "shogun_runtime.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace shogun {
namespace {

inline uint32_t RdU32(const uint8_t* p, size_t off) {
  uint32_t v; std::memcpy(&v, p + off, 4); return v;
}
inline uint16_t RdU16(const uint8_t* p, size_t off) {
  uint16_t v; std::memcpy(&v, p + off, 2); return v;
}
constexpr uint32_t kPage = 0x1000u;
inline uint32_t AlignDown(uint32_t x) { return x & ~(kPage - 1); }
inline uint32_t AlignUp(uint32_t x)   { return (x + kPage - 1) & ~(kPage - 1); }

}  // namespace

Runtime::~Runtime() { if (uc_) uc_close(uc_); }

// ---------------------------------------------------------------- loading
bool Runtime::Load(const uint8_t* image, size_t len, std::string* err) {
  image_.assign(image, image + len);
  if (image_.size() < 64 || std::memcmp(image_.data(), "\x7f" "ELF", 4) != 0) {
    *err = "not an ELF"; return false;
  }
  if (uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc_) != UC_ERR_OK) {
    *err = "uc_open failed"; return false;
  }
  if (!ParseElf(err)) return false;
  if (!MapMemory(err)) return false;
  SetupDataSymbols();
  SetupImportTraps();
  if (!ApplyRelocations(err)) return false;
  EnableVfp();

  uc_hook h;
  uc_hook_add(uc_, &h, UC_HOOK_MEM_INVALID, (void*)&Runtime::HookBadMem, this, 1, 0);
  scratch_end_ = kDataBase + kDataSize;
  heap_ptr_ = kHeapBase;
  return true;
}

bool Runtime::ParseElf(std::string* err) {
  const uint8_t* d = image_.data();
  uint32_t phoff = RdU32(d, 28);
  uint16_t phent = RdU16(d, 42), phnum = RdU16(d, 44);
  for (uint16_t i = 0; i < phnum; i++) {
    size_t o = phoff + i * phent;
    if (RdU32(d, o) == 1)  // PT_LOAD
      loads_.push_back({RdU32(d, o + 4), RdU32(d, o + 8),
                        RdU32(d, o + 16), RdU32(d, o + 20), RdU32(d, o + 24)});
  }
  if (loads_.empty()) { *err = "no PT_LOAD"; return false; }

  uint32_t shoff = RdU32(d, 32);
  uint16_t shnum = RdU16(d, 48), shent = RdU16(d, 46), shstr = RdU16(d, 50);
  auto sh = [&](uint16_t i) {
    size_t o = shoff + i * shent;
    return Section{RdU32(d, o + 16), RdU32(d, o + 20), RdU32(d, o + 12)};
  };
  uint32_t strbase = sh(shstr).off;
  for (uint16_t i = 0; i < shnum; i++) {
    size_t o = shoff + i * shent;
    uint32_t nameoff = RdU32(d, o);
    const char* nm = reinterpret_cast<const char*>(d + strbase + nameoff);
    sections_[std::string(nm)] = sh(i);
  }

  // dynamic symbols -- import resolution
  auto dynsym = sections_.find(".dynsym");
  auto dynstr = sections_.find(".dynstr");
  if (dynsym == sections_.end() || dynstr == sections_.end()) {
    *err = "missing .dynsym/.dynstr"; return false;
  }
  uint32_t ds = dynstr->second.off;
  for (uint32_t o = dynsym->second.off;
       o < dynsym->second.off + dynsym->second.size; o += 16) {
    DynSym s;
    uint32_t nmo = RdU32(d, o);
    s.name  = nmo ? std::string(reinterpret_cast<const char*>(d + ds + nmo)) : "";
    s.value = RdU32(d, o + 4);
    s.type  = d[o + 12] & 0xf;
    s.shndx = RdU16(d, o + 14);
    dynsyms_.push_back(std::move(s));
  }

  // full symbol table -- lets traps and crashes report real names
  auto symtab = sections_.find(".symtab");
  auto strtab = sections_.find(".strtab");
  if (symtab != sections_.end() && strtab != sections_.end()) {
    uint32_t st = strtab->second.off;
    for (uint32_t o = symtab->second.off;
         o < symtab->second.off + symtab->second.size; o += 16) {
      uint32_t nmo = RdU32(d, o);
      if (!nmo) continue;
      if ((d[o + 12] & 0xf) != 2) continue;        // STT_FUNC only
      if (RdU16(d, o + 14) == 0) continue;         // defined only
      std::string n(reinterpret_cast<const char*>(d + st + nmo));
      uint32_t val = RdU32(d, o + 4), sz = RdU32(d, o + 8);
      syms_[n] = {val, sz};
      sym_by_addr_.emplace_back(val & ~1u, n);
    }
    std::sort(sym_by_addr_.begin(), sym_by_addr_.end());
  }

  auto ex = sections_.find(".ARM.exidx");
  if (ex != sections_.end()) {
    exidx_base_  = kImageBase + ex->second.addr;
    exidx_count_ = ex->second.size / 8;
  }
  return true;
}

bool Runtime::MapMemory(std::string* err) {
  uint32_t lo = 0xFFFFFFFFu, hi = 0;
  for (const auto& l : loads_) {
    lo = std::min(lo, AlignDown(l.vaddr));
    hi = std::max(hi, AlignUp(l.vaddr + l.memsz));
  }
  span_ = hi - lo;
  if (uc_mem_map(uc_, kImageBase + lo, span_, UC_PROT_ALL) != UC_ERR_OK) {
    *err = "image map failed"; return false;
  }
  for (const auto& l : loads_)
    uc_mem_write(uc_, kImageBase + l.vaddr, image_.data() + l.off, l.filesz);

  uc_mem_map(uc_, kStackTop - kStackSize, kStackSize, UC_PROT_READ | UC_PROT_WRITE);
  uc_mem_map(uc_, kHeapBase, kHeapSize, UC_PROT_READ | UC_PROT_WRITE);
  uc_mem_map(uc_, kDataBase, kDataSize, UC_PROT_READ | UC_PROT_WRITE);
  uc_mem_map(uc_, kTrapBase, kTrapSize, UC_PROT_ALL);
  uc_mem_map(uc_, AlignDown(kRetMagic), kPage, UC_PROT_ALL);
  return true;
}

void Runtime::SetupDataSymbols() {
  uint32_t p = kDataBase;
  data_syms_["__sF"] = p; p += 0x300;             // stdin/stdout/stderr
  data_syms_["__stack_chk_guard"] = p;
  uint32_t guard = 0xDEADC0DEu;
  uc_mem_write(uc_, p, &guard, 4); p += 4;
  errno_addr_ = p; p += 4;
  scratch_ = p;
}

void Runtime::SetupImportTraps() {
  for (const auto& s : dynsyms_) {
    if (s.name.empty() || s.shndx != 0) continue;
    if (data_syms_.count(s.name)) continue;
    if (std::find(imports_.begin(), imports_.end(), s.name) != imports_.end()) continue;
    imports_.push_back(s.name);
  }
  for (size_t i = 0; i < imports_.size(); i++)
    trap_of_[imports_[i]] = kTrapBase + static_cast<uint32_t>(i) * 4;

  AddTrapRegion(kTrapBase, kTrapSize,
                [](Runtime& rt, uint32_t addr) { rt.ServiceImport(addr); },
                /*already_mapped=*/true);
}

void Runtime::AddTrapRegion(uint32_t base, uint32_t size, TrapFn fn,
                            bool already_mapped) {
  if (!already_mapped) uc_mem_map(uc_, base, size, UC_PROT_ALL);
  trap_regions_.push_back({base, base + size, std::move(fn)});
  uc_hook h;
  uc_hook_add(uc_, &h, UC_HOOK_CODE, (void*)&Runtime::HookTrap, this,
              base, base + size - 1);
}

uint32_t Runtime::ResolveDynSym(uint32_t idx) {
  if (idx >= dynsyms_.size()) return 0;
  const DynSym& s = dynsyms_[idx];
  if (s.shndx != 0) return kImageBase + s.value;       // defined locally
  auto d = data_syms_.find(s.name);
  if (d != data_syms_.end()) return d->second;
  auto t = trap_of_.find(s.name);
  if (t != trap_of_.end()) return t->second;
  return 0;
}

bool Runtime::ApplyRelocations(std::string* err) {
  const uint8_t* d = image_.data();
  for (const char* sec : {".rel.dyn", ".rel.plt"}) {
    auto it = sections_.find(sec);
    if (it == sections_.end()) continue;
    for (uint32_t o = it->second.off; o < it->second.off + it->second.size; o += 8) {
      uint32_t where = RdU32(d, o), info = RdU32(d, o + 4);
      uint32_t type = info & 0xff, sym = info >> 8;
      uint32_t p = kImageBase + where, cur = 0, val = 0;
      switch (type) {
        case kRelRelative:
          uc_mem_read(uc_, p, &cur, 4);
          val = cur + kImageBase;
          break;
        case kRelAbs32:
          uc_mem_read(uc_, p, &cur, 4);
          val = ResolveDynSym(sym) + cur;
          break;
        case kRelGlobDat:
        case kRelJumpSlot:
          val = ResolveDynSym(sym);
          break;
        default:
          *err = "unhandled relocation type " + std::to_string(type);
          return false;
      }
      uc_mem_write(uc_, p, &val, 4);
    }
  }
  return true;
}

void Runtime::EnableVfp() {
  uint32_t c1 = 0, fpexc = 0x40000000u;
  uc_reg_read(uc_, UC_ARM_REG_C1_C0_2, &c1);
  c1 |= (0xFu << 20);
  uc_reg_write(uc_, UC_ARM_REG_C1_C0_2, &c1);
  uc_reg_write(uc_, UC_ARM_REG_FPEXC, &fpexc);
}

// ------------------------------------------------------------------ traps
uint32_t Runtime::SymAddr(const char* name) const {
  auto it = syms_.find(name);
  return it == syms_.end() ? 0u : kImageBase + it->second.value;
}

void Runtime::AddWatch(uint32_t addr, ShimFn fn) {
  const uint32_t at = addr & ~1u;          // callers pass the Thumb bit along
  watches_[at] = std::move(fn);
  uc_hook h = 0;
  uc_hook_add(uc_, &h, UC_HOOK_CODE, (void*)&Runtime::HookWatch, this, at, at);
}

void Runtime::HookWatch(uc_engine*, uint64_t addr, uint32_t, void* user) {
  auto* rt = static_cast<Runtime*>(user);
  auto it = rt->watches_.find(static_cast<uint32_t>(addr));
  if (it != rt->watches_.end()) it->second(*rt);
  // deliberately no PC write: the guest carries on into the real function
}

void Runtime::HookTrap(uc_engine* uc, uint64_t addr, uint32_t, void* user) {
  auto* rt = static_cast<Runtime*>(user);
  uint32_t lr = 0, cpsr = 0;
  uc_reg_read(uc, UC_ARM_REG_LR, &lr);
  uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr);
  const bool cur_thumb = (cpsr >> 5) & 1;
  if (((lr & 1u) != 0) == cur_thumb) {
    // Fast path: caller returns to the instruction set we are already in.
    for (auto& r : rt->trap_regions_)
      if (addr >= r.lo && addr < r.hi) { r.fn(*rt, static_cast<uint32_t>(addr)); break; }
    uint32_t pc = lr & ~1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &pc);
    return;
  }
  rt->pending_ = true;
  rt->pending_addr_ = static_cast<uint32_t>(addr);
  uc_emu_stop(uc);
}

bool Runtime::HookBadMem(uc_engine*, uc_mem_type t, uint64_t addr, int size,
                         int64_t, void* user) {
  auto* rt = static_cast<Runtime*>(user);
  const char* k = t == UC_MEM_READ_UNMAPPED    ? "read"
                : t == UC_MEM_WRITE_UNMAPPED   ? "write"
                : t == UC_MEM_FETCH_UNMAPPED   ? "fetch" : "access";
  rt->last_fault_ = {true, k, addr, size};
  return false;   // let it fault, we report it
}

void Runtime::ServiceImport(uint32_t trap_addr) {
  uint32_t idx = (trap_addr - kTrapBase) / 4;
  if (idx >= imports_.size()) { Ret(0); return; }
  const std::string& name = imports_[idx];
  call_counts_[name]++;
  auto it = shims_.find(name);
  if (it == shims_.end()) {
    if (std::find(unimplemented_.begin(), unimplemented_.end(), name) ==
        unimplemented_.end()) {
      unimplemented_.push_back(name);
      std::fprintf(stderr, "[shogun] UNIMPLEMENTED IMPORT: %s\n", name.c_str());
    }
    Ret(0);
    return;
  }
  it->second(*this);
}

void Runtime::SetShim(const std::string& n, ShimFn fn) { shims_[n] = std::move(fn); }

ShimFn Runtime::ShimFor(const std::string& n) const {
  auto it = shims_.find(n);
  return it == shims_.end() ? ShimFn() : it->second;
}

uint32_t Runtime::TrapOf(const std::string& n) const {
  auto it = trap_of_.find(n);
  return it == trap_of_.end() ? 0 : it->second;
}

// -------------------------------------------------- calling convention
uint32_t Runtime::Arg(int i) {
  uint32_t v = 0;
  if (i < 4) { uc_reg_read(uc_, UC_ARM_REG_R0 + i, &v); return v; }
  uint32_t sp = 0;
  uc_reg_read(uc_, UC_ARM_REG_SP, &sp);
  uc_mem_read(uc_, sp + (i - 4) * 4, &v, 4);
  return v;
}

double Runtime::ArgD(int i) {
  uint64_t lo = Arg(i), hi = Arg(i + 1);
  uint64_t bits = lo | (hi << 32);
  double d; std::memcpy(&d, &bits, 8); return d;
}

void Runtime::Ret(uint32_t v)  { uc_reg_write(uc_, UC_ARM_REG_R0, &v); }

void Runtime::Ret64(uint64_t v) {
  uint32_t lo = static_cast<uint32_t>(v), hi = static_cast<uint32_t>(v >> 32);
  uc_reg_write(uc_, UC_ARM_REG_R0, &lo);
  uc_reg_write(uc_, UC_ARM_REG_R1, &hi);
}

void Runtime::RetD(double v) {
  uint64_t bits; std::memcpy(&bits, &v, 8); Ret64(bits);
}

// ------------------------------------------------------------ guest memory
bool Runtime::Read(uint32_t a, void* dst, size_t n) {
  return uc_mem_read(uc_, a, dst, n) == UC_ERR_OK;
}
bool Runtime::Write(uint32_t a, const void* src, size_t n) {
  return uc_mem_write(uc_, a, src, n) == UC_ERR_OK;
}

std::string Runtime::CStr(uint32_t addr, size_t limit) {
  if (!addr) return std::string();
  std::string out;
  uint8_t b = 0;
  while (out.size() < limit) {
    if (uc_mem_read(uc_, addr + out.size(), &b, 1) != UC_ERR_OK) break;
    if (!b) break;
    out.push_back(static_cast<char>(b));
  }
  return out;
}

uint32_t Runtime::Malloc(uint32_t n) {
  n = (n + 7u) & ~7u;
  if (!n) n = 8;
  for (auto& kv : blocks_) {
    if (kv.second.free && kv.second.size >= n) { kv.second.free = false; return kv.first; }
  }
  uint32_t a = heap_ptr_;
  if (a + n > kHeapBase + kHeapSize) return 0;   // caller sees NULL, like malloc
  heap_ptr_ += n;
  heap_peak_ = std::max(heap_peak_, heap_ptr_ - kHeapBase);
  blocks_[a] = {n, false};
  return a;
}

void Runtime::Free(uint32_t a) {
  auto it = blocks_.find(a);
  if (it != blocks_.end()) it->second.free = true;
}

uint32_t Runtime::AllocScratch(uint32_t n) {
  uint32_t a = scratch_;
  scratch_ = (scratch_ + n + 7u) & ~7u;
  return scratch_ >= scratch_end_ ? 0 : a;
}

// ------------------------------------------------------------------ calling
// Each calling thread gets a 1 MB slice of the guest stack region, handed out
// on first use and remembered for the life of the thread.
uint32_t Runtime::StackForThisThread() {
  static std::atomic<uint32_t> next_slot{0};
  thread_local uint32_t slot = next_slot.fetch_add(1);
  constexpr uint32_t kSlice = 1u << 20;
  return kStackTop - slot * kSlice - 0x1000;
}

// A shim or hook that calls back into the guest would re-lock mu_ on a thread
// that already holds it and hang the whole game -- which is exactly what a
// settings-switch callback did. Depth is per-thread, so the legitimate case of
// two threads entering the guest still serialises on the lock as before.
thread_local int t_guest_depth = 0;

bool Runtime::CallAddr(uint32_t addr, bool thumb, const std::vector<uint32_t>& args,
                       uint32_t* out_r0, std::string* err) {
  if (t_guest_depth > 0) {
    if (err) *err = "re-entrant guest call (a shim called back into the guest)";
    return false;
  }
  struct Depth { Depth() { t_guest_depth++; } ~Depth() { t_guest_depth--; } } depth;

  std::lock_guard<std::mutex> lock(mu_);
  uint32_t sp = StackForThisThread();
  if (args.size() > 4) {
    size_t extra = args.size() - 4;
    sp -= static_cast<uint32_t>(extra * 4);
    sp &= ~7u;
    for (size_t i = 0; i < extra; i++)
      uc_mem_write(uc_, sp + i * 4, &args[i + 4], 4);
  }
  for (size_t i = 0; i < std::min<size_t>(4, args.size()); i++)
    uc_reg_write(uc_, UC_ARM_REG_R0 + static_cast<int>(i), &args[i]);

  uint32_t lr = kRetMagic, cpsr = 0;
  uc_reg_write(uc_, UC_ARM_REG_SP, &sp);
  uc_reg_write(uc_, UC_ARM_REG_LR, &lr);
  uc_reg_read(uc_, UC_ARM_REG_CPSR, &cpsr);
  cpsr = thumb ? (cpsr | 0x20u) : (cpsr & ~0x20u);
  uc_reg_write(uc_, UC_ARM_REG_CPSR, &cpsr);

  pending_ = false;
  last_fault_.valid = false;
  uint32_t start = addr | (thumb ? 1u : 0u);
  for (int guard = 0; guard < 50000000; guard++) {
    uc_err e = uc_emu_start(uc_, start, kRetMagic, 0, 0);
    if (e != UC_ERR_OK) {
      if (err) *err = std::string(uc_strerror(e)) + " -- " + Where();
      return false;
    }
    if (!pending_) break;
    pending_ = false;
    uint32_t ta = pending_addr_;
    for (auto& r : trap_regions_)
      if (ta >= r.lo && ta < r.hi) { r.fn(*this, ta); break; }
    uint32_t nlr = 0;
    uc_reg_read(uc_, UC_ARM_REG_LR, &nlr);
    start = nlr;                    // LSB carries the return instruction set
  }
  if (out_r0) uc_reg_read(uc_, UC_ARM_REG_R0, out_r0);
  return true;
}

bool Runtime::CallSym(const char* name, const std::vector<uint32_t>& args,
                      uint32_t* out_r0, std::string* err) {
  auto it = syms_.find(name);
  if (it == syms_.end()) {
    if (err) *err = std::string("no symbol ") + name;
    return false;
  }
  return CallAddr(kImageBase + (it->second.value & ~1u),
                  (it->second.value & 1u) != 0, args, out_r0, err);
}

std::string Runtime::Where() {
  uint32_t pc = 0;
  uc_reg_read(uc_, UC_ARM_REG_PC, &pc);
  char buf[256];
  std::string loc = "unmapped";
  if (pc >= kImageBase && pc < kImageBase + span_) {
    uint32_t off = pc - kImageBase;
    auto it = std::upper_bound(
        sym_by_addr_.begin(), sym_by_addr_.end(),
        std::make_pair(off, std::string("\xff")));
    if (it != sym_by_addr_.begin()) {
      --it;
      std::snprintf(buf, sizeof buf, "%s+0x%x", it->second.c_str(), off - it->first);
      loc = buf;
    }
  }
  if (last_fault_.valid) {
    std::snprintf(buf, sizeof buf,
                  "pc=0x%08x (%s)  faulting %s of %d bytes at 0x%08llx",
                  pc, loc.c_str(), last_fault_.kind, last_fault_.size,
                  static_cast<unsigned long long>(last_fault_.addr));
  } else {
    std::snprintf(buf, sizeof buf, "pc=0x%08x (%s)", pc, loc.c_str());
  }
  return buf;
}

}  // namespace shogun
