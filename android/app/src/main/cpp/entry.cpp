// entry.cpp -- what Java calls, and how it reaches the guest.
//
// The Java shell mirrors net.int13.HalActivity, whose interface was read out
// of the original classes.dex. Each native method here forwards into the
// corresponding entry point inside the emulated 2012 library, using the fake
// JNIEnv the bridge installed in guest memory.
#include <android/log.h>
#include <dirent.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>

#include <memory>
#include <string>
#include <vector>

#include "jni_bridge.h"
#include "shogun_runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "shogun", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "shogun", __VA_ARGS__)

namespace {

struct App {
  shogun::Runtime rt;
  std::unique_ptr<shogun::JniBridge> jni;
  bool booted = false;
  int  width = 480, height = 800;
  FILE* pak = nullptr;
  std::string files_dir;
  uint64_t ticks = 0, audio_calls = 0;
  double tick_ms = 0, audio_ms = 0;      // rolling sums, reset when logged
  bool audio_armed = false;
};

static double NowMs() {
  struct timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
App* g_app = nullptr;

// The app is not debuggable, so adb cannot look inside its files dir. Log what
// is actually on disk instead -- otherwise "did it save?" is unanswerable.
void ListSaves() {
  if (!g_app) return;
  DIR* d = opendir(g_app->files_dir.c_str());
  if (!d) { LOGE("cannot open %s", g_app->files_dir.c_str()); return; }
  struct dirent* e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    std::string full = g_app->files_dir + "/" + e->d_name;
    struct stat st{};
    if (stat(full.c_str(), &st) == 0)
      LOGI("  save dir: %-24s %lld bytes", e->d_name,
           static_cast<long long>(st.st_size));
  }
  closedir(d);
}

// Every call into the guest goes through here so one failure path reports a
// symbolicated PC instead of a bare signal.
bool GuestCall(const char* sym, const std::vector<uint32_t>& args,
               uint32_t* out = nullptr) {
  std::string err;
  if (!g_app->rt.CallSym(sym, args, out, &err)) {
    LOGE("guest %s failed: %s", sym, err.c_str());
    return false;
  }
  return true;
}

}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_net_int13_shogun_ShogunNative_nativeInit(
    JNIEnv* env, jclass, jobject activity, jbyteArray lib, jstring pak_path,
    jlong pak_len, jstring files_dir, jint w, jint h) {
  delete g_app;
  g_app = new App();
  g_app->width = w;
  g_app->height = h;

  // 1. load the unmodified 2012 library out of the APK's assets
  jsize n = env->GetArrayLength(lib);
  std::vector<uint8_t> image(n);
  env->GetByteArrayRegion(lib, 0, n, reinterpret_cast<jbyte*>(image.data()));
  std::string err;
  if (!g_app->rt.Load(image.data(), image.size(), &err)) {
    LOGE("load failed: %s", err.c_str());
    return JNI_FALSE;
  }
  LOGI("loaded libHAL.Android.so: %d bytes, %zu imports",
       n, g_app->rt.imports().size());

  // 2. host side of the boundary
  const char* fd_c = env->GetStringUTFChars(files_dir, nullptr);
  std::string files(fd_c ? fd_c : "/data/local/tmp");
  env->ReleaseStringUTFChars(files_dir, fd_c);
  g_app->files_dir = files;
  shogun::InstallShims(g_app->rt, files);
  shogun::InstallGl(g_app->rt);
  shogun::InstallHardMode(g_app->rt, files);
  shogun::InstallLeaderboard(g_app->rt);
  g_app->jni = std::make_unique<shogun::JniBridge>(g_app->rt, env, activity);

  // 3. the asset pack, as a standalone file the engine can seek freely
  const char* pp = env->GetStringUTFChars(pak_path, nullptr);
  g_app->pak = std::fopen(pp, "rb");
  LOGI("asset pack %s -> %s (%lld bytes)", pp, g_app->pak ? "open" : "FAILED",
       static_cast<long long>(pak_len));
  env->ReleaseStringUTFChars(pak_path, pp);
  if (!g_app->pak) return JNI_FALSE;
  shogun::RegisterAssetFd(7, g_app->pak);

  // 4. boot in the order a device would
  uint32_t envg = g_app->jni->env_guest();
  uint32_t vmg  = g_app->jni->javavm_guest();
  uint32_t self = g_app->jni->Wrap(env, activity);

  if (!GuestCall("JNI_OnLoad", {vmg, 0})) return JNI_FALSE;

  // Trailing slash, and it matters: the engine forms every save path as
  // <this string> + <file name>, with no separator of its own. Without it
  // ".../files" + "settings.sav" became ".../filessettings.sav" and nothing
  // the game tried to write could be opened.
  const std::string dir_arg = files + "/";
  uint32_t lang  = g_app->jni->Wrap(env, env->NewStringUTF("en"));
  uint32_t fdir  = g_app->jni->Wrap(env, env->NewStringUTF(dir_arg.c_str()));
  uint32_t sdir  = g_app->jni->Wrap(env, env->NewStringUTF(dir_arg.c_str()));
  if (!GuestCall("Java_net_int13_HalActivity_onInit",
                 {envg, self, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                  0, lang, fdir, sdir}))
    return JNI_FALSE;

  // onArchiveInit(FileDescriptor, jlong off, jlong len, jstring path).
  // Two jlongs: AAPCS puts each in an even-aligned register pair, so r3 is
  // skipped as padding and both land on the stack.
  uint32_t fdobj = g_app->jni->WrapFdObject(7);
  std::vector<uint32_t> a = {
      envg, self, fdobj,
      0,                                            // padding for alignment
      0, 0,                                         // offset 0
      static_cast<uint32_t>(pak_len), static_cast<uint32_t>(pak_len >> 32),
      fdir};
  if (!GuestCall("Java_net_int13_HalActivity_onArchiveInit", a)) return JNI_FALSE;

  g_app->booted = true;
  LOGI("engine booted; heap peak %u KB", g_app->rt.HeapPeak() / 1024);
  return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativeTick(JNIEnv* env, jclass) {
  if (!g_app || !g_app->booted) return;
  uint32_t self = g_app->jni->WrapCached(env, g_app->jni->activity(),
                                        shogun::JniBridge::kSlotActivity);
  double t0 = NowMs();
  if (!GuestCall("Java_net_int13_HalActivity_onTick",
                 {g_app->jni->env_guest(), self}))
    g_app->booted = false;         // stop hammering a dead engine
  g_app->tick_ms += NowMs() - t0;
  shogun::HardModeTick(++g_app->ticks);
  shogun::LeaderboardTick(g_app->ticks);
  if (g_app->ticks % 120 == 0) {
    // 512 samples at 22050 Hz is 23.2 ms of audio: if a tick routinely costs
    // more than that, the mixer cannot keep the track fed and you hear it.
    LOGI("tick %llu  draws %llu  heap %u KB  |  tick %.1f ms  audio %.2f ms x%llu",
         static_cast<unsigned long long>(g_app->ticks),
         static_cast<unsigned long long>(shogun::GlDrawCount()),
         g_app->rt.HeapPeak() / 1024,
         g_app->tick_ms / 120.0,
         g_app->audio_calls ? g_app->audio_ms / g_app->audio_calls : 0.0,
         static_cast<unsigned long long>(g_app->audio_calls));
    g_app->tick_ms = 0; g_app->audio_ms = 0; g_app->audio_calls = 0;
    // Ask the engine what it believes its own audio state is. The mixer
    // early-outs on a "playback armed" flag, so silence with a healthy track
    // means the engine never started any music -- not that we lost the PCM.
    uint32_t music = 0, chans = 0, mvol = 0, avol = 0;
    std::string e;
    g_app->rt.CallSym("UE_isMusicPlaying", {}, &music, &e);
    g_app->rt.CallSym("UE_GetNbPlayingChannels", {}, &chans, &e);
    g_app->rt.CallSym("UE_GetMusicVolume", {}, &mvol, &e);
    g_app->rt.CallSym("UE_GetAudioVolume", {}, &avol, &e);
    LOGI("GL: %s", shogun::GlDiagnostics(g_app->rt).c_str());
    LOGI("engine audio: musicPlaying=%u channels=%u musicVol=%u masterVol=%u",
         music, chans, mvol, avol);
    // The mixer early-outs on a "playback armed" flag that HAL_StartAudioPlaying
    // sets. The engine reaches that call on the desktop harness but not here,
    // so once it reports music playing we arm the output path ourselves --
    // otherwise onAudioFrame returns silence forever.
    if (!g_app->audio_armed && music) {
      uint32_t r = 0;
      std::string e2;
      if (g_app->rt.CallSym("HAL_StartAudioPlaying", {}, &r, &e2)) {
        g_app->audio_armed = true;
        LOGI("armed audio output via HAL_StartAudioPlaying");
      } else {
        LOGE("HAL_StartAudioPlaying failed: %s", e2.c_str());
      }
    }
  }
}

JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativeTouch(JNIEnv* env, jclass, jint action,
                                               jint x, jint y) {
  if (!g_app || !g_app->booted) return;
  const char* sym = action == 0 ? "Java_net_int13_HalActivity_onTouchPressed"
                  : action == 1 ? "Java_net_int13_HalActivity_onTouchMove"
                                : "Java_net_int13_HalActivity_onTouchReleased";
  uint32_t self = g_app->jni->WrapCached(env, g_app->jni->activity(),
                                        shogun::JniBridge::kSlotActivity);
  GuestCall(sym, {g_app->jni->env_guest(), self,
                  static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
}

JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativeAudio(JNIEnv* env, jclass,
                                               jbyteArray buf, jint samples) {
  if (!g_app || !g_app->booted) return;
  // Both of these recur every audio frame, so they must reuse one handle.
  // Wrapping fresh each time leaked a global ref and, worse, allocated a new
  // GetPrimitiveArrayCritical staging buffer per frame until the JNI arena ran
  // out -- which is why sound stopped after about a second.
  uint32_t self  = g_app->jni->WrapCached(env, g_app->jni->activity(),
                                          shogun::JniBridge::kSlotActivity);
  uint32_t hbuf  = g_app->jni->WrapCached(env, buf,
                                          shogun::JniBridge::kSlotAudioBuf);
  double t0 = NowMs();
  GuestCall("Java_net_int13_HalActivity_onAudioFrame",
            {g_app->jni->env_guest(), self, hbuf,
             static_cast<uint32_t>(samples)});
  g_app->audio_ms += NowMs() - t0;
  g_app->audio_calls++;
}

// ---- leaderboard bridge --------------------------------------------------
// Java owns the HTTPS call; native owns everything that touches the guest.

JNIEXPORT jstring JNICALL
Java_net_int13_shogun_ShogunNative_nativeLeaderboardPending(JNIEnv* env, jclass) {
  const std::string s = shogun::LeaderboardPending();
  return s.empty() ? nullptr : env->NewStringUTF(s.c_str());
}

JNIEXPORT jstring JNICALL
Java_net_int13_shogun_ShogunNative_nativeLeaderboardName(JNIEnv* env, jclass) {
  const std::string s = shogun::LeaderboardPlayerName();
  return s.empty() ? nullptr : env->NewStringUTF(s.c_str());
}

JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativeLeaderboardResult(
    JNIEnv* env, jclass, jstring board, jint your_best,
    jint w_rank, jint w_best, jstring w_name,
    jint c_rank, jint c_best, jstring c_name,
    jint t_rank, jint t_best, jstring t_name,
    jstring country, jstring city) {
  auto get = [&](jstring j) -> std::string {
    if (!j) return std::string();
    const char* p = env->GetStringUTFChars(j, nullptr);
    std::string s(p ? p : "");
    env->ReleaseStringUTFChars(j, p);
    return s;
  };
  const std::string b = get(board), wn = get(w_name), cn = get(c_name),
                    tn = get(t_name), co = get(country), ci = get(city);
  shogun::LeaderboardResult(
      b.c_str(), static_cast<uint32_t>(your_best),
      static_cast<uint32_t>(w_rank), static_cast<uint32_t>(w_best), wn.c_str(),
      static_cast<uint32_t>(c_rank), static_cast<uint32_t>(c_best), cn.c_str(),
      static_cast<uint32_t>(t_rank), static_cast<uint32_t>(t_best), tn.c_str(),
      co.c_str(), ci.c_str());
}

JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativePause(JNIEnv* env, jclass) {
  if (!g_app || !g_app->booted) return;
  uint32_t self = g_app->jni->WrapCached(env, g_app->jni->activity(),
                                        shogun::JniBridge::kSlotActivity);
  GuestCall("Java_net_int13_HalActivity_onApplicationPause",
            {g_app->jni->env_guest(), self});
}

JNIEXPORT jboolean JNICALL
Java_net_int13_shogun_ShogunNative_nativeBooted(JNIEnv*, jclass) {
  return (g_app && g_app->booted) ? JNI_TRUE : JNI_FALSE;
}

// The surface came back with a context the driver refused to preserve. The
// guest is untouched -- it still holds the same names -- but every real GL
// object behind those names is gone, so rebuild them before the next tick.
JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativeSurfaceRecreated(JNIEnv*, jclass) {
  if (!g_app || !g_app->booted) return;
  shogun::GlContextLost();
}

// Flush anything the engine holds in memory to disk. onApplicationPause is its
// only shutdown hook: nothing else writes the save file, so a process that
// exits without reaching this loses the session.
JNIEXPORT void JNICALL
Java_net_int13_shogun_ShogunNative_nativeSave(JNIEnv* env, jclass) {
  if (!g_app || !g_app->booted) return;
  // The writing itself is done by onApplicationPause, the engine's own
  // shutdown hook; calling the internal pool writer directly produced entries
  // with garbage names, so this only reports what landed on disk.
  ListSaves();
}

}  // extern "C"
