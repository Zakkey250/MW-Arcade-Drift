#pragma once
#include "../src/Controller.h"
namespace mcdai {
// AI commonly uses small steering inputs. Learn their measured yaw polarity
// without forcing the player's large-lock calibration manoeuvre on race AI.
struct Polarity {
 float time=0,vote=0;
 float Observe(const mcd::Sample& s){
  const float values[]={s.dt,s.speed,s.steer,s.yaw,s.beta,s.brake,s.handbrake};
  for(float value:values)if(!std::isfinite(value))return 0;
  if(!s.safe||s.speed<12||s.brake>.1f||s.handbrake>.1f||
     std::abs(s.beta)>.12f||std::abs(s.steer)<.015f||std::abs(s.yaw)<.08f)return 0;
  if(!std::isfinite(s.dt)||s.dt<=0||s.dt>.05f)return 0;
  time+=s.dt;vote+=(s.steer*s.yaw>0?1.f:-1.f)*s.dt;
  if(time<.5f)return 0;
  if(std::abs(vote)<time*.85f){time=vote=0;return 0;}
  return vote>0?1.f:-1.f;
 }
};
}
