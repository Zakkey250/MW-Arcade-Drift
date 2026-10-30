#pragma once
#include "../src/PlayerInputResponse.h"
inline void playerInputResponseTests(){
 using namespace mcd;
 for(float dt:{1.f/30,1.f/60,1.f/120})for(float side:{-1.f,1.f}){
  SpecialController human,ai;human.sign=ai.sign=1;
  auto s=sample(dt);s.speed=30;s.beta=side*.2f;s.steer=side*.6f;s.nativeTurnValid=true;s.nativeTurn=side*.4f;
  for(int i=0;i<240;++i){
   auto h=human.step(s,true,side*1.4f,true,true),a=ai.step(s,true,-side*1.4f,true,false);
   require(h.apply==a.apply&&h.deltaYaw==a.deltaYaw&&h.deltaSpeed==a.deltaSpeed,"human AI share identical physics; map yaw cannot affect it");
   require(h.deltaDirection==0&&std::abs(h.deltaYaw)<=std::min(human.tuning.yawAccelLimit,6.f)*dt+.00001f,"no lateral rail and bounded angular help");
  }
  s.steer=0;require(!human.step(s).apply,"neutral immediately releases special assistance");
  s.safe=false;require(!ai.step(s).apply&&ai.phase==Phase::Grip,"contact invalidates assistance");
 }
 for(float lossPercent:{0.f,25.f,50.f,100.f}){
  SpecialController c;c.sign=1;c.tuning.speedLossPercent=lossPercent;
  auto s=sample(.01f);s.speed=25;s.beta=.2f;s.nativeTurnValid=true;s.nativeTurn=.4f;
  for(int i=0;i<100;++i)c.step(s);
  s.speedBeforeStep=25.02f;auto o=c.step(s);
  require(o.apply&&o.deltaYaw==0&&o.deltaDirection==0&&o.deltaSpeed==0,"special delegates handling to native forces without body writes");
  s.climbing=true;require(c.step(s).deltaSpeed==0,"uphill loss is not refunded");s.climbing=false;
  s.speedBeforeStep=26;require(c.step(s).deltaSpeed==0,"impact loss not refunded");
  s.speedBeforeStep=25.02f;s.brake=1;require(c.step(s).deltaSpeed==0,"intentional braking never refunded");
  s.brake=0;s.gas=0;require(c.step(s).deltaSpeed==0,"lifting never refunds speed");
  s.gas=1;s.speedBeforeStep=0;require(c.step(s).deltaSpeed==0,"no historical target-speed recovery");
 }
 require(ParseControlDevice(L"Wheel")==ControlDevice::Wheel&&ParseControlDevice(L"wHeEl")==ControlDevice::Wheel,"wheel option case insensitive");
 {ModeController human;human.sign=1;human.select(DrivingMode::Special);auto s=sample(.01f);s.steer=.7f;s.brake=0;s.nativeTurnValid=true;s.nativeTurn=.8f;s.beta=.3f;
  for(int i=0;i<100;++i){auto o=human.step(s,true,std::numeric_limits<float>::quiet_NaN(),true);require(o.deltaDirection==0,"human assistance never cancels adequate native tyre turn");}
  s.steer=-.3f;for(int i=0;i<40;++i){auto o=human.step(s,true,std::numeric_limits<float>::quiet_NaN(),true);require(o.deltaDirection==0,"human countersteer keeps native trajectory response");}
 }
 for(auto value:{L"",L"PadKeyboard",L"Pad/Keyboard",L"invalid"})require(ParseControlDevice(value)==ControlDevice::PadKeyboard,"missing or invalid input mode uses pad keyboard");
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  PlayerInputResponse pad,wheel;wheel.device=ControlDevice::Wheel;auto s=sample(dt);s.steer=s.gas=1;
  auto first=pad.step(s,true);
  require(first.steer>0&&first.steer<1&&first.gas>0&&first.gas<1,"digital step softened");
  for(int i=0;i<30;++i){auto exact=wheel.step(s,true);require(exact.steer==s.steer&&exact.gas==s.gas&&exact.brake==s.brake,"wheel continuous controls unchanged");pad.step(s,true);}
  require(pad.steer==1&&pad.gas==1,"pad reaches full demand without amplitude loss");
  s.steer=0;s.gas=0;s.brake=1;auto released=pad.step(s,true);
  require(released.steer==0&&released.gas==0&&released.brake==1,"release and braking immediate");
  Output o;o.deltaYaw=.03f;o.deltaDirection=.02f;pad.apply(o);
  require(std::abs(o.deltaYaw-.0105f)<1e-6f&&std::abs(o.deltaDirection-.007f)<1e-6f,"full brake softens assistance while retaining brake initiated drift");
  s.brake=0;pad.step(s,true);require(pad.brakeWeight>0&&pad.brakeWeight<1,"brake release restores assistance progressively");
  s.steer=-1;s.gas=1;for(float t=0;t<.3f;t+=dt)pad.step(s,true);
  require(pad.steer==-1&&pad.gas==1,"opposite full demand is reachable");
  s.safe=false;pad.step(s,true);require(pad.steer==0&&pad.gas==0,"unsafe frame clears input history");
  s.safe=true;auto raw=pad.step(s,false);require(raw.steer==s.steer&&raw.gas==s.gas&&pad.steer==0,"normal disabled mode bypasses and clears history");
 }
 PlayerInputResponse pad;auto s=sample(.01f);s.steer=.3f;s.gas=.4f;
 for(int i=0;i<100;++i)pad.step(s,true);
 require(pad.steer==s.steer&&pad.gas==s.gas,"partial analog requests retain exact amplitude");
 s.dt=1;pad.step(s,true);require(pad.steer==0&&pad.gas==0,"pause clears response history");
}
