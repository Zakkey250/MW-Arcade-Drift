#pragma once
#include <algorithm>
#include <cmath>
namespace mcd {
inline constexpr float NativeDriftTurnLimit=.65f;
// Normalised native wheel input only. The local EN1.3 road-response evidence
// is retained in diagnostics/drift-native-arcade-20261029/fit_native_input.py.
// beta uses CrossXZ(body, travel); yaw and wheel input use MW native signs.
// A steady angle is a RESULT of turning, never an independent fixed target.
inline float nativeDriftSteering(float targetYaw,float yaw,float beta){
 if(!std::isfinite(targetYaw)||!std::isfinite(yaw)||!std::isfinite(beta))return 0;
 // Native low-grip trial: yaw above 1 rad/s drove beta through 0.8 radians
 // despite full countersteer. Limit the driver's turn demand, not body yaw.
 targetYaw=std::clamp(targetYaw,-NativeDriftTurnLimit,NativeDriftTurnLimit);
 const float equilibriumSlip=.55f*targetYaw;
 const float feedforward=-.54f*targetYaw;
 // Damping checked against the independent 10/20/40% native tyre response
 // traces. Low rear grip needs faster yaw damping before counter lock runs
 // out; the previous 1.06 yaw gain let the initial rotation build for >1 s.
 return std::clamp(feedforward+3.f*(targetYaw-yaw)+1.8f*(equilibriumSlip-beta),-1.f,1.f);
}
struct NativeSteeringSample {float speed=0,beta=0,yaw=0;bool safe=false;};
inline float nativePadDriftSteering(float stick,const NativeSteeringSample& s,float brake,float handbrake){
 if(!s.safe||!std::isfinite(s.speed)||!std::isfinite(s.beta)||!std::isfinite(s.yaw)||
    !std::isfinite(stick)||!std::isfinite(brake)||!std::isfinite(handbrake)||
    s.speed<=5||s.speed>60||std::abs(s.beta)>1.4f||std::abs(s.yaw)>3||handbrake>.2f)return stick;
 // The stick requests a turn; native tyre feedback supplies the counter.
 // Keep this input meaning through beta=0 and direction changes. A beta-only
 // blend changes a turn request into raw wheel lock precisely when yaw can
 // still be large: the recorded CP0 spin starts at beta=0, yaw=1.57 rad/s.
 // Braking changes tyre forces, not the meaning of the turn request. Keep
 // countersteering while the brake is pressed; brake force itself is intact.
 // Do not change gas, braking, wheel mode or the physical pose.
 return nativeDriftSteering(stick*NativeDriftTurnLimit,s.yaw,s.beta);
}
}
