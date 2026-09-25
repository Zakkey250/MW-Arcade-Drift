#pragma once
#include "Controller.h"
namespace mcd {
// Learn only unambiguous single-pedal samples from the game's mapped actions.
struct PedalMapping {
 int gasAxis=-1,brakeAxis=-1,candidateGas=-1,candidateBrake=-1;
 float gasTime=0,brakeTime=0;
 void reset(){*this={};}
 void step(Sample& s,bool available,float left,float right){
  if(!available||!std::isfinite(left)||!std::isfinite(right)||left<0||left>1||right<0||right>1||s.dt<=0||s.dt>.05f){reset();return;}
  const float axes[2]={left,right};
  for(int action=0;action<2;++action){
   const float value=action?s.brake:s.gas,other=action?s.gas:s.brake;
   int& known=action?brakeAxis:gasAxis;int& candidate=action?candidateBrake:candidateGas;float& time=action?brakeTime:gasTime;
   int match=-1;
   if(value>.6f&&other<.05f)for(int i=0;i<2;++i)if(axes[i]>.6f&&axes[1-i]<.05f&&std::abs(axes[i]-value)<.15f)match=i;
   if(match<0){candidate=-1;time=0;continue;}
   if(known>=0&&known!=match){reset();return;}
   if(candidate!=match){candidate=match;time=0;}
   time+=s.dt;if(time>=.20f)known=match;
  }
  if(gasAxis<0||brakeAxis<0||gasAxis==brakeAxis)return;
  // Both triggers down is precisely where combined-axis drivers lose input.
  if(left>.05f&&right>.05f){s.gas=std::max(s.gas,axes[gasAxis]);s.brake=std::max(s.brake,axes[brakeAxis]);}
 }
};
}
