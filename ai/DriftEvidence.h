#pragma once
#include "../src/SurfaceMotion.h"
#include "../src/Controller.h"
namespace mcdai {
// Diagnostic-only physical-clock measurement. No state here affects driving.
struct DriftEvidence {
 std::uint64_t token=0;double simulation=0,lastMoved=0;float seconds=0,metres=0;unsigned rejection=0;
 mcd::Vector previous{},start{},end{};bool previousValid=false;
 bool Step(std::uint64_t current,const mcd::Sample& s,mcd::Vector position,bool powerSpin,float minimumThrottle=.9f){
  if(token!=current){*this={};token=current;}
  const float distance=previousValid?mcd::length(mcd::add(position,mcd::mul(previous,-1))):0;
  const bool clock=std::isfinite(s.dt)&&s.dt>0&&s.dt<=.05f;
  if(clock)simulation+=s.dt;
  if(distance>.0001f)lastMoved=simulation;
  // Some rigid-body poses publish below the integrator frequency. A bounded
  // 100ms motion window rejects an actually stationary spinning car without
  // breaking a real continuous slide on a repeated pose sample.
  rejection=!clock?1:!s.safe?2:s.speed<=12?3:s.gas<=minimumThrottle||s.brake>=.05f||s.handbrake>=.05f?4:
   !std::isfinite(s.beta)||std::abs(s.beta)<mcd::pi/6||std::abs(s.beta)>mcd::pi/4?5:
   !previousValid||simulation-lastMoved>.1?6:distance>=std::max(6.f,s.speed*.1f)?7:!powerSpin?8:0;
  const bool ok=!rejection;
  previous=position;previousValid=true;
  if(!ok)return false;
  if(seconds==0)start=position;end=position;seconds+=s.dt;metres+=distance;return true;
 }
 void End(){seconds=metres=0;}
};
}
