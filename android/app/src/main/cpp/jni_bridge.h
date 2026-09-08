// jni_bridge.h -- fake JNIEnv in guest memory, serviced against the real one.
#pragma once

#include <jni.h>

#include <mutex>

#include <string>
#include <unordered_map>
#include <vector>

#include "shogun_runtime.h"

namespace shogun {

constexpr uint32_t kNSlots = 233;   // JNINativeInterface entries

class JniBridge {
 public:
  JniBridge(Runtime& rt, JNIEnv* env, jobject activity);

  uint32_t env_guest()    const { return env_guest_; }
  uint32_t javavm_guest() const { return javavm_guest_; }
  jobject  activity()     const { return activity_; }

  // Wraps a real jobject as a 32-bit handle the guest can hold.
  // Every Wrap takes a global ref, so anything called per frame MUST go
  // through WrapCached -- wrapping the activity per tick overflowed the
  // global reference table (50,688 refs to one object) and killed the process.
  uint32_t Wrap(JNIEnv* env, jobject o);
  enum Slot { kSlotActivity = 0, kSlotAudioBuf = 1, kSlotCount = 4 };
  uint32_t WrapCached(JNIEnv* env, jobject o, Slot slot);

  // A stand-in for java.io.FileDescriptor. The engine only ever reads the
  // integer out of it via GetIntField, so this hands back a handle that does
  // exactly that -- passing the raw fd as if it were an object made
  // GetIntField unwrap nothing and report fd 0, and the archive never opened.
  uint32_t WrapFdObject(int fd);
  jobject  Unwrap(uint32_t handle);
  JNIEnv*  Env();

 private:
  struct MethodInfo { jmethodID id; std::string sig; std::string name; };

  void Dispatch(uint32_t trap_addr);
  void DoCall(uint32_t slot, bool is_static, bool from_valist);
  void GatherArgs(const std::string& sig, bool from_valist, std::vector<jvalue>* out);
  uint32_t Alloc(uint32_t n);

  // Java drives this from two threads: the GL thread (onTick) and the
  // AudioTrack thread (onAudioFrame). Both wrap objects *before* entering the
  // guest, so the runtime lock does not cover them. Racing here corrupted the
  // handle cache, which made every audio frame allocate a new staging buffer
  // until the arena ran dry -- audio that works for a second, then stops.
  // Held only around bookkeeping, never across a call into the guest, so it
  // cannot invert against the runtime lock.
  std::mutex mu_;

  Runtime& rt_;
  JavaVM*  vm_ = nullptr;
  jobject  activity_ = nullptr;

  uint32_t env_guest_ = 0, javavm_guest_ = 0;
  uint32_t arena_ = 0, arena_end_ = 0, next_handle_ = 0x2000;

  std::unordered_map<uint32_t, jobject>    objs_;
  struct Cached { jobject ref = nullptr; uint32_t handle = 0; };
  Cached cached_[kSlotCount];
  std::unordered_map<uint32_t, MethodInfo> methods_;
  std::unordered_map<uint32_t, jfieldID>   fields_;
  std::unordered_map<uint32_t, int>        fd_objs_;   // handle -> fd
  // GetPrimitiveArrayCritical staging, reused per array (see the .cpp).
  std::unordered_map<uint32_t, uint32_t> crit_;      // array handle -> guest buf
  std::unordered_map<uint32_t, uint32_t> crit_len_;  // guest buf -> length
  std::unordered_map<uint32_t, uint32_t> crit_obj_;  // guest buf -> array handle
};

// Forwards the guest's GLES 1.x calls to the device driver (gl.cpp).
void InstallGl(Runtime& rt);
uint64_t GlDrawCount();
std::string GlDiagnostics(Runtime& rt);
// Rebuild every GL object after Android destroyed the EGL context.
void GlContextLost();
// Lets the JNI layer hand the asset-pack descriptor to fdopen (shims.cpp).
void RegisterAssetFd(int fd, FILE* f);

// Difficulty control and its settings line (hardmode.cpp).
void InstallHardMode(Runtime& rt, const std::string& files_dir);
void HardModeTick(uint64_t ticks);
bool HardModeEnabled();
void SetHardMode(bool on);

}  // namespace shogun
