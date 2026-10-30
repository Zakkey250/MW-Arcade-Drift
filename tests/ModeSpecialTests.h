#pragma once
#include "../src/ModeController.h"
#include "../src/ModeHost.h"
#include "../src/SpecialProfile.h"
#include "../integration/EventLoaderModeClient.h"
#include <thread>
inline void modeSpecialTests(){
 using namespace mcd;
 EventLoaderSpecialModeLease unusedClient;unusedClient.End();require(unusedClient.Update()==EventLoaderSpecialModeLease::Status::Lost,"optional consumer is inert before Begin");
 for(auto original:{DrivingMode::Normal,DrivingMode::Arcade,DrivingMode::Special}){
  ModeSelection mode;mode.manual=original;
  require(mode.effective(1)==original,"manual initial mode");
  auto token=mode.acquire(41,1000,1000);require(token!=0&&mode.effective(1000)==DrivingMode::Special,"event forces special from each manual mode");
  require(!mode.cycle(1100)&&mode.manual==original,"forced key preserves pre-event choice");
  require(!mode.acquire(42,1100,1000),"overlapping owner refused");
  require(!mode.release(42,token,1100)&&!mode.release(41,token+1,1100),"wrong owner and stale release refused");
  require(!mode.renew(41,token,1100,0)&&mode.renew(41,token,1100,1000),"lease range enforced and valid lease renewed");
  require(mode.release(41,token,1200)&&mode.effective(1200)==original,"release restores exact previous mode");
  auto next=mode.acquire(41,1300,250);require(next!=token&&!mode.release(41,token,1400),"old token cannot end a later event");
  require(mode.effective(1550)==original&&!mode.renew(41,next,1550,250),"expiry restores mode and refuses late heartbeat");
 }
 {
  ModeSelection mode;mode.manual=DrivingMode::Normal;
  for(auto expected:{DrivingMode::Arcade,DrivingMode::Special,DrivingMode::Normal}){require(mode.cycle(1)&&mode.effective(1)==expected,"Normal Arcade Special Normal cycle");}
  require(!mode.acquire(0,1,500)&&!mode.acquire(1,1,249)&&!mode.acquire(1,1,10001),"invalid API lease requests rejected");
  ModeHost host;require(!host.acquire(1,100,500),"API unavailable until hooks ready");host.initialize(DrivingMode::Normal);host.activate();
  auto token=host.acquire(1,100,500);require(token!=0,"ready API accepts lease");
  std::thread worker([&](){for(unsigned i=0;i<1000;i++){host.renew(1,token,110,500);host.state(110);}});
  for(unsigned i=0;i<1000;i++)host.state(110);worker.join();
  host.publish(110,true,DrivingMode::Arcade);require(!host.state(120).vehicleReady,"API does not acknowledge special using an old arcade sample");
  host.publish(110,true,DrivingMode::Special);auto state=host.state(120);
  require(state.ready&&state.vehicleReady&&state.manualMode==0&&state.effectiveMode==2&&state.forced,"thread-safe ABI snapshot");
  require(!host.state(300).vehicleReady,"stale vehicle readiness expires");
  require(host.release(1,token,300)&&host.state(300).effectiveMode==0,"host restores normal mode");
  require(!host.state(300).vehicleReady,"restored mode also waits for a matching physics sample");
 }
 for(float dt:{1.f/30,1.f/60,1.f/120}){
  Controller released;ModeController dispatch;released.sign=dispatch.sign=1;
  auto s=sample(dt);
  for(unsigned i=0;i<4000;i++){
   s.steer=std::sin(float(i)*.017f);s.brake=i%240<8?1.f:0;s.handbrake=i%1100<4?1.f:0;s.beta=.15f*std::sin(float(i)*.03f);s.yaw=.4f*std::sin(float(i)*.017f);s.speedBeforeStep=s.speed+.01f;s.safe=i%600!=599;
   auto a=released.step(s),b=dispatch.step(s);
   require(a.apply==b.apply&&a.deltaYaw==b.deltaYaw&&a.deltaDirection==b.deltaDirection&&a.deltaSpeed==b.deltaSpeed&&a.targetSlip==b.targetSlip&&a.targetYaw==b.targetYaw&&released.phase==dispatch.phase,"Arcade dispatch matches release bit for bit");
  }
 }
 for(float dt:{1.f/30,1.f/60,1.f/120})for(float direction:{-1.f,1.f}){
  ModeController c;c.sign=1;c.select(DrivingMode::Special);auto s=sample(dt);s.steer=direction*.75f;s.brake=0;s.nativeTurnValid=true;s.nativeTurn=direction*.4f;
  bool applied=false;float deepest=0;
  for(unsigned i=0;i<120;i++){
   auto o=c.step(s);applied|=o.apply;deepest=std::max(deepest,std::abs(o.targetSlip));
   require(o.deltaYaw==0&&o.deltaDirection==0&&o.deltaSpeed==0,"native Special never writes yaw, travel direction or speed");
  }
  require(applied&&c.phase==Phase::Drift&&deepest>.3f,"special presentation state can arm without a foot brake; this does not prove physical slip");
  s.steer=-direction*.75f;s.beta=direction*.3f;for(unsigned i=0;i<60;i++)c.step(s);
  require(c.special.direction==direction,"transfer waits for previous slide to unwind");
  s.beta=0;for(unsigned i=0;i<60;i++)c.step(s);
  require(c.special.direction==-direction,"sustained counter transfers after alignment");
  s.steer=0;for(unsigned i=0;i<60;i++)require(!c.step(s).apply,"neutral immediately inhibits corrections");
  require(c.phase==Phase::Grip,"neutral eventually ends special slide");
  s.steer=direction*.75f;c.step(s);s.safe=false;
  require(!c.step(s).apply&&c.phase==Phase::Grip,"contact safety cancels special");
  s.safe=true;s.gas=1;for(unsigned i=0;i<10;i++)c.step(s);c.select(DrivingMode::Normal);
  require(!c.step(s).apply&&c.phase==Phase::Grip,"normal clears active special state");
  c.select(DrivingMode::Arcade);require(!c.step(s).apply,"return to arcade requires its original brake entry");
  c.select(DrivingMode::Special);s.speed=10;for(unsigned i=0;i<60;i++)require(!c.step(s).apply,"below entry speed cannot begin special");
  s.speed=60;s.dt=.06f;require(!c.step(s).apply,"long timestep blocks special");
  s.dt=dt;s.speed=std::numeric_limits<float>::quiet_NaN();require(!c.step(s).apply,"nonfinite sample blocks special");
 }
 {
  namespace fs=std::filesystem;auto root=fs::temp_directory_path()/("MWArcadeDrift-special-test-"+std::to_string(GetCurrentProcessId()));
  require(!fs::exists(root),"special fixture unique");fs::create_directories(root);
  const fs::path defaults="MW-Arcade-Drift-public/config/modes/special";
  for(auto file:{"default_drift.json","default_camera.json"})fs::copy_file(defaults/file,root/file);
  auto profile=loadSpecialVehicleProfile(root,"SVJ",true);require(profile.enabled&&profile.drift.baseSlipRad==.2f,"special independent defaults loaded");
  const SpecialTuning defaultsNative{};
  const auto nativeJson=readProfileJson(defaults/"default_drift.json");
  const auto fields=readProfileJson("MW-Arcade-Drift-public/editor/fields.json");
  for(const auto& setting:std::initializer_list<std::pair<const char*,float>>{
   {"frontGripPercent",defaultsNative.frontGripPercent},{"rearGripPercent",defaultsNative.rearGripPercent},
   {"rearStaticGripPercent",defaultsNative.rearStaticGripPercent},{"rearLateralGripPercent",defaultsNative.rearLateralGripPercent},
   {"speedLossPercent",defaultsNative.speedLossPercent}}){
   require(nativeJson.at(setting.first).get<float>()==setting.second,"native source and new-profile defaults agree");
   bool found=false;for(const auto& field:fields)if(field.at("key")==setting.first&&field.at("section")=="special"){
    found=true;require(field.at("value").get<float>()==setting.second,"native GUI seed matches runtime default");}
   require(found,"native handling control is present in the GUI");
  }
  auto d=readProfileJson(root/"vehicles/SVJ/drift.json"),camera=readProfileJson(root/"vehicles/SVJ/camera.json");
  for(unsigned test=0;test<5;test++){
   auto bad=d;if(test==0)bad["schemaVersion"]=2;else if(test==1)bad["unexpected"]=true;else if(test==2)bad["baseSlipRad"]=.4;else if(test==3)bad["exitSpeedMps"]=20;else bad["yawAccelLimit"]="4.5";
   bool rejected=false;try{parseSpecialProfile(bad,camera);}catch(...){rejected=true;}require(rejected,"special invalid schema/key/range/cross-field/type rejected");
  }
  auto before=readProfileJson(root/"vehicles/SVJ/drift.json");loadSpecialVehicleProfile(root,"SVJ",true);require(readProfileJson(root/"vehicles/SVJ/drift.json")==before,"special existing profile never overwritten");
  fs::remove_all(root);
 }
}
