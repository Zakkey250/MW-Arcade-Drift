#include "../src/PedalMapping.h"
#include "../src/DriftCamera.h"
#include "../src/Controller.h"
#include "../src/SurfaceMotion.h"
#include "../src/MotionHistory.h"
#include "../src/Presentation.h"
#include <cstdio>
#include <windows.h>
#include <cstdlib>
#include <limits>
#include <fstream>
#include "../src/Settings.h"
#include "../src/ProfileFiles.h"
#include "Alpha32Camera.h"
int checks=0;
void require(bool b,const char* t){++checks;if(!b){printf("FAIL %s\n",t);exit(1);}}
mcd::Sample sample(float dt){mcd::Sample s;s.dt=dt;s.safe=true;s.speed=60;s.gas=1;s.steer=.6f;return s;}
void trigger(mcd::Controller& c,mcd::Sample& s){c.sign=1;s.brake=0;c.step(s);s.brake=1;for(float t=0;t<.067f;t+=s.dt)c.step(s);}
int main(){
 {
  namespace fs=std::filesystem;
  auto root=fs::temp_directory_path()/("MWArcadeDrift-profile-test-"+std::to_string(GetCurrentProcessId()));
  require(!fs::exists(root),"isolated profile fixture unique");fs::create_directories(root);
  for(auto n:{"default_drift.json","default_camera.json","settings.json"})fs::copy_file(fs::path("MWCriterionDrift/config")/n,root/n);
  auto first=mcd::loadVehicleProfile(root,"svj",true);
  require(fs::exists(root/"vehicles/SVJ/drift.json")&&fs::exists(root/"vehicles/SVJ/camera.json"),"entry creates named pair");
  require(first.enabled&&first.drift.targetYawMax==.8f,"default values copied unchanged");
  auto custom=mcd::readProfileJson(root/"vehicles/SVJ/drift.json");custom["enabled"]=false;custom["targetYawMax"]=.65;
  {std::ofstream f(root/"vehicles/SVJ/drift.json");f<<custom.dump();}
  require(!mcd::loadVehicleProfile(root,"SVJ",true).enabled,"entry preserves disabled custom profile");
  fs::remove(root/"vehicles/SVJ/camera.json");
  require(mcd::loadVehicleProfile(root,"SVJ",true).drift.targetYawMax==.65f,"partial pair creates only missing file");
  for(auto bad:{"../escape","CON","LPT1","COM9",""}){bool rejected=false;try{mcd::loadVehicleProfile(root,bad,true);}catch(...){rejected=true;}require(rejected,"unsafe vehicle name rejected");}
  auto d=mcd::readProfileJson(root/"default_drift.json"),c=mcd::readProfileJson(root/"default_camera.json");
  for(int i=0;i<5;++i){auto x=d,y=c;if(i==0)x["schemaVersion"]=2.5;if(i==1)x["enabled"]="yes";if(i==2)y["orbitGain"]=9;if(i==3)x["unknown"]=1;if(i==4)x["boostEndKmh"]=0;bool rejected=false;try{mcd::parseProfile(x,y);}catch(...){rejected=true;}require(rejected,"malformed pair rejected");}
  {std::ofstream f(root/"default_camera.json");f<<"{";}
  bool rejected=false;try{mcd::loadVehicleProfile(root,"NEWCAR",true);}catch(...){rejected=true;}
  require(rejected&&!fs::exists(root/"vehicles/NEWCAR"),"invalid default pair creates nothing");
  for(auto lang:{"auto","ja","en"}){std::ofstream(root/"settings.json")<<mcd::Json({{"schemaVersion",2},{"language",lang}}).dump();require(mcd::readPreferences(root).language==lang,"language setting read");}
  std::ofstream(root/"settings.json")<<R"({"schemaVersion":2,"language":"invalid"})";
  rejected=false;try{mcd::readPreferences(root);}catch(...){rejected=true;}require(rejected,"unsupported language rejected");
  fs::remove_all(root);
 }

 {
  std::ifstream df("MWCriterionDrift/config/drift.json"),cf("MWCriterionDrift/config/camera.json"),vf("MWCriterionDrift/config/vehicles.json");
  require(bool(df)&&bool(cf)&&bool(vf),"shipped configuration fixtures open");
  auto dj=mcd::Json::parse(df),cj=mcd::Json::parse(cf),vj=mcd::Json::parse(vf);
  auto cfg=mcd::parseSettings(dj,cj,vj);
  require(cfg.select("UNLISTED_ADDON").enabled,"unlisted add-on uses enabled default");
  vj["vehicles"]["gallardo"]={{"enabled",false},{"profile","mild"}};
  cfg=mcd::parseSettings(dj,cj,vj);
  require(!cfg.select("GALLARDO").enabled&&cfg.select("GALLARDO").profile=="mild","normalized vehicle override selects disabled mild profile");
  require(cfg.profiles.at("mild").highSpeedGain==.15f&&cfg.profiles.at("mild").entrySpeedMps==12,"partial profiles inherit default");
  for(int bad=0;bad<7;++bad){auto d=dj,c=cj,v=vj;
   if(bad==0)d["profiles"]["default"]["highSpeedGain"]=5;
   if(bad==1)d["profiles"]["default"]["boostEndKmh"]=100;
   if(bad==2)c["zoomSpeed"]="fast";
   if(bad==3)v["default"]["profile"]="missing";
   if(bad==4)v["vehicles"]["GALLARDO"]={{"enabled",true}};
   if(bad==5)c["zoomSpeeed"]=.5;
   if(bad==6)c["schemaVersion"]=2;
   bool rejected=false;try{mcd::parseSettings(d,c,v);}catch(const std::exception&){rejected=true;}
   require(rejected,"malformed/unsafe configuration rejected atomically");
  }
  mcd::Controller now;alpha32::Controller old;now.tuning=cfg.profiles.at("default");now.sign=old.sign=1;
  mcd::DriftCamera camera;alpha32::DriftCamera oldCamera;camera.tuning=cfg.camera;
  for(int i=0;i<6000;++i){
   auto a=sample(1.f/120);a.speed=35.f+50.f*std::abs(std::sin(i*.001f));a.steer=std::sin(i*.02f);a.beta=.2f*std::sin(i*.015f);a.yaw=.5f*std::sin(i*.01f);
   a.brake=i%300<10?1.f:0;a.handbrake=i%650<5?1.f:0;a.nativeTurnValid=true;a.nativeTurn=a.yaw;a.speedBeforeStep=a.speed+.01f;
   alpha32::Sample b;b.dt=a.dt;b.safe=a.safe;b.speed=a.speed;b.steer=a.steer;b.gas=a.gas;b.brake=a.brake;b.handbrake=a.handbrake;b.beta=a.beta;b.yaw=a.yaw;b.nativeTurnValid=a.nativeTurnValid;b.nativeTurn=a.nativeTurn;b.speedBeforeStep=a.speedBeforeStep;
   auto x=now.step(a);auto y=old.step(b);
   require(unsigned(now.phase)==unsigned(old.phase)&&std::abs(x.deltaYaw-y.deltaYaw)<1e-6f&&std::abs(x.deltaDirection-y.deltaDirection)<1e-6f&&std::abs(x.deltaSpeed-y.deltaSpeed)<1e-6f,"shipped default physics preserves alpha32 output");
   bool active=i%480<300;camera.step(a.dt,active,a.beta);oldCamera.step(a.dt,active,a.beta);
   mcd::Vector e{-5,0,0},c{},u{0,0,1};alpha32::Vector oe{-5,0,0},oc{},ou{0,0,1};camera.transform(e,c,u,{1,0,0});oldCamera.transform(oe,oc,ou,{1,0,0});
   require(std::abs(e.x-oe.x)+std::abs(e.y-oe.y)+std::abs(e.z-oe.z)+std::abs(c.x-oc.x)+std::abs(c.y-oc.y)+std::abs(c.z-oc.z)<1e-5f,"shipped camera JSON preserves alpha32 framing");
  }
  now.tuning=cfg.profiles.at("mild");now.reset(false);
  require(now.phase==mcd::Phase::Grip&&now.pathAssist==0&&now.sign==0&&now.tuning.highSpeedGain==.15f,"vehicle reset clears old dynamics while retaining selected profile");
 }

 // Mid/high speed authority: steady turn, gradual delivery, neutral and counter gates.
 for(float kmh:{90.f,120.f,180.f,240.f,300.f})for(float sign:{-1.f,1.f}){
  auto s=sample(1.f/120);s.speed=kmh/3.6f;s.steer=sign;s.beta=sign*.2f;
  s.nativeTurnValid=true;s.nativeTurn=sign*.55f;s.yaw=sign*.55f;
  mcd::Controller c;c.sign=1;c.direction=sign;c.phase=mcd::Phase::Drift;
  float previous=0;mcd::Output o;
  for(int i=0;i<360;++i){o=c.step(s);
   require(std::abs(o.targetYaw-previous)<=s.dt+.00001f,"stronger turn retains target rise limit");previous=o.targetYaw;
  }
  float oldTarget=std::min(.8f,36.f/s.speed);
  if(kmh<=120)require(std::abs(std::abs(o.targetYaw)-oldTarget)<.0001f,"low speed turn unchanged");
  else require(std::abs(o.targetYaw)>oldTarget*1.14f&&std::abs(o.targetYaw)<=oldTarget*1.301f,"mid high speed turn gains bounded authority");
  if(kmh==240)require(std::abs(c.pathAssist)>.14f,"240 kmh supplies missing path rotation above previous target");
  s.steer=-sign;o=c.step(s);require(o.deltaDirection==0,"counter immediately inhibits stronger path assistance");
  s.steer=0;o=c.step(s);require(o.deltaYaw==0&&o.deltaDirection==0,"neutral stops stronger assistance");
 }

 // Native target is not a measured car center and runtime FOV is unknown.
 // Synthetic box projection is diagnostic only; it cannot certify live framing.
 for(float sign:{-1.f,1.f}){
  mcd::DriftCamera camera;camera.zoom=1;camera.orbit=sign*.4886922f;
  mcd::Vector eye{-5,0,0},center{},up{0,0,1},forward{1,0,0};
  require(camera.transform(eye,center,up,forward),"close rear-quarter transform accepted");
  auto v=mcd::add(center,mcd::mul(eye,-1));v=mcd::mul(v,1/mcd::length(v));
  require(mcd::dot(v,up)<0&&mcd::dot(v,up)>-.06f,"close camera retains shallow road pitch");
 }

 for(float sign:{-1.f,1.f}){
  mcd::DriftCamera camera;camera.zoom=1;camera.orbit=sign*.3f;
  mcd::Vector eye{-5,0,0},center{},up{0,0,1},forward{1,0,0};
  require(camera.transform(eye,center,up,forward),"low camera transforms on either turn");
  require(eye.z>-.57f&&eye.z<=-.55f,"low viewpoint sits below native target");
  require(center.z<-.67f&&center.z>-.69f,"road target below eye creates downward view");
  camera.zoom=0;camera.orbit=.0001f;eye={-5,0,0};center={};up={0,0,1};camera.transform(eye,center,up,forward);
  require(std::abs(eye.z)<.00001f,"height returns continuously near drift end");
 }

 for(float sign:{-1.f,1.f}){
  mcd::DriftCamera camera;camera.zoom=1;camera.orbit=sign*.3f;
  mcd::Vector eye{-5,0,0},center{},up{0,0,1},forward{1,0,0};
  require(camera.transform(eye,center,up,forward),"half screen composition enabled");
  auto view=mcd::add(center,mcd::mul(eye,-1));view=mcd::mul(view,1/mcd::length(view));
  auto right=mcd::cross(view,up);right=mcd::mul(right,1/mcd::length(right));
  auto toCar=mcd::mul(eye,-1);
  float screenSide=mcd::dot(toCar,right)/mcd::dot(toCar,view);
  require(screenSide*sign<-.17f&&screenSide*sign>-.20f,"car moves opposite turn leaving road side open");
  require(mcd::length(eye)>=3.75f&&mcd::length(eye)<=3.80f,"close composition keeps camera outside minimum radius");
 }

 // A relative tail marker tests vertical movement, not unknown vehicle mesh/FOV.
 for(float sign:{-1.f,1.f}){
  mcd::DriftCamera camera;camera.zoom=1;camera.orbit=sign*.3f;
  mcd::Vector eye{-5,0,0},center{},up{0,0,1};camera.transform(eye,center,up,{1,0,0});
  auto vertical=[](mcd::Vector e,mcd::Vector c){
   auto v=mcd::add(c,mcd::mul(e,-1));v=mcd::mul(v,1/mcd::length(v));
   auto r=mcd::cross(v,{0,0,1});r=mcd::mul(r,1/mcd::length(r));auto u=mcd::cross(r,v);
   auto tail=mcd::add(mcd::Vector{-2,0,-.6f},mcd::mul(e,-1));
   return mcd::dot(tail,u)/mcd::dot(tail,v);
  };
  auto oldEye=eye,oldCenter=center;oldEye.z=.06f;oldCenter.z=-.16f;
  require(vertical(eye,center)>vertical(oldEye,oldCenter)+.15f,"lower view lifts relative tail marker away from bottom edge");
 }

 // Independent proximity envelope is bounded and does not dip on a sign flip.
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  mcd::DriftCamera c;
  for(int i=0;i<int(4/dt);++i){float z=c.zoom,v=c.zoomVelocity;c.step(dt,true,.3f);
   require(std::abs(c.zoom-z)<=.5f*dt+.00001f,"zoom speed bounded");
   require(c.zoom==0||c.zoom==1||std::abs(c.zoomVelocity-v)<=.75f*dt+.00001f,"zoom acceleration bounded except endpoint stop");
  }
  require(c.zoom>.99f,"sustained drift reaches close framing");
  for(int i=0;i<int(2/dt);++i){c.step(dt,true,-.3f);require(c.zoom>.99f,"reversal cannot pump zoom out and in");}
  for(int i=0;i<int(6/dt);++i){float z=c.zoom;c.step(dt,false,0);require(std::abs(c.zoom-z)<=.5f*dt+.00001f,"zoom release remains gradual");}
  require(std::abs(c.zoom)<.0001f,"zoom fully returns after release");
 }

 // Repeated interrupted entries previously drove zoom below zero after release.
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  mcd::DriftCamera c;
  for(int i=0;i<int(20/dt);++i){
   bool active=std::fmod(i*dt,3.f)<1.2f;c.step(dt,active,.3f);
   require(c.zoom>=0&&c.zoom<=1,"interrupted zoom never accumulates out-of-range debt");
   require(c.zoom>0||c.zoomVelocity>=0,"fully released zoom has no outward velocity");
  }
  // Observed status at 98591750 ms, alpha30 session.
  c.zoom=-.01221f;c.zoomVelocity=-.02357f;c.step(dt,false,0);
  require(c.zoom==0&&c.zoomVelocity==0,"recorded negative zoom state clears at endpoint");
  c.step(dt,true,.3f);require(c.zoom>0,"new entry starts without recovering invisible negative zoom");
 }

 // Check the complete camera pose across entry, reversal and release, not just yaw.
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  mcd::DriftCamera camera; mcd::Vector previousEye{-5,0,0},previousCenter{};
  for(int i=0;i<int(9/dt);++i){
   float t=i*dt;camera.step(dt,t<6,t<3?.4f:-.4f);
   mcd::Vector eye{-5,0,0},center{},up{0,0,1};camera.transform(eye,center,up,{1,0,0});
   require(mcd::length(mcd::add(eye,mcd::mul(previousEye,-1)))<20*dt,"close camera position stays continuous through reversal");
   require(mcd::length(mcd::add(center,mcd::mul(previousCenter,-1)))<20*dt,"road aim stays continuous through reversal");
   previousEye=eye;previousCenter=center;
  }
 }

 // Actual alpha22 camera sample at 93369093 ms. Mirror it to cover both turns.
 for(float sign:{-1.f,1.f}){
  mcd::DriftCamera camera;camera.zoom=1;camera.orbit=sign*.2538f;
  mcd::Vector eye{-4.759f,sign*-.548f,-.193f},center{},up{0,0,1},forward{.957f,sign*-.291f,.010f};
  auto angle=[&](mcd::Vector e){auto view=mcd::mul(e,-1);return std::atan2(mcd::dot(up,mcd::cross(view,forward)),mcd::dot(view,forward));};
  float before=angle(eye);
  require(camera.transform(eye,center,up,forward),"recorded drifting camera transforms");
  float after=angle(eye);
  require(before*after>0&&std::abs(after)>std::abs(before)+.20f,"both drift directions reveal side instead of snapping behind body");
  require(std::abs(after)<.8f,"recorded diagonal rear view remains rear-facing");
 }

 {
  mcd::DriftCamera camera;camera.zoom=1;camera.orbit=.1f;camera.roll=.01f;
  mcd::Vector eye{-4.826f,-1.925f,-.340f},center{},up{0,0,1};
  require(camera.transform(eye,center,up,mcd::cameraForward({-.465f,.075f,.882f})),"recorded Widescreen render geometry accepts corrected physics basis");
  center={};eye={2.683f,-4.813f,-.365f};up={0,0,1};
  require(camera.transform(eye,center,up,mcd::cameraForward({-.862f,.059f,-.504f})),"second recorded heading accepts corrected basis");
 }

 for(bool reversed:{false,true}){
  mcd::PedalMapping p;auto v=sample(.01f);v.gas=1;v.brake=0;
  for(int i=0;i<30;++i)p.step(v,true,reversed?1.f:0.f,reversed?0.f:1.f);
  v.gas=0;v.brake=1;
  for(int i=0;i<30;++i)p.step(v,true,reversed?0.f:1.f,reversed?1.f:0.f);
  require(p.gasAxis==(reversed?0:1)&&p.brakeAxis==(reversed?1:0),"learn standard and swapped mapped pedals");
  v.gas=0;v.brake=0;p.step(v,true,.8f,.7f);
  require(std::abs(v.gas-(reversed?.8f:.7f))<.001f&&std::abs(v.brake-(reversed?.7f:.8f))<.001f,"recover cancelled combined axis using learned mapping");
  p.step(v,false,0,0);require(p.gasAxis==-1&&p.brakeAxis==-1,"disconnect clears learned mapping");
 }
 {
  mcd::PedalMapping p;auto v=sample(.01f);v.gas=0;v.brake=0;
  p.step(v,true,1,1);require(v.gas==0&&v.brake==0,"unknown mapping cannot invent pedal actions");
  v.gas=1;for(int i=0;i<30;++i)p.step(v,true,0,1);
  v.gas=0;v.brake=1;for(int i=0;i<30;++i)p.step(v,true,1,0);
  v.gas=1;v.brake=0;p.step(v,true,1,0);
  require(p.gasAxis==-1&&p.brakeAxis==-1,"contradictory remapping resets before supplement");
 }

 for(float dt:{1.f/30,1.f/60,1.f/120})for(float dir:{-1.f,1.f}){
  auto s=sample(dt);s.steer=dir*.8f;s.beta=-dir*.02f;
  mcd::Controller c;c.sign=1;c.step(s);s.handbrake=1;
  unsigned entries=0,adds=0;
  for(int i=0;i<int(.1f/dt)+1;i++){auto o=c.step(s);entries+=o.handbrakeEvent==1;adds+=o.handbrakeEvent==2;require(o.deltaSpeed==0,"handbrake braking never refunded");}
  require(entries==1&&adds==0&&c.phase==mcd::Phase::Drift,"handbrake starts one drift without foot brake");
  s.handbrake=0;s.beta=dir*.18f;
  float maxPower=0;
  for(int i=0;i<int(.3f/dt);i++){c.step(s);maxPower=std::max(maxPower,c.handbrakePower);}
  require(maxPower>.8f&&c.phase==mcd::Phase::Drift,"short tap leaves a bounded continuing pulse");
  // Compare the additional target with an otherwise identical normal drift.
  mcd::Controller normal=c;normal.handbrakePower=normal.handbrakeWindow=0;
  float deep=0,shallow=0;
  for(int i=0;i<int(.25f/dt);i++){deep=c.step(s).targetSlip*dir;shallow=normal.step(s).targetSlip*dir;}
  require(deep>shallow+.015f,"handbrake increases slip target over normal drift");
  s.handbrake=1;adds=0;
  for(int i=0;i<int(2.f/dt);i++){auto o=c.step(s);adds+=o.handbrakeEvent==2;require(c.handbrakePower<=1.0001f&&std::abs(o.targetSlip)<=.5001f,"held handbrake cannot accumulate unlimited angle");}
  require(adds==1&&c.handbrakePower==0,"held press adds once then fades");
  s.handbrake=0;c.step(s);s.handbrake=1;require(c.step(s).handbrakeEvent==2,"fresh press adds during power drift");
  s.steer=-dir*.2f;c.step(s);require(c.handbrakePower==0&&c.handbrakeWindow==0,"counter clears added angle request");
  s.steer=dir*.8f;require(c.step(s).handbrakeEvent==0&&c.handbrakePower==0,"held handbrake does not resurrect pulse after counter");
  s.steer=0;for(int i=0;i<int(.3f/dt);i++)require(!c.step(s).apply,"neutral cancels handbrake drift assistance");
  s.steer=dir*.8f;require(!c.step(s).apply,"same held handbrake cannot reenter after neutral exit");
  s.handbrake=0;c.step(s);s.handbrake=1;c.step(s);s.safe=false;
  require(!c.step(s).apply&&c.handbrakePower==0,"air or collision clears handbrake pulse");
  c={};c.sign=1;s=sample(dt);s.steer=0;c.step(s);s.handbrake=1;
  for(int i=0;i<10;i++)require(!c.step(s).apply,"straight handbrake cannot choose a drift direction");
 }
 {
  auto s=sample(1.f/120);mcd::Controller c;trigger(c,s);s.brake=0;s.beta=.18f;
  unsigned events=0;
  for(int tap=0;tap<20;tap++){
   s.handbrake=0;c.step(s);s.handbrake=1;
   for(int i=0;i<3;i++){auto o=c.step(s);events+=o.handbrakeEvent==2;require(c.handbrakePower<=1&&o.targetSlip<=.5001f,"rapid taps remain bounded");}
  }
  require(events==20,"rapid taps produce one addition per press");
  s.speedBeforeStep=60;s.speed=59;c.step(s);s.handbrake=0;s.speed=58.8f;
  require(c.step(s).deltaSpeed==0&&c.entrySpeed==s.speed,"last handbrake deceleration is not repaid at release");
 }
 {
  mcd::MotionHistory h;auto s=sample(.01f);s.handbrake=1;
  h.commit(s,1,100,0,0,60,true);
  h.sample(s,1,110,.1f,0,59.9f);
  require(s.nativeTurnValid&&s.speedBeforeStep==0,"handbrake preserves path observation but blocks speed refund");
  h.commit(s,1,110,.1f,0,59.9f,true);s.handbrake=0;
  h.sample(s,1,120,.2f,0,59.8f);
  require(s.nativeTurnValid&&s.speedBeforeStep==0,"release does not refund handbrake loss");
 }
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  mcd::FrontSteering f;float native[2]={.06f,.05f};
  f.step(dt,native,0);require(!f.active&&f.angle[0]==native[0],"normal steering remains native");
  for(int i=0;i<int(1.f/dt);i++)f.step(dt,native,1);
  require(f.angle[0]<-.055f,"drift reaches countersteer display");
  float previous=f.angle[0];native[0]=native[1]=0;
  f.step(dt,native,1);
  require(f.angle[0]<0&&std::abs(f.angle[0]-previous)<=.30f*dt+.00001f,"instant native centering cannot snap visible front wheel");
  for(int i=0;i<int(2.f/dt);i++){
   previous=f.angle[0];f.step(dt,native,0);
   require(std::abs(f.angle[0]-previous)<=.30f*dt+.0001f,"return angle remains rate bounded");
  }
  require(!f.active&&f.angle[0]==0,"front display returns fully to native");
 }
 // Tire-rate discontinuity and counter release must not instantly restore
 // path assistance. Test the actually delivered trajectory increment.
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  auto s=sample(dt);mcd::Controller c;trigger(c,s);s.brake=0;s.beta=.15f;
  s.nativeTurnValid=true;s.nativeTurn=.9f;
  for(int i=0;i<int(1/dt);i++)c.step(s);
  s.nativeTurn=0;
  float prev=0;
  for(int i=0;i<int(.4f/dt);i++){
   auto o=c.step(s);float rate=o.deltaDirection/dt;
   require(rate-prev<=.9f*dt+.0001f,"native path drop cannot abruptly start assist");prev=rate;
  }
  s.steer=-.3f;c.step(s);require(c.pathAssist==0,"counter clears old path assistance");
  s.steer=.6f;prev=0;
  for(int i=0;i<int(.3f/dt);i++){
   auto o=c.step(s);float rate=o.deltaDirection/dt;
   require(rate-prev<=.9f*dt+.0001f,"counter release ramps path assistance");prev=rate;
  }
  s.steer=0;auto o=c.step(s);
  require(!o.apply&&c.pathAssist==0,"neutral cancels trajectory immediately");
 }
 {
  require(mcd::presentationRPMCaller(0x69419d)&&mcd::presentationRPMCaller(0x6f1536),"verified SoundRacer and HUD paths receive visual RPM");
  require(mcd::driveLayout(0)==mcd::DriveLayout::RWD&&mcd::driveLayout(1)==mcd::DriveLayout::FWD,"native axle endpoints");
  for(float split:{.001f,.25f,.5f,.99f})require(mcd::driveLayout(split)==mcd::DriveLayout::AWD,"mixed torque drives all wheels");
  for(float split:{-1.f,1.01f,std::numeric_limits<float>::quiet_NaN()})require(mcd::driveLayout(split)==mcd::DriveLayout::Unknown,"invalid split fails closed");
  for(auto layout:{mcd::DriveLayout::FWD,mcd::DriveLayout::RWD,mcd::DriveLayout::AWD,mcd::DriveLayout::Unknown}){
   mcd::Presentation e;
   for(int i=0;i<120;i++)e.step(1.f/120,true,.2f,1,0,layout);
   require(e.steer(.2f)<-.19f,"counter steering retained for every layout");
   for(unsigned wheel=0;wheel<4;wheel++){
    const bool spins=layout==mcd::DriveLayout::AWD||(layout==mcd::DriveLayout::RWD&&wheel>=2);
    require(spins?e.wheel(100,wheel)>100:e.wheel(100,wheel)==100,"axle spin selection");
   }
   if(layout==mcd::DriveLayout::FWD||layout==mcd::DriveLayout::Unknown)require(e.spin==0&&e.rpm(5000,8000)==5000,"FWD and unknown preserve native RPM");
   e.step(1.f/120,true,.2f,1,0,mcd::DriveLayout::FWD);
   require(e.spin==0&&e.wheel(100,2)==100,"vehicle change clears old rear spin");
  }
  mcd::Presentation p;
  for(int i=0;i<120;i++)p.step(1.f/120,true,.2f,1,0,mcd::DriveLayout::RWD);
  require(std::abs(p.steer(.2f)+.2f)<.0001f,"front display reverses at settled drift");
  require(p.wheel(100,0)==100&&p.wheel(100,1)==100,"front wheel rotation preserved");
  require(p.wheel(100,2)>100&&p.wheel(-100,3)<-100,"rear spin preserves native rotation sign");
  require(p.rpm(5000,8000)>5000&&p.rpm(7900,8000)<=8000,"visual RPM rises with bounded headroom");
  require(p.rpm(8500,8000)==8500,"native overrev never reduced");
  require(!mcd::presentationRPMCaller(0x6a0dbe)&&!mcd::presentationRPMCaller(0x67032e)&&!mcd::presentationRPMCaller(0),"physics and unknown RPM callers stay native");
  for(int i=0;i<120;i++)p.step(1.f/120,false,0,0,0,mcd::DriveLayout::RWD);
  require(p.steer(.2f)==.2f&&p.wheel(100,2)==100&&p.rpm(5000,8000)==5000,"presentation returns to native after exit");
 }
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  auto s=sample(dt);mcd::Controller c;c.sign=1;
  for(int i=0;i<100;i++)require(!c.step(s).apply,"grip has no writes");
  trigger(c,s);require(c.phase==mcd::Phase::Drift,"begins before brake release");
  s.gas=0;
  for(int i=0;i<int(2/dt);i++){s.speed-=5*dt;auto o=c.step(s);require(c.phase==mcd::Phase::Drift&&o.deltaSpeed==0,"held brake maintains drift without speed refund");}
  require(c.entrySpeed<=s.speed+.001f,"reference follows deliberate deceleration");
  s.brake=0;s.beta=.15f;
  for(int i=0;i<int(2/dt);i++)c.step(s);
  require(c.phase==mcd::Phase::Drift,"coasting remains active");
  s.steer=0;s.yaw=.3f;
  for(int i=0;i<int(1/dt);i++){auto o=c.step(s);require(!o.apply&&o.deltaYaw==0&&o.deltaDirection==0&&o.deltaSpeed==0,"neutral with slip has no motion writes");}
  require(c.phase==mcd::Phase::Grip,"neutral intent cancels despite remaining slip");
  s.steer=.2f;for(int i=0;i<int(1/dt);i++)require(!c.step(s).apply,"steering after cancel cannot resurrect old drift");
  s.steer=0;s.beta=.02f;s.yaw=.02f;
  for(int i=0;i<int(.4f/dt);i++)require(!c.step(s).apply,"aligned neutral never forced to turn");
  require(c.phase==mcd::Phase::Grip,"aligned posture exits");
  // Same held brake must not rearm after alignment.
  s=sample(dt);c={};trigger(c,s);s.steer=0;s.beta=s.yaw=0;
  for(int i=0;i<int(.4f/dt);i++)c.step(s);
  s.steer=.6f;require(!c.step(s).apply,"held brake cannot repeatedly rearm");
  s.brake=0;c.step(s);s.brake=1;for(int i=0;i<5;i++)c.step(s);require(c.phase==mcd::Phase::Drift,"new press rearms");
  // Braking straight, then turning while brake remains down.
  s=sample(dt);c={};c.sign=1;s.steer=0;c.step(s);s.brake=1;
  for(int i=0;i<int(.5f/dt);i++)require(!c.step(s).apply,"straight braking stays grip");
  s.steer=.6f;for(int i=0;i<5;i++)c.step(s);require(c.phase==mcd::Phase::Drift,"steer during held brake starts drift");
  s.brake=0;s.gas=1;s.beta=.2f;s.steer=-1;
  for(int i=0;i<int(.3f/dt);i++){auto o=c.step(s);require(o.deltaDirection==0&&c.direction==1,"counter does not force trajectory before alignment");}
  s.beta=.04f;for(int i=0;i<int(1/dt);i++)c.step(s);require(c.direction==-1,"aligned transfer works");
  s=sample(dt);c={};trigger(c,s);s.brake=0;s.beta=.18f;s.yaw=-.5f;s.steer=-.2f;s.speedBeforeStep=60.1f;
  for(int i=0;i<int(2/dt);i++){auto o=c.step(s);require(c.direction==1&&o.deltaSpeed==0&&o.deltaYaw*s.yaw<=0,"light counter does not flip, accelerate or amplify swing");}
  s.safe=false;require(!c.step(s).apply,"air and collision stop immediately");
  s=sample(dt);trigger(c,s);s.beta=std::numeric_limits<float>::quiet_NaN();require(!c.step(s).apply,"NaN rejected");
 }
 // Continuous cornering loss: feed-forward restores the measured loss;
 // speed never exceeds the entry/reference and held braking is never repaid.
 for(float dt:{1.f/30,1.f/60,1.f/120})for(float decel:{5.f,10.f,15.f}){
  auto s=sample(dt);mcd::Controller c;trigger(c,s);s.brake=0;
  for(int i=0;i<int(10/dt);i++){
   s.speedBeforeStep=s.speed;s.speed-=decel*dt;s.beta=.18f;
   auto o=c.step(s);
   require(o.deltaSpeed>=0&&o.deltaSpeed<=18*dt+.00001f,"refund has bounded per-step rate");
   s.speed+=o.deltaSpeed;require(s.speed<=60.0001f,"no speed beyond reference");
  }
  require(s.speed>59.7f,"continuous slide loss stays below 0.3mps after settling");
  s.speedBeforeStep=s.speed;s.speed-=2;c.step(s);float reduced=c.entrySpeed;
  require(reduced==s.speed,"impact-like loss lowers reference");
  s.speedBeforeStep=s.speed;auto noRestore=c.step(s);require(noRestore.deltaSpeed==0,"impact not repaid next frame");
  s.brake=1;s.speedBeforeStep=s.speed;s.speed-=1;
  require(c.step(s).deltaSpeed==0&&c.entrySpeed==s.speed,"intentional brake loss not refunded");
  s.brake=0;s.speedBeforeStep=s.speed;s.speed+=.5f;
  auto accel=c.step(s);require(accel.deltaSpeed==0&&c.entrySpeed==s.speed,"native acceleration retained without boost");
  s.safe=false;require(!c.step(s).apply,"unsafe state blocks refund");
 }
 // Final output smoothing, including the entry envelope; unlike the old
 // internal-only limit this bounds actual delivered yaw acceleration.
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  auto s=sample(dt);mcd::Controller c;c.sign=1;c.step(s);s.brake=1;
  float prev=0;
  for(int i=0;i<int(2/dt);i++){
   s.beta=-.2f;s.yaw=-.4f;s.nativeTurnValid=true;s.nativeTurn=.4f;
   auto o=c.step(s);float a=o.deltaYaw/dt;
   require(std::abs(a-prev)<=40*dt+.0001f,"actual entry output obeys slew limit");prev=a;
  }
  s.brake=0;s.steer=-.2f;s.yaw=-.4f;
  for(int i=0;i<int(.5f/dt);i++)c.step(s);
  s.steer=.6f;prev=c.deliveredAccel;
  for(int i=0;i<int(.3f/dt);i++){
   auto o=c.step(s);float a=o.deltaYaw/dt;
   require(std::abs(a-prev)<=40*dt+.0001f,"counter release has no stored acceleration kick");prev=a;
  }
  s.steer=0;require(!c.step(s).apply&&c.deliveredAccel==0,"neutral clears delivered acceleration immediately");
 }
 // Slope geometry preserves normal velocity and total speed, both hill signs.
 for(float grade:{-.5f,-.25f,0.f,.25f,.5f}){
  mcd::Vector n{0,std::cos(grade),-std::sin(grade)},f{0,std::sin(grade),std::cos(grade)};
  auto v=mcd::add(mcd::mul(f,60),mcd::mul(n,.4f));
  for(float a:{-.2f,-.01f,0.f,.01f,.2f}){
   auto out=mcd::rotateSurface(v,n,a,0);
   require(std::abs(mcd::dot(out,n)-.4f)<.00001f,"slope normal velocity preserved");
   require(std::abs(mcd::length(out)-mcd::length(v))<.00002f,"slope rotation preserves energy");
   require(std::abs(mcd::surfaceBeta(f,out,n)+a)<.00001f,"slope beta consistent with turn sign");
   auto boosted=mcd::rotateSurface(v,n,a,.1f);
   require(std::abs(mcd::length(mcd::tangent(boosted,n))-60.1f)<.00002f,"boost acts in tangent plane only");
  }
  mcd::MotionHistory h;auto s=sample(.01f);h.commit(s,1,100,v.x,v.y,v.z,true,.4f);
  auto cur=mcd::rotateSurface(v,n,.002f,0);
  h.sample(s,1,110,cur.x,cur.y,cur.z,.4f,n.x,n.y,n.z);
  require(s.nativeTurnValid&&std::abs(s.nativeTurn-.2f)<.0001f,"history measures slope turn");
  require(std::abs(s.speedBeforeStep-60)<.00002f,"history speed independent of slope");
 }
 for(float dt:{.005f,.008333f,.016667f,.033333f}){
  mcd::DriftCamera camera;
  for(int i=0;i<int(3/dt);++i){float previous=camera.orbit;camera.step(dt,true,.8f);require(std::abs(camera.orbit)<=.488693f&&std::abs(camera.orbit-previous)<=.45f*dt+.000001f,"camera angle and rate bounded");}
  for(int i=0;i<int(2/dt);++i){float v=camera.orbitVelocity;camera.step(dt,true,-.6f);require(std::abs(camera.orbitVelocity-v)<=.8f*dt+.00001f,"camera reversal acceleration bounded");}
  float beforeExit=camera.orbit;camera.step(dt,false,0);
  require(std::abs(camera.orbit-beforeExit)<=.45f*dt+.00001f,"contact or drift exit cannot snap camera to zero");
  mcd::Vector eye{0,2,-8},center{0,1,0},up{0,1,0},forward{0,0,1};
  const auto originalCenter=center;float distance=mcd::length(mcd::add(eye,mcd::mul(center,-1)));
  require(camera.transform(eye,center,up,forward),"chase camera transformed");
  require(mcd::length(mcd::add(eye,mcd::mul(originalCenter,-1)))>=3.7499f&&mcd::length(mcd::add(eye,mcd::mul(originalCenter,-1)))<distance,"camera moves closer with minimum distance");
  require(std::abs(mcd::length(up)-1)<.00001f,"camera up remains normalized");
  center=originalCenter;eye={0,1,8};require(!camera.transform(eye,center,up,forward),"lookback not changed");
  center=originalCenter;eye={0,1,-1};require(!camera.transform(eye,center,up,forward),"bumper not changed");
  for(int i=0;i<int(4/dt);++i)camera.step(dt,false,0);
  require(std::abs(camera.orbit)<.00001f&&std::abs(camera.roll)<.00001f,"camera returns to native smoothly");
 }
 printf("PASS %d checks; in-game slope and recovery acceptance pending\n",checks);
}
