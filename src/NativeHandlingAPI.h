#pragma once
#include <cstdint>
#include <cmath>
#include <initializer_list>
// Authored-module contract, fixed-width x86 cdecl. A successful response is
// a copied configuration for a currently owned, fresh Special race binding.
// No game pointers or layout addresses cross this interface.
struct MWArcadeDriftNativeHandlingV1 {
 std::uint32_t size=28,version=1;
 float front=100,rearDynamic=100,rearStatic=100,rearLateral=100,speedLoss=100;
};
static_assert(sizeof(MWArcadeDriftNativeHandlingV1)==28,"native handling ABI");
inline bool validNativeHandling(const MWArcadeDriftNativeHandlingV1& v){
 if(v.size!=sizeof(v)||v.version!=1)return false;
 for(float n:{v.front,v.rearDynamic,v.rearStatic})if(!std::isfinite(n)||n<20||n>100)return false;
 if(!std::isfinite(v.rearLateral)||v.rearLateral<10||v.rearLateral>100)return false;
 return std::isfinite(v.speedLoss)&&v.speedLoss>=0&&v.speedLoss<=100;
}
// 0 = unavailable/unowned, 1 = fresh race configuration, 2 = owned but stale.
// A fresh planner pause retains handling; it only suspends posture correction.
using MWArcadeDriftNativeHandlingV1Fn=std::uint32_t(__cdecl*)(std::uint32_t,MWArcadeDriftNativeHandlingV1*);
