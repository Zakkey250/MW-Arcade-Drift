#pragma once
#include "Controller.h"
#include "SpecialTuning.h"
#include <limits>
namespace mcd {
// Reuse Arcade entry, handbrake, countersteer, transfer and release state.
// Special never rotates linear velocity toward steering or a map line.
struct SpecialController : Controller {
 SpecialTuning tuning;
 Output step(const Sample& s,bool=false,float=std::numeric_limits<float>::quiet_NaN(),bool=false,bool=false){
  auto& base=Controller::tuning;
  base.entrySpeedMps=tuning.entrySpeedMps;
  base.entryHoldSeconds=tuning.entryHoldSeconds;
  base.neutralExitSeconds=tuning.neutralExitSeconds;
  base.baseSlipRad=tuning.baseSlipRad;
  base.slideSlipRad=tuning.slideSlipRad;
  base.handbrakeSlipRad=tuning.handbrakeSlipRad;
  base.targetYawMax=tuning.targetYawMax;
  base.targetPathSpeed=tuning.targetPathSpeed;
  base.pathAssistLimit=0;
  poweredCounterHold=true;
  base.refundFraction=base.recoveryAccel=base.refundMaxAccel=0;
  // Power may arm Arcade after a measured native turn; no virtual input.
  if(phase==Phase::Grip&&sign&&s.safe&&s.dt>0&&s.dt<=.05f&&
     std::isfinite(s.speed)&&s.speed>=tuning.entrySpeedMps&&s.speed<=120&&
     std::isfinite(s.beta)&&std::abs(s.beta)<.8f&&std::isfinite(s.yaw)&&std::abs(s.yaw)<3&&
     std::isfinite(s.steer)&&std::abs(s.steer)>=tuning.entrySteer&&
     std::isfinite(s.gas)&&s.gas>.5f&&s.nativeTurnValid&&std::abs(s.nativeTurn)>.04f){
   phase=Phase::Armed;held=0;direction=s.steer*sign>0?1.f:-1.f;armedHandbrake=false;
  }
  auto out=Controller::step(s);
  // Arcade supplies entry/presentation state only. Special handling is
  // produced by scoped native tyre/aero parameters in the suspension solver.
  // Never write chassis yaw, travel direction or speed, even in free roam.
  out.deltaDirection=out.deltaYaw=out.deltaSpeed=0;pathAssist=0;
  return out;
 }
};
}
