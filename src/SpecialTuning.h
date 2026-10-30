#pragma once
#include <algorithm>
// Arcade-derived Special response. Legacy keys remain readable; route-vector
// correction and native restoring-force cancellation are deliberately absent.
#define MCD_SPECIAL_FIELDS(X) \
 X(entrySpeedMps,12.f,8.f,50.f) \
 X(exitSpeedMps,9.f,8.f,40.f) \
 X(entrySteer,.22f,.08f,.8f) \
 X(entrySlipRad,.10f,.03f,.3f) \
 X(entryHoldSeconds,.06f,.02f,.5f) \
 X(neutralExitSeconds,.40f,.05f,1.f) \
 X(baseSlipRad,.20f,.05f,.4f) \
 X(slideSlipRad,.24f,0.f,.35f) \
 X(handbrakeSlipRad,.08f,0.f,.2f) \
 X(angleSlew,.65f,.1f,1.5f) \
 X(transferSeconds,.22f,.08f,.8f) \
 X(targetYawMax,.95f,.1f,1.6f) \
 X(targetPathSpeed,32.f,5.f,60.f) \
 X(pathAssistLimit,24.f,0.f,45.f) \
 X(pathSlew,.85f,.1f,1.5f) \
 X(slipGain,3.4f,.5f,6.f) \
 X(yawAccelLimit,4.5f,.5f,8.f) \
 X(yawJerkLimit,12.f,2.f,24.f) \
 X(blendSeconds,.25f,.1f,.8f) \
 X(frontGripPercent,85.f,20.f,100.f) \
 X(rearGripPercent,40.f,20.f,100.f) \
 X(rearStaticGripPercent,80.f,20.f,100.f) \
 X(rearLateralGripPercent,20.f,10.f,100.f) \
 X(speedLossPercent,50.f,0.f,100.f)
namespace mcd {
#define MCD_SPECIAL_MEMBER(n,d,lo,hi) float n=d;
struct SpecialTuning { MCD_SPECIAL_FIELDS(MCD_SPECIAL_MEMBER) };
#undef MCD_SPECIAL_MEMBER
}
