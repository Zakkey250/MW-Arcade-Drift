#pragma once
#include "Controller.h"
namespace mcd {
enum class DriveLayout {Unknown, FWD, RWD, AWD};
inline DriveLayout driveLayout(float frontShare){
 if(!std::isfinite(frontShare)||frontShare<0.f||frontShare>1.f)return DriveLayout::Unknown;
 return frontShare==1.f?DriveLayout::FWD:frontShare==0.f?DriveLayout::RWD:DriveLayout::AWD;
}
inline const char* driveName(DriveLayout d){
 return d==DriveLayout::FWD?"FWD":d==DriveLayout::RWD?"RWD":d==DriveLayout::AWD?"AWD/4WD":"Unknown";
}
// Presentation-only envelope. No tire force, engine state or gearbox writes.
struct Presentation {
 float blend=0,spin=0;
 DriveLayout drive=DriveLayout::Unknown;
 void step(float dt,bool eligible,float beta,float gas,float brake,DriveLayout layout){
  if(drive!=layout){spin=0;drive=layout;}
  const float target=eligible?clamp((std::abs(beta)-.025f)/.075f,0.f,1.f):0.f;
  blend+=clamp(target-blend,-2.5f*dt,4.f*dt);
  const bool powerSpin=drive==DriveLayout::RWD||drive==DriveLayout::AWD;
  const float wanted=(powerSpin?target:0.f)*clamp(gas,0.f,1.f)*(brake<.05f?1.f:0.f)*.45f;
  spin+=clamp(wanted-spin,-2.f*dt,1.5f*dt);
 }
 float steer(float native) const {return native*(1.f-2.f*blend);}
 float wheel(float native,unsigned index) const {return index<4&&(drive==DriveLayout::AWD||(drive==DriveLayout::RWD&&index>=2))?native*(1.f+spin):native;}
 float rpm(float native,float redline) const {
  if(drive!=DriveLayout::RWD&&drive!=DriveLayout::AWD)return native;
  if(!std::isfinite(native)||!std::isfinite(redline)||native<0||redline<1000)return native;
  return std::max(native,std::min(native*(1.f+spin),redline*.995f));
 }
};
// GetWheelSteer returns turns (the render bridge multiplies by 2*pi).
// Smooth the final visible angle, not just the reversal multiplier: native
// steering may center in one step while the multiplier is still nonzero.
struct FrontSteering {
 float angle[2]{};
 bool initialized=false,active=false;
 void step(float dt,const float (&native)[2],float reversal){
  if(!initialized){angle[0]=native[0];angle[1]=native[1];initialized=true;}
  if(reversal>0)active=true;
  if(!active){angle[0]=native[0];angle[1]=native[1];return;}
  bool settled=reversal==0;
  for(unsigned i=0;i<2;i++){
   const float target=native[i]*(1.f-2.f*reversal);
   const float change=(target-angle[i])*(1.f-std::exp(-dt/.14f));
   angle[i]+=clamp(change,-.30f*dt,.30f*dt);
   settled=settled&&std::abs(target-angle[i])<.0001f;
  }
  if(settled){active=false;angle[0]=native[0];angle[1]=native[1];}
 }
};
// Exact callsites, not module-wide/range exemptions. Unknown/physics callers
// always receive the unmodified engine value (including AT shift decisions).
inline bool presentationRPMCaller(uint32_t caller){
 return caller==0x69419d || caller==0x6f1536 || caller==0x6f1568 || caller==0x6b73fb;
}
}
