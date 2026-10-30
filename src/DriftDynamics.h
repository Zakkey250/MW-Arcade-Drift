#pragma once
#include "Controller.h"
#include <limits>
namespace mcd {
// Observe the native physics response between consecutive committed yaw
// velocities. Opt-in diagnostic observation only; no direct game memory access.
struct YawResponse {
 bool valid=false;
 float committed=0,nativeAcceleration=0,filteredResistance=0,compensation=0;
 void reset(){*this={};}
 void observe(const mcd::Sample& s){
  nativeAcceleration=0;
  if(!valid||!s.safe||!std::isfinite(s.dt)||!std::isfinite(s.yaw)||s.dt<=0||s.dt>.05f){reset();return;}
  nativeAcceleration=std::clamp((s.yaw-committed)/s.dt,-60.f,60.f);
  filteredResistance+=(nativeAcceleration-filteredResistance)*s.dt/(.08f+s.dt);
 }
 void commit(const mcd::Sample& s,float delta,bool active){
  valid=active&&s.safe&&s.dt>0&&s.dt<=.05f;
  if(valid)committed=s.yaw+delta;else reset();
 }
};
}
