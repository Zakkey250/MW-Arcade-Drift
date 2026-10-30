#pragma once
#include "DrivingMode.h"
#include "SpecialController.h"
namespace mcd {
// Existing Controller is untouched; Arcade dispatch is exactly its old step.
struct ModeController : Controller {
 SpecialController special;
 DrivingMode mode=DrivingMode::Arcade;
 void reset(bool retainSign=true){Controller::reset(retainSign);special.reset(retainSign);special.sign=sign;}
 void select(DrivingMode next){if(mode==next)return;reset();mode=next;}
 Output step(const Sample& s,bool enhancedSpecial=false,float path=std::numeric_limits<float>::quiet_NaN(),bool playerSustain=false){
  if(mode==DrivingMode::Arcade)return Controller::step(s);
  if(mode==DrivingMode::Normal){reset();return {};}
  // The same native-input engine is used for players and owned AI.
  auto output=special.step(s,enhancedSpecial&&!playerSustain,path,enhancedSpecial,playerSustain);
  phase=special.phase;sign=special.sign;learn=special.learn;vote=special.vote;held=special.held;
  blend=special.blend;direction=special.direction;neutralTime=special.neutralTime;
  pathAssist=special.pathAssist;targetAngle=special.targetAngle;targetTurn=special.targetTurn;
  yawAccel=special.yawAccel;exitReason=special.exitReason;
  return output;
 }
};
}
