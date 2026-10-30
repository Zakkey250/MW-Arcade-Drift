#pragma once
#include "Controller.h"
#include <cwctype>
#include <string_view>
namespace mcd {
enum class ControlDevice { PadKeyboard, Wheel };
inline ControlDevice ParseControlDevice(std::wstring_view value){
 constexpr std::wstring_view wheel=L"wheel";
 if(value.size()!=wheel.size())return ControlDevice::PadKeyboard;
 for(size_t i=0;i<wheel.size();++i)if(std::towlower(value[i])!=wheel[i])return ControlDevice::PadKeyboard;
 return ControlDevice::Wheel;
}
// Shapes only the authored drift assistance. Native mapped driving controls,
// pedal assignments and hardware calibration remain owned by the game.
struct PlayerInputResponse {
 ControlDevice device=ControlDevice::PadKeyboard;
 float steer=0,gas=0,brakeWeight=1;
 void reset(){steer=gas=0;brakeWeight=1;}
 Sample step(const Sample& raw,bool active){
  if(!active||device==ControlDevice::Wheel){reset();return raw;}
  const float values[]={raw.dt,raw.steer,raw.gas,raw.brake,raw.handbrake};
  for(float value:values)if(!std::isfinite(value)){reset();return raw;}
  if(!raw.safe||raw.dt<=0||raw.dt>.05f){reset();return raw;}
  Sample out=raw;
  const float requested=clamp(raw.steer,-1.f,1.f);
  // Full-lock digital input reaches lock in 120ms; countersteer is faster.
  // A released axis is immediately neutral, so assistance never steers alone.
  const bool counter=requested*steer<0;
  if(std::abs(requested)<.01f)steer=0;
  else steer+=clamp(requested-steer,-raw.dt/(counter?.08f:.12f),raw.dt/(counter?.08f:.12f));
  const float pedal=clamp(raw.gas,0.f,1.f);
  gas=pedal<gas?pedal:std::min(pedal,gas+raw.dt/.08f);
  // Braking is never delayed. Restore drift assistance progressively after
  // releasing a binary brake, instead of snapping straight back to full yaw.
  const float available=1.f-clamp(raw.brake,0.f,1.f);
  brakeWeight=available<brakeWeight?available:std::min(available,brakeWeight+raw.dt/.12f);
  out.steer=steer;out.gas=gas;
  return out;
 }
 void apply(Output& out)const {
  if(device==ControlDevice::PadKeyboard){
   const float weight=.35f+.65f*brakeWeight;
   out.deltaYaw*=weight;out.deltaDirection*=weight;
  }
 }
};
}
