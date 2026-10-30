#pragma once
#include "../src/SpecialTuning.h"
#include "DriftAIAPI.h"
namespace mcdai {
inline bool usesNativeRaceTuning(unsigned mode,unsigned scope){return mode==2&&(scope==Race||scope==LocalAutodriveRace);}
// Race AI has its own native-tyre calibration. A player's GUI/per-car JSON
// must not retune opponents. Native source-car attributes still determine
// the actual tyre forces; these values are bounded parameter percentages.
inline mcd::SpecialTuning nativeRaceTuning(float frontShare){
 mcd::SpecialTuning t;
 t.frontGripPercent=85;
 t.rearGripPercent=40;
 t.rearStaticGripPercent=80;
 t.rearLateralGripPercent=20;
 t.speedLossPercent=35;
 // Driven-axle calibration, not a model/course-name exception. An unknown
 // layout retains the conservative generic native parameters.
 if(std::isfinite(frontShare)&&frontShare==0){
  t.rearStaticGripPercent=80;t.rearLateralGripPercent=12;
 }else if(std::isfinite(frontShare)&&frontShare>0&&frontShare<1){
  t.rearGripPercent=30;t.rearStaticGripPercent=100;t.rearLateralGripPercent=16;
 }else if(std::isfinite(frontShare)&&frontShare==1){
  t.rearLateralGripPercent=16;
 }
 return t;
}
}
