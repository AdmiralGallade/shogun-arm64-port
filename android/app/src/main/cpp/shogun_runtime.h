// shogun_runtime.h -- ARM32 guest runtime for libHAL.Android.so, on arm64.
//
// A direct port of the proven desktop harness (runtime/guest.py). The Python
// is the specification; this is the same design in C++ with the interpreter
// overhead removed. Constants, trap discipline and failure conventions are
// deliberately identical so the two can be diffed against each other.
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <unicorn/unicorn.h>

namespace shogun {

// ---- guest address map (identical to guest.py) --------------------------
constexpr uint32_t kImageBase = 0x10000000u;
constexpr uint32_t kHeapBase  = 0x40000000u;
constexpr uint32_t kHeapSize  = 256u * 1024u * 1024u;
constexpr uint32_t kStackTop  = 0x70000000u;
constexpr uint32_t kStackSize = 8u * 1024u * 1024u;
constexpr uint32_t kDataBase  = 0x7E000000u;   // __sF, __stack_chk_guard, errno
constexpr uint32_t kDataSize  = 0x10000u;
constexpr uint32_t kJniData   = 0x7C000000u;   // fake JNIEnv vtable + arena
constexpr uint32_t kJniTrap   = 0x7D000000u;   // 233 JNIEnv slots + JavaVM
constexpr uint32_t kTrapBase  = 0x7F000000u;   // one slot per PLT import
constexpr uint32_t kTrapSize  = 0x1000u;
constexpr uint32_t kRetMagic  = 0x7FFF0000u;   // emu_start stops here

// ARM relocation types actually present in this library (measured: 4).
constexpr uint32_t kRelRelative = 23, kRelAbs32 = 2,
                   kRelGlobDat  = 21, kRelJumpSlot = 22;

class Runtime;
using ShimFn    = std::function<void(Runtime&)>;
using TrapFn    = std::function<void(Runtime&, uint32_t /*trap addr*/)>;

struct Sym { uint32_t value; uint32_t size; };   // value keeps the Thumb bit

class Runtime {
 public:
  Runtime() = default;
  ~Runtime();

  // Loads the library from memory (Android hands it to us as an asset/fd).
  bool Load(const uint8_t* image, size_t len, std::string* err);

  // ---- calling convention (AAPCS, soft-float) ---------------------------
  uint32_t Arg(int i);              // r0-r3 then stack
  double   ArgD(int i);             // 64-bit double in an even register pair
  void     Ret(uint32_t v);
  void     RetD(double v);
  void     Ret64(uint64_t v);

  // ---- guest memory -----------------------------------------------------
  bool     Read(uint32_t addr, void* dst, size_t n);
  bool     Write(uint32_t addr, const void* src, size_t n);
  std::string CStr(uint32_t addr, size_t limit = 4096);
  uint32_t Malloc(uint32_t n);
  void     Free(uint32_t addr);
  uint32_t AllocScratch(uint32_t n);
  uint32_t HeapPeak() const { return heap_peak_; }

  // ---- calling into the guest -------------------------------------------
  // Runs until the guest returns; services traps on the way.
  bool CallAddr(uint32_t addr, bool thumb, const std::vector<uint32_t>& args,
                uint32_t* out_r0, std::string* err);
  bool CallSym(const char* name, const std::vector<uint32_t>& args,
               uint32_t* out_r0, std::string* err);

  // ---- registration ------------------------------------------------------
  void SetShim(const std::string& import_name, ShimFn fn);
  ShimFn ShimFor(const std::string& import_name) const;
  void AddTrapRegion(uint32_t base, uint32_t size, TrapFn fn, bool already_mapped);

  // ---- introspection -----------------------------------------------------
  uc_engine* uc() const { return uc_; }
  const std::vector<std::string>& imports() const { return imports_; }
  uint32_t TrapOf(const std::string& import_name) const;
  bool     HasSym(const char* name) const { return syms_.count(name) != 0; }
  uint32_t ExidxBase() const { return exidx_base_; }
  uint32_t ExidxCount() const { return exidx_count_; }
  uint32_t ErrnoAddr() const { return errno_addr_; }
  std::string Where();             // symbolicated PC, for crash reports
  const std::unordered_map<std::string, uint64_t>& CallCounts() const {
    return call_counts_;
  }

 private:
  bool MapMemory(std::string* err);
  bool ParseElf(std::string* err);
  void SetupDataSymbols();
  void SetupImportTraps();
  bool ApplyRelocations(std::string* err);
  void EnableVfp();
  uint32_t ResolveDynSym(uint32_t sym_index);
  void ServiceImport(uint32_t trap_addr);

  static void HookTrap(uc_engine*, uint64_t addr, uint32_t size, void* user);
  static bool HookBadMem(uc_engine*, uc_mem_type, uint64_t addr, int size,
                         int64_t value, void* user);

  uc_engine* uc_ = nullptr;
  std::vector<uint8_t> image_;

  struct Segment { uint32_t off, vaddr, filesz, memsz, flags; };
  std::vector<Segment> loads_;
  struct Section { uint32_t off, size, addr; };
  std::unordered_map<std::string, Section> sections_;

  struct DynSym { std::string name; uint32_t value; uint16_t shndx; uint8_t type; };
  std::vector<DynSym> dynsyms_;
  std::unordered_map<std::string, Sym> syms_;
  std::vector<std::pair<uint32_t, std::string>> sym_by_addr_;  // sorted

  std::vector<std::string> imports_;
  std::unordered_map<std::string, uint32_t> trap_of_;
  std::unordered_map<std::string, ShimFn> shims_;
  std::unordered_map<std::string, uint32_t> data_syms_;
  std::unordered_map<std::string, uint64_t> call_counts_;
  std::vector<std::string> unimplemented_;

  struct TrapRegion { uint32_t lo, hi; TrapFn fn; };
  std::vector<TrapRegion> trap_regions_;

  // heap: first-fit with a free list, matching the Python allocator
  struct Block { uint32_t size; bool free; };
  std::unordered_map<uint32_t, Block> blocks_;
  uint32_t heap_ptr_ = kHeapBase, heap_peak_ = 0;
  uint32_t scratch_ = 0, scratch_end_ = 0, errno_addr_ = 0;
  uint32_t exidx_base_ = 0, exidx_count_ = 0;
  uint32_t span_ = 0;

  // Java drives the guest from two threads: the GL thread (onTick, render)
  // and the AudioTrack thread (onAudioFrame). The library has no locks of its
  // own -- on 2012 hardware it got away with racing -- so we serialise entry
  // and give each thread its own guest stack rather than reproduce that luck.
  std::mutex mu_;
  uint32_t StackForThisThread();

  bool     pending_ = false;
  uint32_t pending_addr_ = 0;
  struct Fault { bool valid; const char* kind; uint64_t addr; int size; };
  Fault last_fault_{false, "", 0, 0};
};

// Installs the 99 libc/OS shims (shims.cpp).
void InstallShims(Runtime& rt, const std::string& files_dir);

}  // namespace shogun
