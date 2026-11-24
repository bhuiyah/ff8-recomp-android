// steam_android.cpp - the Steam client FFNx's achievement code talks to (see
// include/steamworkssdk/steam_api.h), as files shared with the app:
//
//   files/steam/state.txt    written by the app (SteamLink.java) after it read the player's
//                            FF8 achievements from Steam: "appid <id>" (the edition being
//                            played: 39150 the 2013 release, 1026680 the Remastered), then
//                            "ach <API name> <0|1> <display name>" and "stat <API name> <value>"
//                            (the 2013 release's counters: nmy_kill, mag_draw, mag_stck, won_card)
//   files/steam/pending.txt  appended here when FFNx unlocks one: "<API name>\t<unix time>",
//                            or sets a stat: "stat\t<API name>\t<value>"; the app sends these
//                            to Steam and clears them.
//
// The game runs in files/game, so the folder is ../steam.
#include "steamworkssdk/steam_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern "C" void ff7_achievements_banner(const char *title);

namespace {

constexpr AppId_t k_ff8_2013 = 39150;
AppId_t g_app = k_ff8_2013;                 // state.txt "appid": the edition being played

struct Ach { bool achieved = false; std::string display; };
std::map<std::string, int32> g_stats;

std::mutex g_mu;
bool g_loaded = false, g_have_state = false;
std::string g_dir;
std::map<std::string, Ach> g_ach;

void load() {
    if (g_loaded) return;
    g_loaded = true;
    char cwd[512];
    if (!getcwd(cwd, sizeof cwd)) return;
    g_dir = std::string(cwd) + "/../steam";
    mkdir(g_dir.c_str(), 0775);
    FILE *f = fopen((g_dir + "/state.txt").c_str(), "rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strncmp(line, "appid ", 6) == 0) { g_app = (AppId_t)strtoul(line + 6, nullptr, 10); continue; }
        if (strncmp(line, "stat ", 5) == 0) {
            char name[160] = {}; long v = 0;
            if (sscanf(line + 5, "%159s %ld", name, &v) == 2) g_stats[name] = (int32)v;
            continue;
        }
        if (strncmp(line, "ach ", 4) != 0) continue;
        char name[160] = {}; int got = 0; int off = 0;
        if (sscanf(line + 4, "%159s %d %n", name, &got, &off) < 2) continue;
        Ach &a = g_ach[name];
        a.achieved = got != 0;
        a.display = off ? std::string(line + 4 + off) : std::string(name);
    }
    fclose(f);
    // unlocks still waiting to be sent count as achieved; stats waiting count as set (the
    // counters only grow, so the larger value wins)
    if (FILE *p = fopen((g_dir + "/pending.txt").c_str(), "rb")) {
        while (fgets(line, sizeof line, p)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strncmp(line, "stat\t", 5) == 0) {
                char name[160] = {}; long v = 0;
                if (sscanf(line + 5, "%159[^\t]\t%ld", name, &v) == 2 && v > g_stats[name]) g_stats[name] = (int32)v;
                continue;
            }
            line[strcspn(line, "\t")] = 0;
            if (*line) g_ach[line].achieved = true;
        }
        fclose(p);
    }
    g_have_state = true;
    std::fprintf(stderr, "[steam] app %u: %zu achievements and %zu stats known from Steam\n",
                 (unsigned)g_app, g_ach.size(), g_stats.size());
}

}  // namespace

bool ISteamUserStats::GetAchievement(const char *name, bool *achieved) {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    auto it = g_ach.find(name);
    if (achieved) *achieved = it != g_ach.end() && it->second.achieved;
    return it != g_ach.end();
}

bool ISteamUserStats::SetAchievement(const char *name) {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    Ach &a = g_ach[name];
    if (a.achieved) return true;
    a.achieved = true;
    if (FILE *f = fopen((g_dir + "/pending.txt").c_str(), "ab")) {
        std::fprintf(f, "%s\t%lld\n", name, (long long)std::time(nullptr));
        std::fclose(f);
    }
    std::fprintf(stderr, "[steam] achievement unlocked: %s (queued for Steam)\n", name);
    ff7_achievements_banner(a.display.empty() ? name : a.display.c_str());
    return true;
}

bool ISteamUserStats::GetStat(const char *name, int32 *v) {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    auto it = g_stats.find(name);
    if (v) *v = it != g_stats.end() ? it->second : 0;
    return g_have_state;                       // a stat Steam never stored reads as 0
}

bool ISteamUserStats::SetStat(const char *name, int32 v) {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    if (!g_have_state) return false;
    int32 &cur = g_stats[name];
    if (cur == v) return true;
    cur = v;
    if (FILE *f = fopen((g_dir + "/pending.txt").c_str(), "ab")) {
        std::fprintf(f, "stat\t%s\t%d\n", name, (int)v);
        std::fclose(f);
    }
    return true;
}

const char *ISteamUserStats::GetAchievementDisplayAttribute(const char *name, const char *key) {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    auto it = g_ach.find(name);
    if (it == g_ach.end() || strcmp(key, "name") != 0) return "";
    return it->second.display.c_str();
}

AppId_t ISteamUtils::GetAppID() {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    return g_app;
}

// FF7Recomp's hook (FFNx's FF7 list per edition, patched into common.cpp); FF8's list follows
// ff8_remastered_edition.
extern "C" int ff7r_steam_is_2013() { return SteamUtils()->GetAppID() == k_ff8_2013; }

bool ISteamUser::BLoggedOn() {
    std::lock_guard<std::mutex> lk(g_mu);
    load();
    return g_have_state;
}

ISteamUserStats *SteamUserStats() { static ISteamUserStats s; return &s; }
ISteamUtils *SteamUtils() { static ISteamUtils s; return &s; }
ISteamUser *SteamUser() { static ISteamUser s; return &s; }

// Only reached when the launcher turned enable_steam_achievements on (signed in): FFNx's
// window creation fails if this does, so it always succeeds; without a cached state the
// achievement code stays idle (BLoggedOn false).
bool SteamAPI_Init() { return true; }
