// entry.cpp -- what Java calls, and how it reaches the guest.
//
// The Java shell mirrors net.int13.HalActivity, whose interface was read out
// of the original classes.dex. Each native method here forwards into the
// corresponding entry point inside the emulated 2012 library, using the fake
// JNIEnv the bridge installed in guest memory.
#include <android/log.h>
#include <dirent.h>
#include <sched.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jni_bridge.h"
#include "shogun_runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "shogun", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "shogun", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "shogun", __VA_ARGS__)

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
  // How evenly frames are actually being shown, measured on the host side.
  // The average tick cost tells you whether the emulator can keep up; it says
  // nothing about whether frames arrive at a steady rate, which is what
  // stutter is. So keep the worst gap as well as the mean -- an average over
  // 120 frames hides precisely the spike you are hunting.
  double last_tick_at = 0, gap_sum = 0, gap_max = 0;
  uint64_t gaps = 0, gaps_late = 0;
  uint64_t clock_queries_at_log = 0;
  uint64_t lock_wait_at_log = 0, exec_at_log = 0;
  double tick_exec_ms = 0, audio_exec_ms = 0;   // emulation only, no lock wait
  double worst_tick_exec_ms = 0;
  uint64_t stalls = 0; double stall_ms = 0;     // gaps over half a second
  std::unordered_map<std::string, uint64_t> calls_at_log;
  uint64_t unpacks = 0; double unpack_bytes = 0;
  uint64_t traps_fast_at_log = 0, traps_slow_at_log = 0;
  double diag_ms = 0;            // cost of the previous window's own logging
  // What the frame before this one actually spent its time on. Averages over
  // 120 frames cannot show a single jolt, and a single jolt is what gets felt.
  double prev_exec_ms = 0, prev_lock_ms = 0, prev_gl_ms = 0;
  uint64_t lw_at_tick = 0, gl_at_tick = 0;
  uint64_t jolts = 0;
  bool audio_armed = false;
};

// Keep the emulator off the little cores.
//
// Measured on a Pixel 7: the render thread was migrating across all three
// clusters -- including the A55s -- and landing on cores sitting at 553 MHz,
// against an X1 that can run at 2850. That is a 5x clock swing on the one
// thread that has a frame deadline, and it is felt as the game speeding up and
// slowing down. The scheduler is not wrong to do it: guest execution is
// serialised, so each thread blocks for much of every frame and looks small.
//
// The fastest cluster is found by asking the kernel rather than hardcoding
// core numbers, which differ between devices.
void PinToFastestCores(const char* who) {
  static std::vector<int> fast;
  static std::once_flag once;
  std::call_once(once, [] {
    std::vector<std::pair<int, long>> all;
    long best = 0;
    for (int c = 0; c < 32; c++) {
      char path[128];
      std::snprintf(path, sizeof path,
                    "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", c);
      FILE* f = std::fopen(path, "r");
      if (!f) continue;
      long v = 0;
      if (std::fscanf(f, "%ld", &v) == 1 && v > 0) {
        all.emplace_back(c, v);
        if (v > best) best = v;
      }
      std::fclose(f);
    }
    for (const auto& kv : all)
      if (kv.second == best) fast.push_back(kv.first);
  });
  if (fast.empty()) return;
  cpu_set_t set;
  CPU_ZERO(&set);
  std::string list;
  for (int c : fast) { CPU_SET(c, &set); list += " cpu" + std::to_string(c); }
  if (sched_setaffinity(0, sizeof set, &set) == 0)
    LOGI("pinned %s to the fastest cluster:%s", who, list.c_str());
  else
    LOGW("could not pin %s to the fastest cluster", who);
}

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

  // The engine unpacks assets from the pack with its own LZMA/LZ4 decoder,
  // emulated instruction by instruction. If the level-select stalls are that
  // decoder, this counter moves with them; if they are something else, it
  // stays flat. Entry only, so it costs a callback per asset.
  if (const uint32_t unpack_at = g_app->rt.SymAddr("UE_Unpack")) {
    g_app->rt.AddWatch(unpack_at, [](shogun::Runtime& r) {
      if (!g_app) return;
      g_app->unpacks++;
      // Learn the container before deciding whether a host decoder can stand
      // in for the emulated one. The engine logs "unpacked head : 0x%x algo :
      // %d unpacked : %d", so the compressed block carries its own header --
      // dump the first words of each argument and read it off.
      if (g_app->unpacks <= 6) {
        uint32_t hdr[4] = {0, 0, 0, 0};
        r.Read(r.Arg(0), hdr, sizeof hdr);
        LOGI("UE_Unpack(a0=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x) "
             "head=%08x %08x %08x %08x",
             r.Arg(0), r.Arg(1), r.Arg(2), r.Arg(3),
             hdr[0], hdr[1], hdr[2], hdr[3]);
      }
    });
    LOGI("watching UE_Unpack @0x%08x", unpack_at);
  } else {
    LOGI("UE_Unpack not exported; cannot count asset unpacking");
  }
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
  // Per thread, and the render thread is rebuilt whenever the surface returns.
  static thread_local bool pinned = false;
  if (!pinned) { pinned = true; PinToFastestCores("render thread"); }
  uint32_t self = g_app->jni->WrapCached(env, g_app->jni->activity(),
                                        shogun::JniBridge::kSlotActivity);
  double t0 = NowMs();
  const uint64_t exec0 = shogun::Runtime::ExecNsThisThread();
  g_app->lw_at_tick = g_app->rt.LockWaitNs();
  g_app->gl_at_tick = shogun::GlNsTotal();
  if (g_app->last_tick_at > 0) {
    const double gap = t0 - g_app->last_tick_at;
    // A gap of half a second is the app having been in the background, not a
    // dropped frame; counting it would swamp the statistic it belongs to.
    if (gap < 500.0) {
      g_app->gap_sum += gap;
      if (gap > g_app->gap_max) g_app->gap_max = gap;
      g_app->gaps++;
      // Two 60 Hz frames: visibly a hitch whatever rate we are pacing to.
      if (gap > 33.0) g_app->gaps_late++;
      // Four frames' worth is no longer a hitch, it is a jolt. Name where it
      // went rather than letting it average away.
      if (gap > 66.0) {
        g_app->jolts++;
        LOGW("JOLT %.0f ms frame: %.1f emulating + %.1f queueing + %.1f GL "
             "(%.1f unaccounted)",
             gap, g_app->prev_exec_ms, g_app->prev_lock_ms, g_app->prev_gl_ms,
             gap - g_app->prev_exec_ms - g_app->prev_lock_ms -
                 g_app->prev_gl_ms);
      }
    } else {
      // Keep these out of the mean, where one of them would swamp 120 frames,
      // but never out of the total -- a half-second freeze is the symptom.
      g_app->stalls++;
      g_app->stall_ms += gap;
    }
  }
  g_app->last_tick_at = t0;
  if (!GuestCall("Java_net_int13_HalActivity_onTick",
                 {g_app->jni->env_guest(), self}))
    g_app->booted = false;         // stop hammering a dead engine
  g_app->tick_ms += NowMs() - t0;
  shogun::HardModeTick(++g_app->ticks);
  shogun::LeaderboardTick(g_app->ticks);
  const double this_tick_exec =
      (shogun::Runtime::ExecNsThisThread() - exec0) / 1e6;
  g_app->prev_exec_ms = this_tick_exec;
  g_app->prev_lock_ms =
      (g_app->rt.LockWaitNs() - g_app->lw_at_tick) / 1e6;
  g_app->prev_gl_ms =
      (shogun::GlNsTotal() - g_app->gl_at_tick) / 1e6;
  g_app->tick_exec_ms += this_tick_exec;
  if (this_tick_exec > g_app->worst_tick_exec_ms)
    g_app->worst_tick_exec_ms = this_tick_exec;
  if (g_app->ticks % 120 == 0) {
    // 512 samples at 22050 Hz is 23.2 ms of audio: if a tick routinely costs
    // more than that, the mixer cannot keep the track fed and you hear it.
    // Everything in this block runs on the render thread inside a frame, so
    // time the whole of it: instrumentation that costs a frame is a bug.
    const double diag_t0 = NowMs();
    LOGI("tick %llu  draws %llu  heap %u KB  |  tick %.1f ms  audio %.2f ms x%llu",
         static_cast<unsigned long long>(g_app->ticks),
         static_cast<unsigned long long>(shogun::GlDrawCount()),
         g_app->rt.HeapPeak() / 1024,
         g_app->tick_ms / 120.0,
         g_app->audio_calls ? g_app->audio_ms / g_app->audio_calls : 0.0,
         static_cast<unsigned long long>(g_app->audio_calls));
    // Frames per second as actually delivered, plus the worst single gap in
    // the window. A healthy 60 fps is 16.7 ms with a worst case near it; a
    // worst case at twice the mean is a frame that was not shown on time, and
    // that is what you feel.
    const uint64_t cq = shogun::ClockQueryCount();
    LOGI("frame: %.2f ms mean (%.1f fps)  worst %.2f ms  hitches %llu/%llu  "
         "stalls %llu (%.0f ms)  guest clock reads %.1f/frame",
         g_app->gaps ? g_app->gap_sum / g_app->gaps : 0.0,
         g_app->gap_sum > 0 ? 1000.0 * g_app->gaps / g_app->gap_sum : 0.0,
         g_app->gap_max,
         static_cast<unsigned long long>(g_app->gaps_late),
         static_cast<unsigned long long>(g_app->gaps),
         static_cast<unsigned long long>(g_app->stalls), g_app->stall_ms,
         g_app->gaps ? (cq - g_app->clock_queries_at_log) /
                       static_cast<double>(g_app->gaps) : 0.0);
    g_app->clock_queries_at_log = cq;
    // Where the frame actually went. Both threads enter the emulator through
    // one lock, so a slow tick can mean the emulator is slow or merely that
    // the audio thread was holding it. Only this split tells them apart.
    const uint64_t lw = g_app->rt.LockWaitNs(), ex = g_app->rt.ExecNs();
    const double frames = g_app->gaps ? static_cast<double>(g_app->gaps) : 1.0;
    // The emulator is one lane: these two numbers add up against the frame
    // period, so if their sum approaches it the device is simply out of
    // emulation throughput and no amount of pacing will buy 60 fps.
    LOGI("guest per frame: %.2f ms game + %.2f ms audio = %.2f ms emulating "
         "(+ %.2f ms queueing)  budget %.2f ms",
         g_app->tick_exec_ms / frames,
         g_app->audio_exec_ms / frames,
         (ex - g_app->exec_at_log) / 1e6 / frames,
         (lw - g_app->lock_wait_at_log) / 1e6 / frames,
         g_app->gaps ? g_app->gap_sum / frames : 0.0);
    {
      const uint64_t tf = g_app->rt.TrapsFast(), ts = g_app->rt.TrapsSlow();
      const uint64_t df = tf - g_app->traps_fast_at_log;
      const uint64_t ds = ts - g_app->traps_slow_at_log;
      LOGI("import traps per frame: %.0f fast + %.0f slow (%.1f%% restart the "
           "emulator)",
           df / frames, ds / frames,
           (df + ds) ? 100.0 * ds / (df + ds) : 0.0);
      g_app->traps_fast_at_log = tf; g_app->traps_slow_at_log = ts;
    }
    LOGI("GL host time, %s", shogun::GlProfile().c_str());
    LOGI("this log line cost %.1f ms last time round", g_app->diag_ms);
    LOGI("worst single tick %.1f ms emulating; UE_Unpack x%llu (%.0f KB out)",
         g_app->worst_tick_exec_ms,
         static_cast<unsigned long long>(g_app->unpacks),
         g_app->unpack_bytes / 1024.0);
    // Which host call the guest leaned on. A stall that is all compute shows
    // nothing here; one that is file work shows fread.
    {
      std::vector<std::pair<uint64_t, std::string>> top;
      for (const auto& kv : g_app->rt.CallCounts()) {
        auto p = g_app->calls_at_log.find(kv.first);
        const uint64_t d = kv.second - (p == g_app->calls_at_log.end() ? 0 : p->second);
        if (d) top.emplace_back(d, kv.first);
      }
      std::sort(top.rbegin(), top.rend());
      std::string line;
      for (size_t i = 0; i < top.size() && i < 6; i++)
        line += "  " + top[i].second + " x" + std::to_string(top[i].first);
      if (!line.empty()) LOGI("hottest shims:%s", line.c_str());
      g_app->calls_at_log = g_app->rt.CallCounts();
    }
    g_app->exec_at_log = ex; g_app->lock_wait_at_log = lw;
    g_app->tick_exec_ms = 0; g_app->audio_exec_ms = 0;
    g_app->worst_tick_exec_ms = 0;
    g_app->stalls = 0; g_app->stall_ms = 0;
    g_app->unpacks = 0; g_app->unpack_bytes = 0;
    g_app->gap_sum = 0; g_app->gap_max = 0; g_app->gaps = 0; g_app->gaps_late = 0;
    g_app->tick_ms = 0; g_app->audio_ms = 0; g_app->audio_calls = 0;
    // Ask the engine what it believes its own audio state is. The mixer
    // early-outs on a "playback armed" flag, so silence with a healthy track
    // means the engine never started any music -- not that we lost the PCM.
    LOGI("GL: %s", shogun::GlDiagnostics(g_app->rt).c_str());
    // These four each take the emulator lock and run a full guest call, and
    // they only ever existed to notice when the engine starts its music so we
    // can arm the output. Once armed that answer cannot change, and asking
    // anyway cost a visible hitch every 120 frames -- which at 45 fps is every
    // 2.7 seconds, exactly the period the stutter was reported at. So ask only
    // while the answer still matters.
    uint32_t music = 0, chans = 0, mvol = 0, avol = 0;
    if (!g_app->audio_armed) {
      std::string e;
      g_app->rt.CallSym("UE_isMusicPlaying", {}, &music, &e);
      g_app->rt.CallSym("UE_GetNbPlayingChannels", {}, &chans, &e);
      g_app->rt.CallSym("UE_GetMusicVolume", {}, &mvol, &e);
      g_app->rt.CallSym("UE_GetAudioVolume", {}, &avol, &e);
      LOGI("engine audio: musicPlaying=%u channels=%u musicVol=%u masterVol=%u",
           music, chans, mvol, avol);
    }
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
    g_app->diag_ms = NowMs() - diag_t0;
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
  static thread_local bool pinned = false;
  if (!pinned) { pinned = true; PinToFastestCores("audio thread"); }
  // Both of these recur every audio frame, so they must reuse one handle.
  // Wrapping fresh each time leaked a global ref and, worse, allocated a new
  // GetPrimitiveArrayCritical staging buffer per frame until the JNI arena ran
  // out -- which is why sound stopped after about a second.
  uint32_t self  = g_app->jni->WrapCached(env, g_app->jni->activity(),
                                          shogun::JniBridge::kSlotActivity);
  uint32_t hbuf  = g_app->jni->WrapCached(env, buf,
                                          shogun::JniBridge::kSlotAudioBuf);
  double t0 = NowMs();
  const uint64_t exec0 = shogun::Runtime::ExecNsThisThread();
  GuestCall("Java_net_int13_HalActivity_onAudioFrame",
            {g_app->jni->env_guest(), self, hbuf,
             static_cast<uint32_t>(samples)});
  g_app->audio_ms += NowMs() - t0;
  g_app->audio_exec_ms +=
      (shogun::Runtime::ExecNsThisThread() - exec0) / 1e6;
  g_app->audio_calls++;
}

// ---- leaderboard bridge --------------------------------------------------
// Java owns the HTTPS call; native owns everything that touches the guest.

JNIEXPORT jstring JNICALL
Java_net_int13_shogun_ShogunNative_nativeLeaderboardPending(JNIEnv* env, jclass) {
  const std::string s = shogun::LeaderboardPending();
  return s.empty() ? nullptr : env->NewStringUTF(s.c_str());
}

// True once, each time the in-game GitHub button is pressed.
JNIEXPORT jboolean JNICALL
Java_net_int13_shogun_ShogunNative_nativeTakeOpenGithub(JNIEnv*, jclass) {
  return shogun::TakeOpenGithub() ? JNI_TRUE : JNI_FALSE;
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
