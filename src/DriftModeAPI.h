#pragma once
#include <windows.h>
#include <cstdint>
// cdecl x86, fixed-width fields. This API controls mode selection only.
// It is NOT permission to write AI cars. Call GetModeV1 first; ready must be 1.
#pragma pack(push,8)
struct MWArcadeDriftModeV1 {
 std::uint32_t size,version,manualMode,effectiveMode,forced,ready,vehicleReady,appliedMode;
 std::uint64_t tick,owner,token,expires;
};
#pragma pack(pop)
static_assert(sizeof(MWArcadeDriftModeV1)==64,"Mode ABI size");
using MWArcadeDriftGetModeV1Fn=BOOL(__cdecl*)(MWArcadeDriftModeV1*);
using MWArcadeDriftAcquireSpecialV1Fn=BOOL(__cdecl*)(std::uint64_t,std::uint32_t,std::uint64_t*);
using MWArcadeDriftRenewSpecialV1Fn=BOOL(__cdecl*)(std::uint64_t,std::uint64_t,std::uint32_t);
using MWArcadeDriftReleaseSpecialV1Fn=BOOL(__cdecl*)(std::uint64_t,std::uint64_t);
// Exports (exact undecorated names): MWArcadeDriftGetModeV1,
// MWArcadeDriftAcquireSpecialV1, MWArcadeDriftRenewSpecialV1,
// MWArcadeDriftReleaseSpecialV1. A lease is 250..10000ms; renew before expiry.
// Single owner, no nested overrides. Manual mode is frozen during the lease.
// Release/timeout restores the exact manual mode saved before the override.
