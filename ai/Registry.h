#pragma once
#include "DriftAIAPI.h"
#include "Polarity.h"
#include "YawResponse.h"
#include "../src/ModeController.h"
#include "../src/MotionHistory.h"
#include <array>
namespace mcdai {
inline bool validLease(std::uint32_t value){return value>=250&&value<=2000;}
inline bool validBinding(const BindingV1& b){
 if(b.size!=sizeof(b)||b.version!=Version||!b.owner||!b.generation||b.vehicle<0x10000||!b.simHandle||
    (b.mode!=1&&b.mode!=2)||!validLease(b.leaseMs)||b.scope>LocalAutodriveRace||(b.scope!=FreeRoam?!b.race:b.race!=0))return false;
 for(auto value:b.reserved)if(value)return false;return true;
}
struct Entry {
 bool used=false,allow=false,intentLatched=false;
 BindingV1 binding{};std::uint64_t token=0,deadline=0,commandTick=0;
 std::uint32_t body=0;IntentV1 intent;std::uint64_t intentTick=0;
 Polarity polarity;YawResponse yawResponse;float pathRate=0,pathStrength=1,pathWeight=1; mcd::ModeController controller; mcd::MotionHistory motion;StateV1 state;
 void reset(){controller.reset();motion.clear();yawResponse.reset();pathRate=0;pathStrength=pathWeight=1;intentLatched=false;intent={};intentTick=0;state.eligible=0;state.phase=0;state.calibrated=controller.sign!=0;state.deltaYaw=state.deltaDirection=state.deltaSpeed=0;}
 bool fresh(std::uint64_t now)const{return allow&&now>=commandTick&&now-commandTick<=FreshMilliseconds;}
 // Handling belongs to the current race/vehicle lease, not the planner's
 // permission to add posture correction. A blocked/reversing driver still
 // has the same tyres. Expiry, release and mode/race changes remain vetoes.
 bool nativeHandlingFresh(std::uint64_t now)const{return used&&binding.mode==2&&binding.scope!=FreeRoam&&now<deadline&&now>=commandTick&&now-commandTick<=FreshMilliseconds;}
 bool intentFresh(std::uint64_t now)const{return fresh(now)&&intent.active&&now>=intentTick&&now-intentTick<=FreshMilliseconds;}
 mcd::Output step(const mcd::Sample& sample,std::uint64_t now){
  if(intentTick&&!intentFresh(now)){
   controller.reset();pathRate=0;intentLatched=false;auto neutral=sample;neutral.steer=neutral.brake=neutral.handbrake=0;
   controller.step(neutral);return {};
  }
  if(intentFresh(now)&&!intentLatched&&sample.safe){
   // A lease paused during recovery has no intervening physics command with
   // intent=0. Release the frozen controller's button latches before consuming
   // the first real entry pulse, including on initial acquisition.
   controller.reset();auto released=sample;released.steer=released.brake=released.handbrake=0;
   controller.step(released);intentLatched=true;
  }
  if(intentFresh(now)){
   // Map geometry controls native inputs only. No virtual steering sample,
   // route-driven velocity rotation, or map-supplied fixed slip angle.
   const auto arcade=controller.tuning;const auto special=controller.special.tuning;
   controller.tuning.pathAssistLimit=0;
   controller.special.tuning.speedLossPercent=35.f;
   auto output=controller.step(sample);
   controller.tuning=arcade;controller.special.tuning=special;
   output.deltaDirection=0;pathRate=0;
   if(sample.gas<.95f)output.deltaSpeed=0;
   return output;
  }
  pathRate=0;
  return controller.step(sample);
 }

 StateV1 view(std::uint64_t now)const{
  auto result=state;
  if(!fresh(now)||now<result.sampleTick||now-result.sampleTick>FreshMilliseconds){
   result.eligible=0;result.reason=allow?StaleCommand:Paused;
   result.deltaYaw=result.deltaDirection=result.deltaSpeed=0;
  }return result;
 }
};
// Caller serializes access. This registry has no game memory access or writes.
inline bool validRefresh(const RefreshV1& r,std::uint64_t now){return r.size==sizeof(r)&&r.version==Version&&validLease(r.leaseMs)&&!(r.flags&~3u)&&now<=UINT64_MAX-r.leaseMs;}
struct Registry {
 std::array<Entry,Capacity> entries{};std::uint64_t sequence=0;
 void expire(std::uint64_t now){for(auto& e:entries)if(e.used&&now>=e.deadline)e={};}
 Entry* find(std::uint64_t owner,std::uint64_t token){if(!owner||!token)return nullptr;for(auto& e:entries)if(e.used&&e.binding.owner==owner&&e.token==token)return &e;return nullptr;}
 Result acquire(const BindingV1& b,std::uint32_t body,std::uint64_t now,Entry*& out){
  out=nullptr;if(!validBinding(b)||!body||now>UINT64_MAX-b.leaseMs)return BadRequest;expire(now);
  for(auto& e:entries)if(e.used&&e.binding.vehicle==b.vehicle)return Busy;
  for(auto& e:entries)if(!e.used){
   if(sequence==UINT64_MAX)return Busy;
   e={};e.used=true;e.binding=b;e.body=body;e.token=++sequence;e.deadline=now+b.leaseMs;e.commandTick=now;
   e.controller.select(mcd::DrivingMode(b.mode));e.state.vehicle=b.vehicle;e.state.simHandle=b.simHandle;e.state.mode=b.mode;e.state.leased=1;e.state.token=e.token;out=&e;return Ok;
  }return Busy;
 }
 Result refresh(const RefreshV1& r,std::uint64_t now){
  if(!validRefresh(r,now))return BadRequest;
  expire(now);auto* e=find(r.owner,r.token);if(!e)return NotOwned;
  if((r.flags&ResetState)||!(r.flags&AllowCorrection)||!e->fresh(now))e->reset();
  e->allow=(r.flags&AllowCorrection)!=0;e->commandTick=now;e->deadline=now+r.leaseMs;return Ok;
 }
 Result release(std::uint64_t owner,std::uint64_t token){auto* e=find(owner,token);if(!e)return NotOwned;*e={};return Ok;}
 Result setIntent(const IntentV1& i,std::uint64_t now){
  if(i.size!=sizeof(i)||i.version!=Version||i.active>1||i.reserved||!std::isfinite(i.pathYaw)||!std::isfinite(i.targetSlip)||std::abs(i.pathYaw)>2||std::abs(i.targetSlip)>.65f)return BadRequest;
  expire(now);auto* e=find(i.owner,i.token);if(!e)return NotOwned;
  if(i.active&&!e->fresh(now))return NotReady;
  e->intent=i;e->intentTick=now;return Ok;
 }
 Result setPathAssist(const PathAssistV1& p,std::uint64_t now){
  if(p.size!=sizeof(p)||p.version!=Version||p.reserved||!std::isfinite(p.strength)||p.strength<.25f||p.strength>1)return BadRequest;
  expire(now);auto* e=find(p.owner,p.token);if(!e)return NotOwned;
  if(!e->intentFresh(now))return NotReady;
  e->pathStrength=p.strength;return Ok;
 }
};
}
