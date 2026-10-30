#pragma once
#include <array>
#include <initializer_list>
#include "../src/NativePadSteering.h"
#include "../src/NativeDriftSteering.h"
inline void nativePadSteeringTests(){
 for(float side:{-1.f,1.f}){
  const float input=mcd::nativeDriftSteering(side*.5f,side*.5f,side*.275f);
  require(std::abs(input+side*.27f)<.0001f,"native equilibrium requests countersteer without a fixed body angle");
  mcd::NativeSteeringSample car{22,side*.5f,side*.8f,true};
  const float requested=side,assisted=mcd::nativePadDriftSteering(requested,car,0,0);
  require(std::isfinite(assisted)&&std::abs(assisted)<=1&&assisted*side<.5f,"pad full lock unwinds through real steering during a powered slide");
  require(mcd::nativePadDriftSteering(requested,car,.5f,0)==assisted,"braking retains the steering request meaning and countersteer");
  require(mcd::nativePadDriftSteering(requested,car,0,1)==requested,"handbrake entry retains driver steering");
  car.safe=false;require(mcd::nativePadDriftSteering(requested,car,0,0)==requested,"unsafe/airborne car receives no slide steering assistance");
  car.safe=true;car.beta=side*1.f;car.yaw=side*.8f;
  require(mcd::nativePadDriftSteering(requested,car,0,0)*side<0,"grounded large slip retains native countersteer instead of reverting to full inward lock");
  car.beta=0;car.yaw=side*1.57f;
  require(mcd::nativePadDriftSteering(side*.1f,car,0,0)*side<-.8f,"crossing zero slip cannot drop yaw stabilization during a direction change");
 }
 // On the observed local linear model the input feedback must damp an
 // initial yaw/slip perturbation. This is a control regression, not gameplay
 // acceptance or a claim that every car has this linear tyre response.
 for(const auto& model:std::initializer_list<std::array<float,6>>{
   {2.274338f,2.337665f,.167753f,-1.289235f,-2.225992f,.766364f},
   {3.53f,3.25f,.071f,-1.935f,-3.429f,.832f},
   {3.75f,1.31f,-.12f,-2.04f,-4.09f,.804f}}){float yaw=.5f,beta=.3f;
  for(unsigned i=0;i<600;++i){const float u=mcd::nativeDriftSteering(0,yaw,beta);
   const float rdot=model[0]*u+model[1]*beta+model[2]*yaw,bdot=model[3]*u+model[4]*beta+model[5]*yaw;
   yaw+=rdot/120;beta+=bdot/120;}
  require(std::abs(yaw)<.005f&&std::abs(beta)<.005f,"native input feedback damps observed yaw/slip mode");}
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  mcd::NativePadSteering f;std::uint64_t now=1000;
  require(f.step(1,now,dt,0,true)==0,"native pad starts from real input");
  float previous=0;
  for(unsigned n=0;n<30;++n){now+=unsigned(dt*1000);const float v=f.step(1,now,dt,1,true);
   require(v>=previous&&v-previous<=dt/.12f+.0001f&&v<=1,"digital pad has bounded native steering slew");previous=v;}
  require(previous==1,"full steering remains available");
  require(f.step(1,++now,dt,0,true)==0,"centering is immediate");
  require(f.step(2,++now,dt,-.5f,true)==-.5f,"another vehicle cannot inherit old steering");
  require(f.step(2,now+500,dt,.5f,true)==.5f,"resume from pause starts from current request");
  require(f.step(2,now+501,dt,-1,false)==-1&&f.identity==0,"wheel or normal mode bypasses and clears pad smoothing");
 }
}
