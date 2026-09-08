// shims.cpp -- host implementations of the 99 imports.
//
// Ported from runtime/shims.py. The import set is closed: the library has no
// dlopen and issues no SVC, so nothing can appear here at runtime that is not
// registered below.
//
// One convention is deliberately non-uniform and must stay that way:
// gethostbyname returns a POINTER and reports failure as NULL. Stubbing it to
// -1 alongside the socket calls made the engine accept 0xffffffff as a valid
// struct hostent* and dereference it -- a crash that only appeared 169 frames
// into a running game, inside UE_ResolveHostAddress.
#include "shogun_runtime.h"

#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <unordered_map>

namespace shogun {
namespace {

constexpr const char* kTag = "shogun";

struct FileState {
  std::string root;                                  // app-private storage
  std::unordered_map<uint32_t, FILE*> open;          // guest FILE* -> host
  std::unordered_map<int, FILE*> fds;                // fd -> host (asset pack)
  uint32_t tm_buf = 0;
};
FileState* g_files = nullptr;

std::string HostPath(const std::string& p) {
  std::string s = p;
  for (auto& c : s) if (c == '\\') c = '/';
  // The engine builds a path by pasting the storage directory it was given
  // onto a file name, so what arrives here is already absolute and already
  // inside our sandbox. Stripping the leading slash and re-prefixing turned
  // it into <root>/<root>/<name>, and every single save failed to open.
  if (s.size() > g_files->root.size() &&
      s.compare(0, g_files->root.size(), g_files->root) == 0)
    return s;
  while (!s.empty() && s.front() == '/') s.erase(s.begin());
  return g_files->root + "/" + s;
}

// ---- ARM32 varargs -------------------------------------------------------
// Walk r1-r3 then the stack, using the format string to know each width.
std::string FormatVa(Runtime& rt, const std::string& fmt, int argi) {
  std::string out;
  size_t i = 0;
  int n = argi;
  char tmp[512];
  while (i < fmt.size()) {
    if (fmt[i] != '%') { out.push_back(fmt[i++]); continue; }
    size_t j = i + 1;
    while (j < fmt.size() && std::strchr("-+ #0123456789.*lhqLzjt", fmt[j])) j++;
    if (j >= fmt.size()) break;
    char conv = fmt[j];
    std::string spec = fmt.substr(i, j - i + 1);
    bool ll = spec.find("ll") != std::string::npos || spec.find('q') != std::string::npos;
    // strip length modifiers; we supply the right C type ourselves
    std::string clean;
    for (size_t k = 0; k < spec.size(); k++)
      if (!std::strchr("lhqLzjt", spec[k])) clean.push_back(spec[k]);
    tmp[0] = 0;
    if (conv == '%') { out.push_back('%'); i = j + 1; continue; }
    if (conv == 'd' || conv == 'i') {
      if (ll) {
        if (n % 2) n++;
        int64_t v = static_cast<int64_t>(
            static_cast<uint64_t>(rt.Arg(n)) | (static_cast<uint64_t>(rt.Arg(n + 1)) << 32));
        n += 2;
        std::snprintf(tmp, sizeof tmp, (clean.substr(0, clean.size() - 1) + "lld").c_str(), (long long)v);
      } else {
        std::snprintf(tmp, sizeof tmp, clean.c_str(), static_cast<int32_t>(rt.Arg(n++)));
      }
    } else if (std::strchr("uxXo", conv)) {
      std::snprintf(tmp, sizeof tmp, clean.c_str(), rt.Arg(n++));
    } else if (std::strchr("fFeEgG", conv)) {
      if (n % 2) n++;
      double d = rt.ArgD(n); n += 2;
      std::snprintf(tmp, sizeof tmp, clean.c_str(), d);
    } else if (conv == 'c') {
      std::snprintf(tmp, sizeof tmp, clean.c_str(), static_cast<int>(rt.Arg(n++) & 0xff));
    } else if (conv == 's') {
      std::string s = rt.CStr(rt.Arg(n++));
      std::snprintf(tmp, sizeof tmp, clean.c_str(), s.c_str());
    } else if (conv == 'p') {
      std::snprintf(tmp, sizeof tmp, "0x%x", rt.Arg(n++));
    } else {
      std::snprintf(tmp, sizeof tmp, "%s", clean.c_str());
    }
    out += tmp;
    i = j + 1;
  }
  return out;
}

}  // namespace

void InstallShims(Runtime& rt, const std::string& files_dir) {
  static FileState state;
  state.root = files_dir;
  g_files = &state;

  auto S = [&rt](const char* n, ShimFn f) { rt.SetShim(n, std::move(f)); };

  // ------------------------------------------------------------- memory
  S("malloc",  [](Runtime& r) { r.Ret(r.Malloc(r.Arg(0))); });
  S("free",    [](Runtime& r) { r.Free(r.Arg(0)); r.Ret(0); });
  S("realloc", [](Runtime& r) {
    uint32_t p = r.Arg(0), n = r.Arg(1);
    if (!p) { r.Ret(r.Malloc(n)); return; }
    uint32_t np = r.Malloc(n);
    if (np && n) {
      std::vector<uint8_t> buf(n);
      r.Read(p, buf.data(), n);          // copies at most the new size
      r.Write(np, buf.data(), n);
    }
    r.Free(p);
    r.Ret(np);
  });
  auto copy = [](Runtime& r) {
    uint32_t d = r.Arg(0), s = r.Arg(1), n = r.Arg(2);
    if (n) {
      std::vector<uint8_t> buf(n);
      r.Read(s, buf.data(), n);
      r.Write(d, buf.data(), n);
    }
    r.Ret(d);
  };
  S("memcpy", copy);
  S("memmove", copy);
  S("memset", [](Runtime& r) {
    uint32_t d = r.Arg(0), n = r.Arg(2);
    uint8_t c = static_cast<uint8_t>(r.Arg(1));
    if (n) { std::vector<uint8_t> buf(n, c); r.Write(d, buf.data(), n); }
    r.Ret(d);
  });

  // --------------------------------------------------------------- math
  // Soft-float: doubles arrive in an even register pair and return in r0:r1.
  auto d1 = [](double (*fn)(double)) {
    return [fn](Runtime& r) { r.RetD(fn(r.ArgD(0))); };
  };
  auto d2 = [](double (*fn)(double, double)) {
    return [fn](Runtime& r) { r.RetD(fn(r.ArgD(0), r.ArgD(2))); };
  };
  S("sin",  d1(std::sin));   S("cos",  d1(std::cos));   S("tan", d1(std::tan));
  S("asin", d1(std::asin));  S("acos", d1(std::acos));  S("atan", d1(std::atan));
  S("sqrt", d1(std::sqrt));  S("log",  d1(std::log));
  S("atan2", d2(std::atan2)); S("pow", d2(std::pow));

  // --------------------------------------------------------------- time
  S("clock_gettime", [](Runtime& r) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t v[2] = {static_cast<uint32_t>(ts.tv_sec),
                     static_cast<uint32_t>(ts.tv_nsec)};
    r.Write(r.Arg(1), v, sizeof v);
    r.Ret(0);
  });
  S("time", [](Runtime& r) {
    uint32_t t = static_cast<uint32_t>(::time(nullptr));
    if (r.Arg(0)) r.Write(r.Arg(0), &t, 4);
    r.Ret(t);
  });
  S("localtime", [](Runtime& r) {
    uint32_t tp = r.Arg(0), secs = 0;
    if (tp) r.Read(tp, &secs, 4); else secs = static_cast<uint32_t>(::time(nullptr));
    time_t t = secs;
    struct tm lt{};
    localtime_r(&t, &lt);
    if (!g_files->tm_buf) g_files->tm_buf = r.AllocScratch(44);
    int32_t v[9] = {lt.tm_sec, lt.tm_min, lt.tm_hour, lt.tm_mday,
                    lt.tm_mon, lt.tm_year, lt.tm_wday, lt.tm_yday, lt.tm_isdst};
    r.Write(g_files->tm_buf, v, sizeof v);
    r.Ret(g_files->tm_buf);
  });
  // The engine only uses usleep to yield; real sleeping would stall the frame.
  S("usleep", [](Runtime& r) { r.Ret(0); });

  // ------------------------------------------------------------ file I/O
  S("fopen", [](Runtime& r) {
    std::string path = r.CStr(r.Arg(0));
    std::string mode = r.CStr(r.Arg(1));
    if (mode.empty()) mode = "rb";
    if (mode.find('b') == std::string::npos) mode += "b";
    FILE* f = std::fopen(HostPath(path).c_str(), mode.c_str());
    // Saving is the one thing that has to be visible from outside the process,
    // and the app is not debuggable, so every write-mode open gets a line.
    if (!f) {
      __android_log_print(f ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, kTag,
          "fopen(\"%s\", \"%s\") -> %s   [%s]", path.c_str(), mode.c_str(),
          f ? "ok" : "FAILED", HostPath(path).c_str());
    }
    if (!f) { r.Ret(0); return; }
    uint32_t fp = r.Malloc(8);
    g_files->open[fp] = f;
    r.Ret(fp);
  });
  // The asset pack arrives as a descriptor from Java (openRawResourceFd).
  S("fdopen", [](Runtime& r) {
    int fd = static_cast<int>(r.Arg(0));
    auto it = g_files->fds.find(fd);
    if (it == g_files->fds.end()) { r.Ret(0); return; }
    uint32_t fp = r.Malloc(8);
    g_files->open[fp] = it->second;
    r.Ret(fp);
  });
  S("fread", [](Runtime& r) {
    uint32_t buf = r.Arg(0), sz = r.Arg(1), n = r.Arg(2), fp = r.Arg(3);
    auto it = g_files->open.find(fp);
    if (it == g_files->open.end() || !sz || !n) { r.Ret(0); return; }
    std::vector<uint8_t> tmp(static_cast<size_t>(sz) * n);
    size_t got = std::fread(tmp.data(), 1, tmp.size(), it->second);
    if (got) r.Write(buf, tmp.data(), got);
    r.Ret(static_cast<uint32_t>(got / sz));
  });
  S("fwrite", [](Runtime& r) {
    uint32_t buf = r.Arg(0), sz = r.Arg(1), n = r.Arg(2), fp = r.Arg(3);
    auto it = g_files->open.find(fp);
    if (it == g_files->open.end() || !sz || !n) { r.Ret(0); return; }
    std::vector<uint8_t> tmp(static_cast<size_t>(sz) * n);
    r.Read(buf, tmp.data(), tmp.size());
    std::fwrite(tmp.data(), 1, tmp.size(), it->second);
    r.Ret(n);
  });
  S("fseek", [](Runtime& r) {
    auto it = g_files->open.find(r.Arg(0));
    if (it == g_files->open.end()) { r.Ret(0xFFFFFFFFu); return; }
    std::fseek(it->second, static_cast<int32_t>(r.Arg(1)), static_cast<int>(r.Arg(2)));
    r.Ret(0);
  });
  S("ftell", [](Runtime& r) {
    auto it = g_files->open.find(r.Arg(0));
    r.Ret(it == g_files->open.end() ? 0xFFFFFFFFu
                                    : static_cast<uint32_t>(std::ftell(it->second)));
  });
  S("fclose", [](Runtime& r) {
    auto it = g_files->open.find(r.Arg(0));
    if (it != g_files->open.end()) { std::fclose(it->second); g_files->open.erase(it); }
    r.Ret(0);
  });
  S("fflush", [](Runtime& r) {
    auto it = g_files->open.find(r.Arg(0));
    if (it != g_files->open.end()) std::fflush(it->second);
    r.Ret(0);
  });
  S("fputc", [](Runtime& r) { r.Ret(r.Arg(0)); });
  S("stat", [](Runtime& r) {
    FILE* f = std::fopen(HostPath(r.CStr(r.Arg(0))).c_str(), "rb");
    if (f) { std::fclose(f); r.Ret(0); } else r.Ret(0xFFFFFFFFu);
  });
  S("mkdir", [](Runtime& r) {
    std::string p = HostPath(r.CStr(r.Arg(0)));
    std::string cmd;
    // create the whole chain; the engine makes nested save directories
    for (size_t i = 1; i <= p.size(); i++) {
      if (i == p.size() || p[i] == '/') {
        std::string part = p.substr(0, i);
        ::mkdir(part.c_str(), 0770);
      }
    }
    r.Ret(0);
  });
  S("remove", [](Runtime& r) {
    r.Ret(std::remove(HostPath(r.CStr(r.Arg(0))).c_str()) == 0 ? 0 : 0xFFFFFFFFu);
  });
  S("chdir", [](Runtime& r) { r.Ret(0); });
  S("dup",   [](Runtime& r) { r.Ret(r.Arg(0)); });   // same descriptor is fine
  S("close", [](Runtime& r) { r.Ret(0); });
  S("fcntl", [](Runtime& r) { r.Ret(0); });

  // ------------------------------------------------------- varargs output
  S("__android_log_print", [](Runtime& r) {
    std::string tag = r.CStr(r.Arg(1));
    std::string fmt = r.CStr(r.Arg(2));
    std::string msg = FormatVa(r, fmt, 3);
    __android_log_print(ANDROID_LOG_INFO, tag.empty() ? kTag : tag.c_str(),
                        "%s", msg.c_str());
    r.Ret(0);
  });
  S("fprintf", [](Runtime& r) {
    std::string fmt = r.CStr(r.Arg(1));
    std::string msg = FormatVa(r, fmt, 2);
    auto it = g_files->open.find(r.Arg(0));
    if (it != g_files->open.end()) std::fwrite(msg.data(), 1, msg.size(), it->second);
    else __android_log_print(ANDROID_LOG_WARN, kTag, "%s", msg.c_str());
    r.Ret(static_cast<uint32_t>(msg.size()));
  });

  // ------------------------------------------------------------- runtime
  S("__errno", [](Runtime& r) { r.Ret(r.ErrnoAddr()); });
  S("abort", [](Runtime& r) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "guest called abort()");
    r.Ret(0);
  });
  S("__stack_chk_fail", [](Runtime& r) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "guest stack smashing detected");
    r.Ret(0);
  });
  // The unwinder itself lives inside the library and runs emulated; we only
  // have to hand it the .ARM.exidx bounds.
  S("__gnu_Unwind_Find_exidx", [](Runtime& r) {
    uint32_t count = r.ExidxCount();
    if (r.Arg(1)) r.Write(r.Arg(1), &count, 4);
    r.Ret(r.ExidxBase());
  });
  for (const char* n : {"__cxa_begin_cleanup", "__cxa_type_match",
                        "__cxa_call_unexpected"})
    S(n, [](Runtime& r) { r.Ret(1); });

  // ------------------------------------------------------------- sockets
  // Vestigial (adhocwifi.c / gamecenter.c). Note the split convention.
  for (const char* n : {"socket", "bind", "connect", "send", "recv", "sendto",
                        "recvfrom", "sendmsg", "setsockopt", "gethostname"})
    S(n, [](Runtime& r) { r.Ret(0xFFFFFFFFu); });
  S("gethostbyname", [](Runtime& r) { r.Ret(0); });   // returns a pointer: NULL
}

// Lets the JNI layer hand the asset-pack descriptor to fdopen.
void RegisterAssetFd(int fd, FILE* f) {
  if (g_files) g_files->fds[fd] = f;
}

}  // namespace shogun
