// leaderboard.cpp -- the online leaderboard, without the original server.
//
// The game talked to int13's "Hub Server" over NWT, their own obfuscated
// binary packet protocol: connect to a hardcoded IP, ask it which IP:port is
// serving leaderboards today, then speak NWT to that. The strings still say so:
//
//     LEADERBOARD: Connected to the Hub Server.
//     LEADERBOARD: Service available @%ip:%d.
//     NWT: Incorrect host name %s, expected 'xxx.xxx.xxx.xxx'
//
// Those addresses were int13's a decade ago. Nothing answers, and reviving the
// protocol would mean reversing the obfuscation and then hosting raw TCP.
//
// None of which is necessary, because the engine exposes
//
//     LEADERBOARD_SetScoreReceivedCallback(handle, callback, user)
//
// and the callback it registers -- onReceiveScore -- is what actually fills in
// the ranking screen. Its 13 arguments were recovered from the log lines it
// prints for each of them ("World best: %d (%s)", "Country rank: %d (%s)" ...),
// which agree exactly with the mangled parameter types:
//
//   onReceiveScore(board, yourBest,
//                  worldRank,   worldBest,   worldBestName,
//                  countryRank, countryBest, countryBestName,
//                  cityRank,    cityBest,    cityBestName,
//                  countryName, cityName, SHOGUN*)
//
// So the port does the networking over ordinary HTTPS (in Java, where TLS is
// somebody else's problem) and calls that function with the answer. The guest
// never opens a socket.
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "jni_bridge.h"
#include "shogun_runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "shogun", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "shogun", __VA_ARGS__)

namespace shogun {
namespace {

// LEADERBOARD_SubmitScore(handle, char* board, uint score) is where the game
// announces a score; watching it is how the port learns there is one to send.
const char* kSubmitScore   = "_Z23LEADERBOARD_SubmitScorejPcj";
const char* kGetPlayerName = "_Z25LEADERBOARD_GetPlayerNamej";
const char* kOnReceive     = "_Z14onReceiveScorePcjjjS_jjS_jjS_S_S_P6SHOGUN";

// The engine copies every name with a 31-character limit.
constexpr size_t kNameMax = 31;

struct Result {
  std::string board;
  uint32_t your_best = 0;
  uint32_t w_rank = 0, w_best = 0;  std::string w_name;
  uint32_t c_rank = 0, c_best = 0;  std::string c_name;
  uint32_t t_rank = 0, t_best = 0;  std::string t_name;
  std::string country, city;
};

struct State {
  Runtime* rt = nullptr;
  std::mutex mu;                  // the watch, Java and the tick all touch this
  std::string pending_board;      // set by the watch, drained by Java
  uint32_t    pending_score = 0;
  bool        have_pending = false;
  std::vector<Result> inbox;      // set by Java, drained by the tick
  uint32_t    lb_handle = 0;      // first argument the game passes SubmitScore
  uint64_t    sent = 0, applied = 0;
};
State g;

uint32_t GuestStr(const std::string& s) {
  const std::string t = s.size() > kNameMax ? s.substr(0, kNameMax) : s;
  const uint32_t n = static_cast<uint32_t>(t.size()) + 1;
  const uint32_t p = g.rt->Malloc(n);
  if (p) g.rt->Write(p, t.c_str(), n);
  return p;
}

// Hand one result to the engine's own callback. Runs from the tick, never from
// a watch: CallSym would re-enter the runtime lock this thread already holds.
void Apply(const Result& r) {
  const uint32_t shogun = CurrentShogun();
  if (!shogun) return;

  const uint32_t a_board = GuestStr(r.board.empty() ? "total" : r.board);
  const uint32_t a_wname = GuestStr(r.w_name);
  const uint32_t a_cname = GuestStr(r.c_name);
  const uint32_t a_tname = GuestStr(r.t_name);
  const uint32_t a_ctry  = GuestStr(r.country);
  const uint32_t a_city  = GuestStr(r.city);
  if (!a_board || !a_wname || !a_cname || !a_tname || !a_ctry || !a_city) {
    LOGW("leaderboard: out of guest memory, dropping result");
    return;
  }

  uint32_t out = 0;
  std::string err;
  const bool ok = g.rt->CallSym(
      kOnReceive,
      {a_board, r.your_best,
       r.w_rank, r.w_best, a_wname,
       r.c_rank, r.c_best, a_cname,
       r.t_rank, r.t_best, a_tname,
       a_ctry, a_city, shogun},
      &out, &err);

  for (uint32_t p : {a_board, a_wname, a_cname, a_tname, a_ctry, a_city})
    g.rt->Free(p);

  if (!ok) {
    LOGW("leaderboard: onReceiveScore failed: %s", err.c_str());
    return;
  }
  g.applied++;
  LOGI("leaderboard: '%s' applied - you %u, world #%u (best %u by %s), "
       "%s #%u, %s #%u",
       r.board.c_str(), r.your_best, r.w_rank, r.w_best, r.w_name.c_str(),
       r.country.c_str(), r.c_rank, r.city.c_str(), r.t_rank);
}

}  // namespace

void InstallLeaderboard(Runtime& rt) {
  g.rt = &rt;
  const uint32_t at = rt.SymAddr(kSubmitScore);
  if (!at) { LOGW("leaderboard: %s not found", kSubmitScore); return; }
  // r0 = leaderboard handle, r1 = board name, r2 = score.
  rt.AddWatch(at, [](Runtime& r) {
    const uint32_t handle = r.Arg(0);
    std::string board = r.CStr(r.Arg(1), 32);
    const uint32_t score = r.Arg(2);
    std::lock_guard<std::mutex> lk(g.mu);
    g.lb_handle = handle;
    g.pending_board = board.empty() ? "total" : board;
    g.pending_score = score;
    g.have_pending = true;
  });
  LOGI("leaderboard: watching %s @0x%08x", kSubmitScore, at);
}

// Called from the tick. Opening the leaderboard screen with nothing to send
// still asks the server for the current standings, which is a score of 0.
void LeaderboardTick(uint64_t ticks) {
  if (!g.rt) return;
  std::vector<Result> todo;
  {
    std::lock_guard<std::mutex> lk(g.mu);
    todo.swap(g.inbox);
  }
  for (const Result& r : todo) Apply(r);
}

std::string LeaderboardPending() {
  std::lock_guard<std::mutex> lk(g.mu);
  if (!g.have_pending) return std::string();
  g.have_pending = false;
  g.sent++;
  char buf[64];
  std::snprintf(buf, sizeof buf, "\n%u", g.pending_score);
  return g.pending_board + buf;
}

// The name the player typed into the "worldwide leaderboard nickname" prompt.
std::string LeaderboardPlayerName() {
  if (!g.rt || !g.lb_handle) return std::string();
  uint32_t p = 0;
  std::string err;
  if (!g.rt->CallSym(kGetPlayerName, {g.lb_handle}, &p, &err) || !p)
    return std::string();
  return g.rt->CStr(p, kNameMax);
}

void LeaderboardResult(const char* board, uint32_t your_best,
                       uint32_t w_rank, uint32_t w_best, const char* w_name,
                       uint32_t c_rank, uint32_t c_best, const char* c_name,
                       uint32_t t_rank, uint32_t t_best, const char* t_name,
                       const char* country, const char* city) {
  Result r;
  r.board = board ? board : "total";
  r.your_best = your_best;
  r.w_rank = w_rank; r.w_best = w_best; r.w_name = w_name ? w_name : "--";
  r.c_rank = c_rank; r.c_best = c_best; r.c_name = c_name ? c_name : "--";
  r.t_rank = t_rank; r.t_best = t_best; r.t_name = t_name ? t_name : "--";
  r.country = country ? country : "--";
  r.city = city ? city : "--";
  std::lock_guard<std::mutex> lk(g.mu);
  g.inbox.push_back(std::move(r));   // applied from the tick
}

}  // namespace shogun
