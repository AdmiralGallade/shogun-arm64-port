// jni_bridge.cpp -- the JNIEnv the guest sees, wired to the real one.
//
// The library does not only receive JNI calls, it makes them: Java hands it a
// JNIEnv*, and emulated code dereferences that to reach a table of function
// pointers. So we build a fake JNIEnv inside guest memory whose 233 slots are
// each a distinct trap address, and service them here against the real JNIEnv.
//
// Ported from runtime/jni.py, with one difference that matters: the desktop
// harness answered these from a mock JVM; this answers them from Dalvik.
#include "jni_bridge.h"

#include <android/log.h>

#include <cstring>
#include <vector>

namespace shogun {
namespace {
constexpr const char* kTag = "shogun";

// JNIEnv slot indices we actually service. Everything else aborts loudly with
// its index -- the scan of all 3,465 function bodies found ~20 in genuine use,
// and a few of those hits were false positives from unrelated pointer tables.
enum : uint32_t {
  kGetVersion = 4, kFindClass = 6,
  kExceptionOccurred = 15, kExceptionDescribe = 16, kExceptionClear = 17,
  kPushLocalFrame = 19, kPopLocalFrame = 20,
  kNewGlobalRef = 21, kDeleteGlobalRef = 22, kDeleteLocalRef = 23,
  kIsSameObject = 24, kNewLocalRef = 25, kEnsureLocalCapacity = 26,
  kGetObjectClass = 31, kIsInstanceOf = 32, kGetMethodID = 33,
  kCallObjectMethod = 34,          // .. kCallVoidMethodA = 63
  kCallVoidMethodA = 63,
  kGetFieldID = 94, kGetObjectField = 95, kGetIntField = 100,
  kGetStaticMethodID = 113,
  kCallStaticObjectMethod = 114, kCallStaticVoidMethodA = 143,
  kNewString = 163, kNewStringUTF = 167,
  kGetStringUTFLength = 168, kGetStringUTFChars = 169,
  kReleaseStringUTFChars = 170,
  kGetArrayLength = 171,
  kMonitorEnter = 217, kMonitorExit = 218,
  kGetPrimitiveArrayCritical = 222, kReleasePrimitiveArrayCritical = 223,
  kExceptionCheck = 228,
};

// Split a JNI signature into single-letter argument kinds.
std::vector<char> SigArgs(const std::string& sig) {
  std::vector<char> out;
  size_t i = sig.find('(');
  if (i == std::string::npos) return out;
  for (++i; i < sig.size() && sig[i] != ')'; ) {
    char c = sig[i];
    if (c == 'L') { i = sig.find(';', i) + 1; out.push_back('L'); }
    else if (c == '[') {
      while (i < sig.size() && sig[i] == '[') i++;
      if (i < sig.size() && sig[i] == 'L') i = sig.find(';', i) + 1; else i++;
      out.push_back('L');
    } else { out.push_back(c); i++; }
  }
  return out;
}

char SigReturn(const std::string& sig) {
  size_t i = sig.find(')');
  return (i == std::string::npos || i + 1 >= sig.size()) ? 'V' : sig[i + 1];
}
}  // namespace

JniBridge::JniBridge(Runtime& rt, JNIEnv* env, jobject activity)
    : rt_(rt) {
  env->GetJavaVM(&vm_);
  activity_ = env->NewGlobalRef(activity);

  // vtable: 233 slots, each a distinct trap address
  uc_mem_map(rt_.uc(), kJniData, 0x20000, UC_PROT_ALL);
  for (uint32_t i = 0; i < kNSlots; i++) {
    uint32_t t = kJniTrap + i * 4;
    rt_.Write(kJniData + i * 4, &t, 4);
  }
  env_guest_ = kJniData + 0x1000;
  uint32_t vt = kJniData;
  rt_.Write(env_guest_, &vt, 4);

  // JavaVM for JNI_OnLoad: JNIInvokeInterface is 8 slots, parked past the
  // JNIEnv table so one trap page serves both.
  uint32_t vmv = kJniData + 0x800;
  for (uint32_t i = 0; i < 8; i++) {
    uint32_t t = kJniTrap + (300 + i) * 4;
    rt_.Write(vmv + i * 4, &t, 4);
  }
  javavm_guest_ = kJniData + 0x1008;
  rt_.Write(javavm_guest_, &vmv, 4);

  arena_ = kJniData + 0x2000;
  arena_end_ = kJniData + 0x20000;

  rt_.AddTrapRegion(kJniTrap, 0x1000,
                    [this](Runtime&, uint32_t addr) { Dispatch(addr); },
                    /*already_mapped=*/false);
}

uint32_t JniBridge::Alloc(uint32_t n) {
  // caller holds mu_
  uint32_t a = arena_;
  arena_ = (arena_ + n + 7u) & ~7u;
  return arena_ >= arena_end_ ? 0 : a;
}

uint32_t JniBridge::Wrap(JNIEnv* env, jobject o) {
  if (!o) return 0;
  std::lock_guard<std::mutex> lock(mu_);
  uint32_t h = next_handle_;
  next_handle_ += 8;
  objs_[h] = env->NewGlobalRef(o);
  return h;
}

// Reuses one global ref (and one handle) for an object that recurs every
// frame, replacing it only if Java hands us a genuinely different object.
uint32_t JniBridge::WrapCached(JNIEnv* env, jobject o, Slot slot) {
  if (!o) return 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    Cached& c = cached_[slot];
    if (c.ref && env->IsSameObject(c.ref, o)) return c.handle;
    if (c.ref) { objs_.erase(c.handle); env->DeleteGlobalRef(c.ref); c.ref = nullptr; }
  }
  uint32_t h = Wrap(env, o);          // takes mu_ itself
  std::lock_guard<std::mutex> lock(mu_);
  Cached& c = cached_[slot];
  c.handle = h;
  c.ref = objs_[h];
  return h;
}

uint32_t JniBridge::WrapFdObject(int fd) {
  std::lock_guard<std::mutex> lock(mu_);
  uint32_t h = next_handle_;
  next_handle_ += 8;
  fd_objs_[h] = fd;
  return h;
}

jobject JniBridge::Unwrap(uint32_t h) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = objs_.find(h);
  return it == objs_.end() ? nullptr : it->second;
}

JNIEnv* JniBridge::Env() {
  JNIEnv* e = nullptr;
  if (vm_->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) != JNI_OK)
    vm_->AttachCurrentThread(&e, nullptr);
  return e;
}

// Collect arguments for a call, either from registers (Call...Method) or from
// a guest va_list (Call...MethodV). Both need the signature to know widths.
void JniBridge::GatherArgs(const std::string& sig, bool from_valist,
                           std::vector<jvalue>* out) {
  auto kinds = SigArgs(sig);
  if (from_valist) {
    uint32_t p = rt_.Arg(3);
    for (char k : kinds) {
      jvalue v{};
      if (k == 'J' || k == 'D') {
        p = (p + 7u) & ~7u;
        uint32_t lo = 0, hi = 0;
        rt_.Read(p, &lo, 4); rt_.Read(p + 4, &hi, 4); p += 8;
        uint64_t bits = lo | (static_cast<uint64_t>(hi) << 32);
        if (k == 'J') v.j = static_cast<jlong>(bits);
        else std::memcpy(&v.d, &bits, 8);
      } else {
        uint32_t w = 0;
        rt_.Read(p, &w, 4); p += 4;
        if (k == 'L') v.l = Unwrap(w);
        else if (k == 'F') std::memcpy(&v.f, &w, 4);
        else if (k == 'Z') v.z = w ? JNI_TRUE : JNI_FALSE;
        else v.i = static_cast<jint>(w);
      }
      out->push_back(v);
    }
  } else {
    int i = 3;                       // env, obj, methodID, then varargs
    for (char k : kinds) {
      jvalue v{};
      if (k == 'J' || k == 'D') {
        if (i % 2) i++;
        uint64_t bits = rt_.Arg(i) | (static_cast<uint64_t>(rt_.Arg(i + 1)) << 32);
        i += 2;
        if (k == 'J') v.j = static_cast<jlong>(bits);
        else std::memcpy(&v.d, &bits, 8);
      } else {
        uint32_t w = rt_.Arg(i++);
        if (k == 'L') v.l = Unwrap(w);
        else if (k == 'F') std::memcpy(&v.f, &w, 4);
        else if (k == 'Z') v.z = w ? JNI_TRUE : JNI_FALSE;
        else v.i = static_cast<jint>(w);
      }
      out->push_back(v);
    }
  }
}

void JniBridge::DoCall(uint32_t slot, bool is_static, bool from_valist) {
  JNIEnv* env = Env();
  uint32_t objh = rt_.Arg(1), midh = rt_.Arg(2);
  auto it = methods_.find(midh);
  if (it == methods_.end()) { rt_.Ret(0); return; }
  const MethodInfo& mi = it->second;
  std::vector<jvalue> args;
  GatherArgs(mi.sig, from_valist, &args);
  jobject self = Unwrap(objh);
  const jvalue* a = args.empty() ? nullptr : args.data();

  switch (SigReturn(mi.sig)) {
    case 'V':
      if (is_static) env->CallStaticVoidMethodA(static_cast<jclass>(self), mi.id, a);
      else           env->CallVoidMethodA(self, mi.id, a);
      rt_.Ret(0);
      break;
    case 'Z': {
      jboolean r = is_static
          ? env->CallStaticBooleanMethodA(static_cast<jclass>(self), mi.id, a)
          : env->CallBooleanMethodA(self, mi.id, a);
      rt_.Ret(r ? 1 : 0);
      break;
    }
    case 'I': case 'B': case 'C': case 'S': {
      jint r = is_static
          ? env->CallStaticIntMethodA(static_cast<jclass>(self), mi.id, a)
          : env->CallIntMethodA(self, mi.id, a);
      rt_.Ret(static_cast<uint32_t>(r));
      break;
    }
    case 'J': {
      jlong r = is_static
          ? env->CallStaticLongMethodA(static_cast<jclass>(self), mi.id, a)
          : env->CallLongMethodA(self, mi.id, a);
      rt_.Ret64(static_cast<uint64_t>(r));
      break;
    }
    case 'F': {
      jfloat r = is_static
          ? env->CallStaticFloatMethodA(static_cast<jclass>(self), mi.id, a)
          : env->CallFloatMethodA(self, mi.id, a);
      uint32_t bits; std::memcpy(&bits, &r, 4); rt_.Ret(bits);
      break;
    }
    case 'D': {
      jdouble r = is_static
          ? env->CallStaticDoubleMethodA(static_cast<jclass>(self), mi.id, a)
          : env->CallDoubleMethodA(self, mi.id, a);
      rt_.RetD(r);
      break;
    }
    default: {   // object / array
      jobject r = is_static
          ? env->CallStaticObjectMethodA(static_cast<jclass>(self), mi.id, a)
          : env->CallObjectMethodA(self, mi.id, a);
      rt_.Ret(Wrap(env, r));
      if (r) env->DeleteLocalRef(r);
      break;
    }
  }
}

void JniBridge::Dispatch(uint32_t addr) {
  uint32_t slot = (addr - kJniTrap) / 4;
  // note: Unwrap/Wrap below take mu_ individually; never held across a
  // re-entry into the guest, so no lock inversion with the runtime mutex.
  JNIEnv* env = Env();

  // ---- JavaVM (JNIInvokeInterface) --------------------------------------
  if (slot >= 300) {
    uint32_t which = slot - 300;
    if (which == 6 /*GetEnv*/ || which == 4 /*AttachCurrentThread*/ ||
        which == 7 /*AttachCurrentThreadAsDaemon*/) {
      rt_.Write(rt_.Arg(1), &env_guest_, 4);
    }
    rt_.Ret(0);
    return;
  }

  switch (slot) {
    case kGetVersion:            rt_.Ret(JNI_VERSION_1_6); return;
    case kExceptionCheck:        rt_.Ret(env->ExceptionCheck() ? 1 : 0); return;
    case kExceptionOccurred:     rt_.Ret(0); return;
    case kExceptionClear:        env->ExceptionClear(); rt_.Ret(0); return;
    case kExceptionDescribe:     env->ExceptionDescribe(); rt_.Ret(0); return;
    case kDeleteLocalRef:
    case kDeleteGlobalRef:
    case kEnsureLocalCapacity:
    case kPushLocalFrame:
    case kPopLocalFrame:
    case kMonitorEnter:
    case kMonitorExit:           rt_.Ret(0); return;
    case kNewGlobalRef:
    case kNewLocalRef:           rt_.Ret(rt_.Arg(1)); return;
    case kIsSameObject:          rt_.Ret(rt_.Arg(1) == rt_.Arg(2) ? 1 : 0); return;
    case kIsInstanceOf:          rt_.Ret(1); return;

    case kFindClass: {
      std::string n = rt_.CStr(rt_.Arg(1));
      // The engine hands over a dotted name; JNI FindClass wants slashes.
      for (auto& ch : n) if (ch == '.') ch = '/';
      jclass c = env->FindClass(n.c_str());
      if (!c) { env->ExceptionClear();
                __android_log_print(ANDROID_LOG_WARN, kTag,
                                    "FindClass failed: %s", n.c_str()); }
      rt_.Ret(Wrap(env, c));
      if (c) env->DeleteLocalRef(c);
      return;
    }
    case kGetObjectClass: {
      jobject o = Unwrap(rt_.Arg(1));
      jclass c = o ? env->GetObjectClass(o) : nullptr;
      rt_.Ret(Wrap(env, c));
      if (c) env->DeleteLocalRef(c);
      return;
    }
    case kGetMethodID:
    case kGetStaticMethodID: {
      jclass c = static_cast<jclass>(Unwrap(rt_.Arg(1)));
      std::string nm = rt_.CStr(rt_.Arg(2)), sig = rt_.CStr(rt_.Arg(3));
      jmethodID id = nullptr;
      if (c) {
        id = (slot == kGetMethodID) ? env->GetMethodID(c, nm.c_str(), sig.c_str())
                                    : env->GetStaticMethodID(c, nm.c_str(), sig.c_str());
      }
      if (!id) { env->ExceptionClear();
                 __android_log_print(ANDROID_LOG_WARN, kTag,
                                     "GetMethodID failed: %s%s", nm.c_str(), sig.c_str()); }
      uint32_t h = next_handle_; next_handle_ += 8;
      methods_[h] = {id, sig, nm};
      rt_.Ret(id ? h : 0);
      return;
    }
    case kGetFieldID: {
      jclass c = static_cast<jclass>(Unwrap(rt_.Arg(1)));
      std::string nm = rt_.CStr(rt_.Arg(2)), sig = rt_.CStr(rt_.Arg(3));
      jfieldID f = c ? env->GetFieldID(c, nm.c_str(), sig.c_str()) : nullptr;
      if (!f) env->ExceptionClear();
      uint32_t h = next_handle_; next_handle_ += 8;
      fields_[h] = f;
      rt_.Ret(f ? h : 0);
      return;
    }
    case kGetIntField: {
      // java.io.FileDescriptor.descriptor, for the asset pack
      auto fd = fd_objs_.find(rt_.Arg(1));
      if (fd != fd_objs_.end()) {
        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "GetIntField -> fd %d", fd->second);
        rt_.Ret(static_cast<uint32_t>(fd->second));
        return;
      }
      jobject o = Unwrap(rt_.Arg(1));
      auto it = fields_.find(rt_.Arg(2));
      rt_.Ret((o && it != fields_.end() && it->second)
                  ? static_cast<uint32_t>(env->GetIntField(o, it->second)) : 0);
      return;
    }
    case kGetObjectField: {
      jobject o = Unwrap(rt_.Arg(1));
      auto it = fields_.find(rt_.Arg(2));
      rt_.Ret((o && it != fields_.end() && it->second)
                  ? Wrap(env, env->GetObjectField(o, it->second)) : 0);
      return;
    }

    // ---- strings ---------------------------------------------------------
    case kNewStringUTF: {
      std::string s = rt_.CStr(rt_.Arg(1));
      rt_.Ret(Wrap(env, env->NewStringUTF(s.c_str())));
      return;
    }
    case kNewString:  rt_.Ret(Wrap(env, env->NewStringUTF(""))); return;
    case kGetStringUTFLength: {
      auto s = static_cast<jstring>(Unwrap(rt_.Arg(1)));
      rt_.Ret(s ? static_cast<uint32_t>(env->GetStringUTFLength(s)) : 0);
      return;
    }
    case kGetStringUTFChars: {
      auto s = static_cast<jstring>(Unwrap(rt_.Arg(1)));
      if (!s) { rt_.Ret(0); return; }
      const char* c = env->GetStringUTFChars(s, nullptr);
      size_t n = std::strlen(c) + 1;
      uint32_t p = Alloc(static_cast<uint32_t>(n));
      if (p) rt_.Write(p, c, n);
      env->ReleaseStringUTFChars(s, c);
      if (rt_.Arg(2)) { uint32_t one = 1; rt_.Write(rt_.Arg(2), &one, 4); }
      rt_.Ret(p);
      return;
    }
    case kReleaseStringUTFChars: rt_.Ret(0); return;

    // ---- arrays ----------------------------------------------------------
    case kGetArrayLength: {
      auto a = static_cast<jarray>(Unwrap(rt_.Arg(1)));
      rt_.Ret(a ? static_cast<uint32_t>(env->GetArrayLength(a)) : 0);
      return;
    }
    case kGetPrimitiveArrayCritical: {
      auto a = static_cast<jbyteArray>(Unwrap(rt_.Arg(1)));
      if (!a) { rt_.Ret(0); return; }
      jsize n = env->GetArrayLength(a);
      // One staging buffer per array, reused. The audio thread asks for the
      // same array every frame; allocating per call exhausts the arena within
      // seconds of playback.
      std::unique_lock<std::mutex> lk(mu_);
      uint32_t& p = crit_[rt_.Arg(1)];
      if (!p) p = Alloc(static_cast<uint32_t>(n));
      lk.unlock();
      if (p) {
        void* src = env->GetPrimitiveArrayCritical(a, nullptr);
        if (src) { rt_.Write(p, src, n);
                   env->ReleasePrimitiveArrayCritical(a, src, JNI_ABORT); }
      }
      crit_len_[p] = n;
      crit_obj_[p] = rt_.Arg(1);
      if (rt_.Arg(2)) { uint32_t zero = 0; rt_.Write(rt_.Arg(2), &zero, 4); }
      rt_.Ret(p);
      return;
    }
    case kReleasePrimitiveArrayCritical: {
      uint32_t p = rt_.Arg(2);
      auto lo = crit_len_.find(p), oo = crit_obj_.find(p) == crit_obj_.end()
                                            ? crit_len_.end() : crit_len_.find(p);
      (void)oo;
      if (lo != crit_len_.end()) {
        auto a = static_cast<jbyteArray>(Unwrap(crit_obj_[p]));
        if (a) {
          std::vector<uint8_t> buf(lo->second);
          rt_.Read(p, buf.data(), buf.size());
          void* dst = env->GetPrimitiveArrayCritical(a, nullptr);
          if (dst) { std::memcpy(dst, buf.data(), buf.size());
                     env->ReleasePrimitiveArrayCritical(a, dst, 0); }
        }
      }
      rt_.Ret(0);
      return;
    }
    default: break;
  }

  // ---- the Call<Type>Method families -------------------------------------
  if (slot >= kCallObjectMethod && slot <= kCallVoidMethodA) {
    uint32_t v = (slot - kCallObjectMethod) % 3;      // 0=..., 1=V, 2=A
    DoCall(slot, /*is_static=*/false, /*from_valist=*/v == 1);
    return;
  }
  if (slot >= kCallStaticObjectMethod && slot <= kCallStaticVoidMethodA) {
    uint32_t v = (slot - kCallStaticObjectMethod) % 3;
    DoCall(slot, /*is_static=*/true, /*from_valist=*/v == 1);
    return;
  }

  // Unimplemented slots abort loudly with their index rather than returning
  // zero -- a silent 0 turns into a null dereference deep inside the engine.
  __android_log_print(ANDROID_LOG_ERROR, kTag,
                      "UNIMPLEMENTED JNIEnv slot %u (trap 0x%08x)", slot, addr);
  rt_.Ret(0);
}

}  // namespace shogun
