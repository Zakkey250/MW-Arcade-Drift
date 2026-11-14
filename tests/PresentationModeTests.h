#pragma once
inline void presentationModeTests(){
 using namespace mcd;
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  auto s=sample(dt);s.beta=.25f;s.steer=0;s.speed=25;
  DriftCamera camera;
  for(unsigned i=0;i<unsigned(1.f/dt);++i)camera.step(dt,driftCameraActive(DrivingMode::Special,s,false),s.beta);
  require(camera.orbit>.02f&&camera.zoom>.1f,"Special camera responds to measured slide with neutral steering and no Arcade phase");
  require(!driftCameraActive(DrivingMode::Arcade,s,false)&&!driftCameraActive(DrivingMode::Normal,s,true),"Arcade entry and Normal camera isolation retained");
  s.safe=false;require(!driftCameraActive(DrivingMode::Special,s,true),"airborne/collision state stops Special camera activation");s.safe=true;
  s.speed=0;require(!driftCameraActive(DrivingMode::Special,s,true),"stationary car cannot activate slide camera");
  for(auto drive:{DriveLayout::RWD,DriveLayout::AWD}){
   Presentation effect;
   for(unsigned i=0;i<unsigned(1.f/dt);++i)effect.step(dt,true,.25f,1,0,drive);
   require(effect.wheel(80,2)>200&&effect.wheel(-80,3)<-200,"visible powered-wheel speed is substantially increased in both directions");
   require(effect.smoke(.2f,2)>.5f&&effect.smoke(2.5f,3)>6&&effect.smoke(100,3)==100,"powered-wheel smoke is stronger without reducing native high-slip effects");
   if(drive==DriveLayout::RWD)require(effect.wheel(80,0)==80&&effect.smoke(.2f,0)==.2f,"RWD front wheels retain native rotation and smoke");
   require(effect.smoke(-.2f,2)==-effect.smoke(.2f,2),"left/right skid gets symmetric smoke without changing its sign");
   require(effect.smoke(.2f,4)==.2f,"invalid wheel is untouched");
   for(unsigned i=0;i<unsigned(1.f/dt);++i)effect.step(dt,false,0,0,0,drive);
   require(effect.wheel(80,2)==80&&effect.smoke(.2f,2)==.2f,"all powered-wheel effects return to native on exit");
  }
 }
 require(presentationWheelCaller(0x6b71c8)&&!presentationWheelCaller(0x6a0dbe),"wheel override is restricted to the verified presentation bridge");
 require(presentationSmokeCaller(0x6b720d)&&!presentationSmokeCaller(0)&&!presentationSmokeCaller(0x6b722c),"smoke override leaves physics and other tyre queries untouched");
}
