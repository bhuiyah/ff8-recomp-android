// steamworkssdk/steam_api.h - the Steamworks surface FFNx uses for achievements.
// There is no Steam client on the device. src/ffnx_port/steam_android.cpp stands in for it:
// it reads the player's Steam achievement state that the launcher cached
// (files/steam/state.txt, written after the player signed in to Steam in the launcher) and
// queues unlocks in files/steam/pending.txt, which the app sends to Steam (SteamLink.java).
// The launcher only turns FFNx's enable_steam_achievements on when the player is signed in.
#pragma once
#include <cstdint>

typedef int32_t int32;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint32_t AppId_t;
enum EResult { k_EResultOK = 1, k_EResultFail = 2 };

struct UserStatsReceived_t { uint64 m_nGameID; EResult m_eResult; uint64 m_steamIDUser; };
struct UserStatsStored_t { uint64 m_nGameID; EResult m_eResult; };
struct UserAchievementStored_t { uint64 m_nGameID; bool m_bGroupAchievement; char m_rgchAchievementName[128]; uint32 m_nCurProgress, m_nMaxProgress; };

class ISteamUserStats {
public:
    bool GetAchievement(const char *name, bool *achieved);
    bool SetAchievement(const char *name);
    const char *GetAchievementDisplayAttribute(const char *name, const char *key);
    // integer stats (FF8 2013: enemies killed, magic drawn and stocked, card games won), from
    // state.txt; a new value is queued in pending.txt like an unlock
    bool GetStat(const char *name, int32 *v);
    bool GetStat(const char *, float *v) { if (v) *v = 0; return false; }
    bool SetStat(const char *name, int32 v);
    bool SetStat(const char *, float) { return false; }
    bool StoreStats() { return true; }                                      // queued at once
    bool IndicateAchievementProgress(const char *, uint32, uint32) { return true; }
    bool RequestCurrentStats() { return true; }
};
class ISteamUtils { public: AppId_t GetAppID(); };
class CGameID {
    uint64 id_ = 0;
public:
    CGameID() = default;
    explicit CGameID(uint64 id) : id_(id) {}
    explicit CGameID(AppId_t app) : id_(app) {}
    uint64 ToUint64() const { return id_; }
    AppId_t AppID() const { return (AppId_t)(id_ & 0xFFFFFFu); }
    bool operator==(const CGameID &o) const { return id_ == o.id_; }
    bool operator!=(const CGameID &o) const { return id_ != o.id_; }
};
class ISteamUser { public: bool BLoggedOn(); };

ISteamUserStats *SteamUserStats();
ISteamUtils *SteamUtils();
ISteamUser *SteamUser();
bool SteamAPI_Init();
inline void SteamAPI_Shutdown() {}
inline void SteamAPI_RunCallbacks() {}
inline bool SteamAPI_RestartAppIfNecessary(uint32) { return false; }

// STEAM_CALLBACK(class, method, param, member): a member function the Steam client would
// call back; nothing calls it here (FFNx loads the achievement state directly when signed in).
template <class C, class P> struct CCallbackStub { CCallbackStub(C *, void (C::*)(P *)) {} };
#define STEAM_CALLBACK(thisclass, func, param, var) \
    CCallbackStub<thisclass, param> var{ this, &thisclass::func }; \
    void func(param *pParam)
