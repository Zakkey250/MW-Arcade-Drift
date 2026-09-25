#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Tuning.h"
namespace mcd {
constexpr float pi=3.14159265358979323846f;
inline float clamp(float x,float a,float b){return std::max(a,std::min(b,x));}
inline float wrap(float x){return std::remainder(x,2*pi);}
struct Sample {
 float dt=0,speed=0,steer=0,gas=0,brake=0,handbrake=0,beta=0,yaw=0;
 float speedBeforeStep=0;
 float nativeTurn=0;
 bool nativeTurnValid=false;
 bool safe=false;
};
inline void independentPedals(Sample& s,bool selectedPad,float left,float right){
 if(selectedPad&&std::isfinite(left)&&std::isfinite(right)&&left>=0&&left<=1&&right>=0&&right<=1){
  s.brake=std::max(s.brake,left);s.gas=std::max(s.gas,right);
 }
}
enum class Phase { Grip, Armed, Drift, Exit };
struct Output {float deltaYaw=0,deltaDirection=0,deltaSpeed=0,targetYaw=0,targetSlip=0;bool apply=false;unsigned handbrakeEvent=0;};
struct Controller {
 DriftTuning tuning;
 Phase phase=Phase::Grip;
 float held=0,elapsed=0,blend=0,direction=0,sign=0,learn=0,vote=0;
 float entrySpeed=0,neutralTime=0,driftBrakeHeld=0,tapGasGrace=0,targetAngle=0,targetTurn=0;
 float slide=0,reverseHeld=0,liftHeld=0,yawAccel=0,deliveredAccel=0;
 float pathAssist=0,recoverySoft=0,filteredNative=0;
 bool nativeInitialized=false;
 float handbrakePower=0,handbrakeWindow=0;
 bool handbrakeDown=true,handbrakeConsumed=true,armedHandbrake=false;
 unsigned exitReason=0; // 1 held brake, 2 throttle, 3 aligned, 4 slow, 5 timeout, 6 unsafe
 bool previousBrake=true,gasSeen=false,brakeConsumed=true;
 void reset(bool retainSign=true){handbrakePower=handbrakeWindow=0;handbrakeDown=handbrakeConsumed=true;armedHandbrake=false;pathAssist=recoverySoft=filteredNative=0;nativeInitialized=false;slide=reverseHeld=liftHeld=yawAccel=deliveredAccel=0;phase=Phase::Grip;held=elapsed=blend=direction=entrySpeed=neutralTime=driftBrakeHeld=tapGasGrace=targetAngle=targetTurn=0;previousBrake=true;brakeConsumed=true;gasSeen=false;exitReason=0;if(!retainSign){sign=learn=vote=0;}}
 Output step(const Sample& s){
  Output o{};
  const float vals[]={s.dt,s.speed,s.steer,s.gas,s.brake,s.handbrake,s.beta,s.yaw};
  for(float v:vals)if(!std::isfinite(v)){reset();exitReason=6;return o;}
  if(!s.safe||s.dt<=0||s.dt>0.05f||s.speed<8||s.speed>120||std::abs(s.beta)>.8f||std::abs(s.yaw)>3 ){reset();exitReason=6;return o;}
  const bool brake=s.brake>.25f;
  const bool wasHandbrake=handbrakeDown;
  if(s.handbrake>.25f)handbrakeDown=true;else if(s.handbrake<.1f)handbrakeDown=false;
  if(!handbrakeDown)handbrakeConsumed=false;
  if(phase==Phase::Grip&&!sign&&s.speed>12&&std::abs(s.steer)>.2f&&std::abs(s.yaw)>.04f&&std::abs(s.beta)<.12f&&!brake&&!handbrakeDown){
   learn+=s.dt;vote+=(s.steer*s.yaw>0?1.f:-1.f)*s.dt;
   if(learn>=.5f){if(std::abs(vote)>learn*.8f)sign=vote>0?1.f:-1.f;else learn=vote=0;}
  }
  if(!brake)brakeConsumed=false;
  if(phase==Phase::Grip&&((brake&&!brakeConsumed)||(handbrakeDown&&!handbrakeConsumed))&&sign&&s.speed>=tuning.entrySpeedMps&&std::abs(s.steer)>=.08f){phase=Phase::Armed;armedHandbrake=handbrakeDown&&!handbrakeConsumed;held=0;direction=s.steer*sign>0?1.f:-1.f;}
  if(phase==Phase::Armed){
   held+=s.dt;
   if(s.steer*sign*direction<.05f){phase=Phase::Grip;}
   else if(held>=tuning.entryHoldSeconds){if(armedHandbrake){handbrakeWindow=.25f;handbrakeConsumed=true;o.handbrakeEvent=1;}phase=Phase::Drift;pathAssist=0;recoverySoft=1;nativeInitialized=false;brakeConsumed=true;elapsed=blend=neutralTime=0;entrySpeed=s.speed;slide=reverseHeld=liftHeld=yawAccel=deliveredAccel=0;targetAngle=s.beta;targetTurn=0;exitReason=0;}
  }
  previousBrake=brake;
  if(phase==Phase::Drift){
   elapsed+=s.dt;
   // Braking/coasting does not cancel. Never repay deliberate brake deceleration.
   if(s.brake>.05f||s.handbrake>.05f||wasHandbrake)entrySpeed=std::min(entrySpeed,s.speed);
   const bool aligned=std::abs(s.steer)<.1f&&std::abs(s.beta)<.055f&&std::abs(s.yaw)<.18f;
   neutralTime=std::abs(s.steer)<.08f?neutralTime+s.dt:0;
   if(aligned||neutralTime>tuning.neutralExitSeconds)exitReason=3;
   else if(s.speed<10)exitReason=4;
   if(exitReason)phase=Phase::Exit;
  }
  // Exit returns control immediately. Never drive native yaw toward zero.
  if(phase==Phase::Exit){handbrakePower=handbrakeWindow=0;handbrakeConsumed=true;armedHandbrake=false;phase=Phase::Grip;pathAssist=0;recoverySoft=1;nativeInitialized=false;brakeConsumed=true;blend=slide=yawAccel=deliveredAccel=targetAngle=targetTurn=reverseHeld=0;return o;}
  if(phase!=Phase::Drift)return o;
  blend=std::min(1.f,blend+s.dt/.18f);
  const bool neutral=std::abs(s.steer)<.08f;
  if(neutral){handbrakePower=handbrakeWindow=0;handbrakeConsumed=true;pathAssist=0;recoverySoft=1;nativeInitialized=false;yawAccel=deliveredAccel=0;blend=0;targetTurn=0;targetAngle=s.beta;return o;}
  const float signedSteer=clamp(s.steer*sign,-1.f,1.f);
  reverseHeld=signedSteer*direction<-.65f?reverseHeld+s.dt:0;
  // A short countersteer unwinds the slide. Sustained opposite input transfers
  // the slide direction, through a rate-limited target instead of a sign jump.
  if(reverseHeld>=.3f&&(std::abs(s.beta)<.08f||s.beta*direction<=0)){direction=-direction;reverseHeld=0;handbrakePower=handbrakeWindow=0;handbrakeConsumed=true;}
  const float relative=signedSteer*direction;
  // One bounded pulse per press, never an accumulating torque impulse.
  // Counter/transfer/neutral clear both the active pulse and its stored request.
  if(relative<-.05f){handbrakePower=handbrakeWindow=0;handbrakeConsumed=true;}
  else {
   if(handbrakeDown&&!handbrakeConsumed&&relative>=.08f){handbrakeWindow=.25f;handbrakeConsumed=true;o.handbrakeEvent=2;}
   const float powerTarget=handbrakeWindow>0?1.f:0.f;
   handbrakePower+=clamp(powerTarget-handbrakePower,-.8f*s.dt,5.f*s.dt);
   handbrakeWindow=std::max(0.f,handbrakeWindow-s.dt);
  }
  slide=clamp(slide+(1.2f*relative+.2f*s.gas-.4f)*s.dt,.15f,1.f);
  const float counter=clamp((-relative-.05f)/.6f,0.f,1.f);
  const float requested=direction*(tuning.baseSlipRad+tuning.slideSlipRad*slide+tuning.handbrakeSlipRad*handbrakePower)*(1.f-.85f*counter);
  const bool transferring=neutral||targetAngle*direction<0||relative<-.1f;
  const float angleRate=transferring?.12f+.10f*counter:.6f;
  targetAngle+=clamp(requested-targetAngle,-angleRate*s.dt,angleRate*s.dt);
  // Unwind the previous slide before asking for opposite path assistance.
  // A shared sign gate prevents the two target controllers fighting each other.
  // Smoothly add mid/high-speed path authority; entry/counter slew limits stay intact.
  float speedBlend=clamp((s.speed-tuning.boostStartKmh/3.6f)/((tuning.boostEndKmh-tuning.boostStartKmh)/3.6f),0.f,1.f);
  speedBlend=speedBlend*speedBlend*(3.f-2.f*speedBlend);
  const float turnGain=1.f+tuning.highSpeedGain*speedBlend;
  float requestedTurn=signedSteer*std::min(tuning.targetYawMax,tuning.targetPathSpeed/s.speed)*turnGain*(1.f+tuning.handbrakeTurnGain*handbrakePower);
  if(requestedTurn*targetAngle<0)requestedTurn=0;
  const float turnRate=transferring||targetTurn*signedSteer<0?1.8f:1.f;
  targetTurn+=clamp(requestedTurn-targetTurn,-turnRate*s.dt,turnRate*s.dt);
  o.targetSlip=targetAngle;o.targetYaw=targetTurn;
  // Native path rotation was measured between our previous committed velocity
  // and this sample. It includes MW tire response. Never subtract it to force
  // a lower turn rate: only supply missing rotation in the requested direction.
  const float native=s.nativeTurnValid&&std::isfinite(s.nativeTurn)?clamp(s.nativeTurn,-2.f,2.f):s.yaw;
  const float turnSign=targetTurn>0?1.f:targetTurn<0?-1.f:0;
  const float extra=neutral||relative<-.05f?0.f:turnSign*clamp(std::abs(targetTurn)-turnSign*native,0.f,tuning.pathAssistLimit/s.speed);
  // Bound re-entry of trajectory assistance, including release of countersteer.
  // Drop immediately when native tires already provide the requested turn.
  if(relative<-.05f||pathAssist*extra<0)pathAssist=0;
  const float pathTarget=extra*blend;
  pathAssist+=clamp(pathTarget-pathAssist,-tuning.pathSlew*s.dt,tuning.pathSlew*s.dt);
  if(std::abs(pathAssist)>std::abs(pathTarget))pathAssist=pathTarget;
  o.deltaDirection=pathAssist*s.dt;
  // Match body yaw to the resulting path plus the slip error. Unlike alpha8's
  // weak spring alone, this compensates the observed native yaw/path mismatch.
  // Active drift only; Grip/Exit never write or converge native yaw to zero.
  // Tire response is noisy at the physics-step scale. Filter only the yaw
  // reference; the no-extra-turn gate above still uses the current native rate.
  if(!nativeInitialized){filteredNative=native;nativeInitialized=true;}
  filteredNative+=(native-filteredNative)*(s.dt/(.08f+s.dt));
  const float predictedPath=filteredNative+pathAssist;
  const float requestedSoft=std::max(counter,targetAngle*direction<0?.65f:0.f);
  recoverySoft=std::max(requestedSoft,recoverySoft-2.5f*s.dt);
  const float soften=recoverySoft;
  const float slipGain=3.f-2.2f*soften;
  const float desiredYaw=clamp(predictedPath+slipGain*((neutral?0.f:targetAngle)-s.beta),-1.6f,1.6f);
  const float accelLimit=6.f-4.5f*soften;
  const float requestedAccel=clamp((desiredYaw-s.yaw)*(10.f-6.f*soften),-accelLimit,accelLimit);
  const float jerk=16.f-8.f*soften;
  yawAccel+=clamp(requestedAccel-yawAccel,-jerk*s.dt,jerk*s.dt);
  // Limit the delivered acceleration AFTER entry blending. Otherwise blend
  // growth adds a second jerk term, bypassing the internal slew limit.
  const float effectiveTarget=yawAccel*blend;
  deliveredAccel+=clamp(effectiveTarget-deliveredAccel,-jerk*s.dt,jerk*s.dt);
  // No hidden acceleration may build while counter output is suppressed.
  // Neutral/safety/counter inhibition takes precedence over smoothing.
  if(counter>0&&deliveredAccel*s.yaw>0){deliveredAccel=0;yawAccel=0;}
  o.deltaYaw=deliveredAccel*s.dt;
  const float loss=s.speedBeforeStep-s.speed;
  // Lower the reference after impact-like loss; do not restore it later.
  if(s.speedBeforeStep>0&&std::isfinite(loss)&&loss>=20.f*s.dt)entrySpeed=std::min(entrySpeed,s.speed);
  if(counter==0&&!transferring&&s.gas>.5f&&s.brake<.05f&&s.handbrake<.05f&&!wasHandbrake&&s.speedBeforeStep>0&&std::isfinite(loss)&&loss<20.f*s.dt){
   // Preserve naturally reached speed, never raise the reference from our own
   // correction (which is clamped to that reference). Braking lowers it above.
   entrySpeed=std::max(entrySpeed,s.speed);
   const float deficit=std::max(0.f,entrySpeed-s.speed);
   const float refund=tuning.refundFraction*std::max(0.f,loss);
   const float recovery=std::min(tuning.recoveryAccel,2.f*deficit)*s.dt;
   o.deltaSpeed=std::min({tuning.refundMaxAccel*s.dt,(refund+recovery)*blend,deficit});
  }
  o.apply=true;return o;
 }
};
}
