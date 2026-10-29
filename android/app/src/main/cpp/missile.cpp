// missile.cpp -- the M (missile) and E (extinguishing coilgun) weapons.
//
// The weapons themselves are guest code: missile/missile.c, compiled for the
// same ARMv5TE Thumb the game uses and linked at kMissileBase.
// missile/build.py turns it into missile_blob.h -- the blob, the list of `bl`
// instructions to redirect into it, and the buttons' art.
//
// This only puts those in place, before the engine runs a single instruction:
// map the blob, rewrite the call sites, and leave the buttons where the
// engine's '*' paths look (the files directory it was handed in onInit). The
// desktop harness does exactly the same from runtime/missile.py, which is how
// the weapon was developed and checked.
//
// Every site is checked against the bytes the build saw. A library that does
// not match is left entirely untouched: half-patched would be worse than none.
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "missile_blob.h"
#include "shogun_runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "shogun", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "shogun", __VA_ARGS__)

namespace shogun {
bool InstallMissile(Runtime& rt, const std::string& files_dir) {
  using namespace missile;

  for (const Patch& p : kPatches) {
    uint8_t have[4] = {0, 0, 0, 0};
    if (!rt.Read(p.addr, have, p.len) || std::memcmp(have, p.old, p.len) != 0) {
      LOGW("missile: 0x%08x is not what the build expected -- not installing",
           p.addr);
      return false;
    }
  }

  if (uc_mem_map(rt.uc(), kMissileBase, kMissileSize, UC_PROT_ALL) !=
      UC_ERR_OK) {
    LOGW("missile: could not map 0x%08x", kMissileBase);
    return false;
  }
  rt.Write(kMissileBase, kBlob, sizeof kBlob);
  for (const Patch& p : kPatches) rt.Write(p.addr, p.now, p.len);

  // Rewritten on every start, so an update to the art always lands.
  for (const Icon& ic : kIcons) {
    const std::string path = files_dir + "/" + ic.name;
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
      std::fwrite(ic.data, 1, ic.size, f);
      std::fclose(f);
    } else {
      LOGW("missile: could not write %s; that button will be blank",
           path.c_str());
    }
  }

  LOGI("missile: M and E installed (%zu-byte blob @0x%08x, %zu patches)",
       sizeof kBlob, kMissileBase, sizeof kPatches / sizeof kPatches[0]);
  return true;
}

}  // namespace shogun
