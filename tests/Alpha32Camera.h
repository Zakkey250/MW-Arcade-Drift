#pragma once
#include "Alpha32Controller.h"
#include "Alpha32Surface.h"
namespace alpha32 {
inline Vector cameraForward(Vector physics){return {physics.z,-physics.x,physics.y};}
struct DriftCamera {
 float orbit=0,roll=0,orbitVelocity=0,rollVelocity=0,zoom=0,zoomVelocity=0;
 static void spring(float& angle,float& velocity,float target,float dt,float speedLimit,float accelLimit){
  // Small fixed upper integration interval keeps behavior consistent across FPS.
  while(dt>0){float h=std::min(dt,1.f/240.f);dt-=h;
   float acceleration=clamp(36.f*(target-angle)-12.f*velocity,-accelLimit,accelLimit);
   velocity=clamp(velocity+acceleration*h,-speedLimit,speedLimit);
   angle+=velocity*h;
  }
 }
 void step(float dt,bool active,float beta){
  if(!std::isfinite(dt)||!std::isfinite(beta)||dt<=0||dt>.05f){*this={};return;}
  const float aim=active?clamp(beta*1.8f,-.4886922f,.4886922f):0;
  const float bank=active?clamp(-beta*.06f,-.0174533f,.0174533f):0;
  spring(orbit,orbitVelocity,aim,dt,.45f,.8f);
  spring(roll,rollVelocity,bank,dt,.045f,.12f);
  // Independent unsigned envelope avoids pulling back whenever steering changes sign.
  const float proximity=active?clamp(std::abs(beta)/.12f,0.f,1.f):0;
  spring(zoom,zoomVelocity,proximity,dt,.5f,.75f);
  // Output already clips at endpoints. Clear hidden spring debt there as well.
  if(zoom<=0){zoom=0;zoomVelocity=std::max(0.f,zoomVelocity);}
  else if(zoom>=1){zoom=1;zoomVelocity=std::min(0.f,zoomVelocity);}
 }
 bool transform(Vector& eye,Vector& center,Vector& up,Vector forward)const{
  const auto finite=[](Vector v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
  if(!finite(eye)||!finite(center)||!finite(up)||!finite(forward))return false;
  Vector offset=add(eye,mul(center,-1));float d=length(offset),u=length(up),f=length(forward);
  // Rear chase geometry only. Cockpit, bumper, look-back and side views fall back.
  if(d<3||d>25||u<.9f||u>1.1f||f<.9f||f>1.1f)return false;
  if(std::abs(orbit)<.00001f&&std::abs(roll)<.00001f&&std::abs(zoom)<.00001f)return false;
  const float behind=-dot(offset,forward)/(d*f);
  float weight=clamp((behind-.15f)/.5f,0.f,1.f);weight=weight*weight*(3-2*weight);
  if(weight<=0)return false;
  Vector axis=mul(up,1/u);
  // Positive orbit preserves the side view: negative orbit cancelled native slip framing.
  offset=rotateSurface(offset,axis,orbit*weight,0);
  // Zoom/height continue smoothly through the zero crossing of orbit.
  const float framing=clamp(zoom,0.f,1.f)*weight;
  const float closeDistance=std::min(d,std::max(3.75f,d*(1-.38f*framing)));
  offset=mul(offset,closeDistance/d);eye=add(center,offset);
  Vector view=mul(offset,-1/closeDistance);
  Vector right=cross(view,axis);float rightLength=length(right);
  if(rightLength>.001f){
   right=mul(right,1/rightLength);
   const float side=clamp(orbit/.20f,-1.f,1.f);
   // Aim into the visible road half. The car moves to the opposite screen half.
   center=add(center,add(mul(right,side*.25f*closeDistance*framing),mul(view,.35f*closeDistance*framing)));
  }
  // Keep a low rear-quarter viewpoint, but look gently down toward the road.
  // Height is relative to the native target; no terrain query is implied.
  const float height=dot(offset,axis);
  eye=add(eye,mul(axis,clamp(-.56f-height,-.82f,.20f)*framing));
  center=add(center,mul(axis,-.68f*framing));
  view=add(center,mul(eye,-1));view=mul(view,1/length(view));
  up=rotateSurface(up,view,roll*weight,0);
  return true;
 }
};
}
