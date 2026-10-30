#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mcd {
// Filters the native steering request, never the chassis or its velocity.
// Wheel mode bypasses this class. Brakes, gas and handbrake are untouched.
struct NativePadSteering {
 std::uint32_t identity=0;std::uint64_t tick=0;float value=0;
 void reset(){identity=0;tick=0;value=0;}
 float step(std::uint32_t car,std::uint64_t now,float dt,float requested,bool active){
  if(!active||!car||!std::isfinite(dt)||dt<=0||dt>.05f||!std::isfinite(requested)||std::abs(requested)>1.01f){reset();return requested;}
  if(identity!=car||!tick||now<tick||now-tick>250){value=requested;identity=car;}
  tick=now;requested=std::clamp(requested,-1.f,1.f);
  if(std::abs(requested)<.01f)value=0;
  else {const float rate=dt/(requested*value<0?.08f:.12f);value+=std::clamp(requested-value,-rate,rate);}
  return value;
 }
};
}
