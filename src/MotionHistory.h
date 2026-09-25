#pragma once
#include "Controller.h"
#include "SurfaceMotion.h"
namespace mcd {
// Capture the velocity actually left by our setters, then compare it with the
// next physics sample. The loss can occur outside the hooked function itself.
struct MotionHistory {
 uint32_t body=0;uint64_t tick=0;
 float x=0,y=0,z=0,speed=0,normal=0;
 bool valid=false,refundEligible=false;
 void clear(){valid=refundEligible=false;}
 bool usable(const Sample& s,float normalSpeed) const {
  return s.safe&&s.dt>0&&s.dt<=.05f&&s.speed>=8&&s.speed<=120&&
   std::isfinite(s.handbrake)&&s.handbrake>=0&&s.handbrake<=1.01f&&std::isfinite(normalSpeed)&&std::abs(normalSpeed)<1.f&&
   std::isfinite(s.beta)&&std::abs(s.beta)<=.8f&&std::isfinite(s.yaw)&&std::abs(s.yaw)<=3;
 }
 void sample(Sample& s,uint32_t currentBody,uint64_t now,float vx,float vy,float vz,float normalSpeed=0,float nx=0,float ny=1,float nz=0){
  s.speedBeforeStep=0;s.nativeTurn=0;s.nativeTurnValid=false;
  if(!valid||body!=currentBody||now<tick||now-tick>100||!usable(s,normalSpeed)||
     !std::isfinite(vx)||!std::isfinite(vy)||!std::isfinite(vz)||std::abs(normalSpeed-normal)>.3f)return;
  Vector n{nx,ny,nz};auto previous=tangent(Vector{x,y,z},n),current=tangent(Vector{vx,vy,vz},n);
  const float angle=std::atan2(dot(n,cross(previous,current)),dot(previous,current));
  if(std::abs(angle)>.12f)return;
  s.nativeTurn=angle/s.dt;s.nativeTurnValid=true;
  // Compare total speeds, then express the previous speed at the current
  // grade. A pitch change alone must not look like lost kinetic speed.
  if(refundEligible&&s.gas>.5f&&s.brake<.05f&&s.handbrake<.05f)s.speedBeforeStep=std::sqrt(std::max(0.f,speed*speed-normalSpeed*normalSpeed));
 }
 void commit(const Sample& s,uint32_t currentBody,uint64_t now,float vx,float vy,float vz,bool drifting,float normalSpeed=0){
  clear();if(!usable(s,normalSpeed)||!std::isfinite(vx)||!std::isfinite(vy)||!std::isfinite(vz))return;
  body=currentBody;tick=now;x=vx;y=vy;z=vz;normal=normalSpeed;speed=std::hypot(std::hypot(vx,vz),vy);valid=true;
  refundEligible=drifting&&s.gas>.5f&&s.brake<.05f&&s.handbrake<.05f;
 }
};
}
