#include <windows.h>
#include "Registry.h"
#include "DriftEvidence.h"
#include "NativeRaceTuning.h"
#include "../integration/EventLoaderDriftAIClient.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
static unsigned checks=0;
static void Check(bool value,const char* name){++checks;if(!value){printf("FAIL %s check=%u\n",name,checks);std::exit(1);}}
#define CHECK(x) Check(bool(x),#x)
int main(){
 {
  CHECK(!mcdai::usesNativeRaceTuning(2,mcdai::FreeRoam));
  CHECK(mcdai::usesNativeRaceTuning(2,mcdai::Race)&&mcdai::usesNativeRaceTuning(2,mcdai::LocalAutodriveRace));
  CHECK(!mcdai::usesNativeRaceTuning(1,mcdai::Race)&&!mcdai::usesNativeRaceTuning(2,99));
  // Real driven-axle classification must not leak human per-car settings
  // into an opponent or apply AWD tuning to unknown/invalid layouts.
  const auto rear=mcdai::nativeRaceTuning(0),all=mcdai::nativeRaceTuning(.5f),front=mcdai::nativeRaceTuning(1);
  CHECK(rear.rearLateralGripPercent==12&&all.rearLateralGripPercent==16&&front.rearLateralGripPercent==16);
  CHECK(rear.rearStaticGripPercent==80&&all.rearStaticGripPercent==100&&front.rearStaticGripPercent==80);
  CHECK(rear.rearGripPercent==40&&all.rearGripPercent==30&&front.rearGripPercent==40);
  for(float invalid:{-1.f,2.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
   const auto unknown=mcdai::nativeRaceTuning(invalid);
   CHECK(unknown.frontGripPercent==front.frontGripPercent&&unknown.rearGripPercent==front.rearGripPercent&&
    unknown.rearStaticGripPercent==front.rearStaticGripPercent&&unknown.rearLateralGripPercent==20);
  }
 }
 {
  using namespace mcdai;Registry handling;Entry* car=nullptr;BindingV1 b;
  b.owner=1;b.generation=1;b.vehicle=0x20000;b.simHandle=1;b.mode=2;b.scope=Race;b.race=0x40000;
  CHECK(handling.acquire(b,0x30000,100,car)==Ok);
  RefreshV1 r;r.owner=b.owner;r.token=car->token;r.flags=AllowCorrection;
  CHECK(handling.refresh(r,110)==Ok&&car->nativeHandlingFresh(110));
  r.flags=0;CHECK(handling.refresh(r,120)==Ok&&!car->fresh(120)&&car->nativeHandlingFresh(120));
  CHECK(!car->nativeHandlingFresh(119)&&!car->nativeHandlingFresh(271));
  car->binding.mode=1;CHECK(!car->nativeHandlingFresh(120));car->binding.mode=2;
  car->binding.scope=FreeRoam;CHECK(!car->nativeHandlingFresh(120));car->binding.scope=Race;
  auto token=car->token;CHECK(handling.release(1,token)==Ok&&!car->nativeHandlingFresh(120));
 }
 {
  mcdai::DriftEvidence measured;mcd::Sample s;s.safe=true;s.dt=1.f/60;s.speed=25;s.gas=1;s.beta=.6f;
  for(unsigned i=0;i<302;++i)measured.Step(1,s,{float(i)*.4f,0,0},true);
  CHECK(measured.seconds>5&&measured.metres>120);
  CHECK(std::abs(measured.end.x-120.4f)<.001f);
  CHECK(!measured.Step(1,s,{121,0,0},false));CHECK(std::abs(measured.end.x-120.4f)<.001f);measured.End();CHECK(measured.seconds==0);
  s.safe=false;CHECK(!measured.Step(1,s,{121.4f,0,0},true));CHECK(measured.seconds==0);
  s.safe=true;s.beta=.2f;CHECK(!measured.Step(1,s,{121.8f,0,0},true));
  s.beta=.6f;CHECK(!measured.Step(1,s,{500,0,0},true));CHECK(std::abs(measured.end.x-120.4f)<.001f);
  CHECK(!measured.Step(2,s,{500.4f,0,0},true)&&measured.seconds==0);
  measured.Step(2,s,{500.8f,0,0},true);
  for(unsigned i=0;i<4;++i)CHECK(measured.Step(2,s,{500.8f,0,0},true));
  for(unsigned i=0;i<10;++i)measured.Step(2,s,{500.8f,0,0},true);
  CHECK(!measured.Step(2,s,{500.8f,0,0},true)&&measured.rejection==6);
 }
 using namespace mcdai;Registry r;BindingV1 a;a.vehicle=0x20000;a.simHandle=1;a.mode=1;a.owner=1;a.generation=1;a.scope=FreeRoam;
 Entry* e=nullptr;CHECK(r.acquire(a,0x30000,100,e)==Ok);CHECK(e&&e->token==1&&!e->allow);auto token=e->token;
 BindingV1 duplicate=a;duplicate.owner=2;CHECK(r.acquire(duplicate,0x30000,101,e)==Busy);CHECK(!e);
 RefreshV1 f;f.owner=2;f.token=token;f.flags=AllowCorrection;CHECK(r.refresh(f,110)==NotOwned);
 f.owner=1;CHECK(validRefresh(f,110));auto invalid=f;invalid.flags=4;CHECK(!validRefresh(invalid,110));invalid=f;invalid.size=0;CHECK(!validRefresh(invalid,110));CHECK(!validRefresh(f,UINT64_MAX));CHECK(r.refresh(f,110)==Ok);e=r.find(1,token);CHECK(e&&e->fresh(260)&&!e->fresh(261));
 e->controller.sign=1;e->controller.phase=mcd::Phase::Drift;e->state.phase=2;e->motion.valid=true;f.flags=0;CHECK(r.refresh(f,120)==Ok);CHECK(!e->allow&&!e->motion.valid&&e->controller.phase==mcd::Phase::Grip&&e->state.phase==0);CHECK(e->view(121).reason==Paused);
 f.flags=AllowCorrection;CHECK(r.refresh(f,125)==Ok);e->controller.phase=mcd::Phase::Drift;CHECK(r.refresh(f,300)==Ok);CHECK(e->controller.phase==mcd::Phase::Grip);
 e->state.sampleTick=300;e->state.eligible=1;e->state.reason=Applied;e->state.deltaYaw=.01f;CHECK(e->view(301).eligible==1);CHECK(e->view(451).reason==StaleCommand&&e->view(451).eligible==0&&e->view(451).deltaYaw==0);CHECK(e->view(1).eligible==0);
 CHECK(r.release(2,token)==NotOwned);CHECK(r.release(1,token)==Ok);CHECK(!r.find(1,token));
 CHECK(r.acquire(a,0x30000,400,e)==Ok);CHECK(e->token!=token);token=e->token;r.expire(900);CHECK(!r.find(1,token));
 for(unsigned i=0;i<Capacity;i++){auto b=a;b.vehicle+=i*0x1000;b.simHandle=i+1;b.owner=i+1;b.mode=i%2+1;CHECK(r.acquire(b,0x50000+i*0x1000,1000,e)==Ok);}
 auto over=a;over.vehicle=0x90000;CHECK(r.acquire(over,0x99000,1000,e)==Busy);
 auto* first=r.find(1,3);CHECK(first);first->controller.sign=1;first->controller.phase=mcd::Phase::Drift;
 for(auto& entry:r.entries)if(entry.binding.owner!=1)CHECK(entry.controller.sign==0&&entry.controller.phase==mcd::Phase::Grip);
 r.expire(1500);for(auto& entry:r.entries)CHECK(!entry.used);
 for(unsigned field=0;field<12;field++){auto b=a;switch(field){case 0:b.size=0;break;case 1:b.version=2;break;case 2:b.owner=0;break;case 3:b.generation=0;break;case 4:b.vehicle=1;break;case 5:b.simHandle=0;break;case 6:b.mode=0;break;case 7:b.mode=3;break;case 8:b.leaseMs=249;break;case 9:b.leaseMs=2001;break;case 10:b.scope=Race;break;case 11:b.reserved[1]=1;break;}CHECK(r.acquire(b,0x30000,2000,e)==BadRequest);}
 CHECK(r.acquire(a,0x30000,UINT64_MAX-10,e)==BadRequest);
 // Both AI families dispatch the frozen controller equations without changing
 // any player state. Different vehicles keep independent history and polarity.
 for(unsigned mode=1;mode<=2;mode++){
  mcd::ModeController candidate;candidate.select(mcd::DrivingMode(mode));candidate.sign=candidate.special.sign=1;
  mcd::Controller arcade;mcd::SpecialController special;arcade.sign=special.sign=1;
  for(unsigned i=0;i<12000;i++){
   mcd::Sample s;s.safe=true;s.dt=1.f/120;s.speed=18+float(i%100)/10;s.steer=(i%240<120?.35f:-.35f);s.gas=.8f;s.brake=i%500<30?.4f:0;s.handbrake=i%600<20?.7f:0;s.beta=.05f*std::sin(float(i)*.01f);s.yaw=s.steer*.4f;s.nativeTurnValid=true;s.nativeTurn=s.yaw;
   auto x=candidate.step(s),y=mode==1?arcade.step(s):special.step(s);
   CHECK(x.apply==y.apply&&x.deltaYaw==y.deltaYaw&&x.deltaDirection==y.deltaDirection&&x.deltaSpeed==y.deltaSpeed);
   if(mode==2)CHECK(x.deltaSpeed==0);
  }
 }
 std::array<EventLoaderClient,16> parallelClients{};
 CHECK(parallelClients[0].Frame(0x20000,1,1,0x30000,1,false,false)==Disabled);
 EventLoaderClient client(1);StateV1 absent;
 CHECK(client.Frame(0x20000,1,0,0x30000,1,true,true)==Disabled);
 CHECK(client.Frame(0x20000,1,2,0x30000,1,true,true)==NotReady);
 CHECK(client.Frame(0x20000,1,2,0x30000,2,false,true)==Disabled);
 CHECK(client.Frame(0x20000,1,3,0x30000,1,true,true)==BadRequest);
 CHECK(client.Read(absent)==NotOwned);client.Reset();
 // Planner intent has the same ownership/freshness boundary as correction.
 Registry planned;Entry* plannedEntry=nullptr;CHECK(planned.acquire(a,0x30000,100,plannedEntry)==Ok);
 IntentV1 intent;intent.owner=a.owner;intent.token=plannedEntry->token;intent.active=1;intent.targetSlip=.3f;intent.pathYaw=.5f;
 CHECK(planned.setIntent(intent,100)==NotReady);
 RefreshV1 allow;allow.owner=a.owner;allow.token=plannedEntry->token;allow.flags=AllowCorrection;
 CHECK(planned.refresh(allow,101)==Ok);CHECK(planned.setIntent(intent,102)==Ok);
 PathAssistV1 path;path.owner=a.owner;path.token=plannedEntry->token;path.strength=.6f;
 CHECK(planned.setPathAssist(path,103)==Ok&&plannedEntry->pathStrength==.6f);
 for(float bad:{0.f,.24f,1.01f,std::numeric_limits<float>::quiet_NaN()}){auto invalidPath=path;invalidPath.strength=bad;CHECK(planned.setPathAssist(invalidPath,103)==BadRequest);}
 {auto foreign=path;foreign.owner=999;CHECK(planned.setPathAssist(foreign,103)==NotOwned);}
 CHECK(planned.refresh(allow,200)==Ok);
 CHECK(plannedEntry->intentFresh(252)&&!plannedEntry->intentFresh(253));
 CHECK(planned.setPathAssist(path,253)==NotReady);
 auto invalidIntent=intent;invalidIntent.targetSlip=std::numeric_limits<float>::quiet_NaN();CHECK(planned.setIntent(invalidIntent,110)==BadRequest);
 invalidIntent=intent;invalidIntent.owner=999;CHECK(planned.setIntent(invalidIntent,210)==NotOwned);
 allow.flags=0;CHECK(planned.refresh(allow,211)==Ok);CHECK(!plannedEntry->intentFresh(211));
 CHECK(plannedEntry->pathStrength==1&&plannedEntry->pathWeight==1);
 // After neutral planning, the first actual HB edge must arm Arcade. Wheel
 // countersteer must not reverse the separately requested slide direction.
 for(unsigned mode=1;mode<=2;++mode){
  Entry x;x.used=x.allow=true;x.commandTick=100;x.intentTick=100;
  x.controller.select(mcd::DrivingMode(mode));x.controller.sign=x.controller.special.sign=1;
  mcd::Sample input;input.safe=true;input.dt=1.f/60;input.speed=25;input.gas=1;
  CHECK(!x.step(input,101).apply);
  x.intent.active=1;x.intent.targetSlip=.3f;x.intent.pathYaw=.5f;
  input.steer=-.4f;input.handbrake=.9f;
  for(unsigned t=102;t<110;++t)x.step(input,t);
  CHECK(x.controller.phase==mcd::Phase::Drift&&x.controller.direction<0);
  x.intent.active=0;CHECK(!x.step(input,111).apply&&x.controller.phase==mcd::Phase::Grip);
   // Recovery pause may be followed immediately by the next real HB pulse.
   // No neutral physics tick is guaranteed between those two API commands.
   x.reset();x.intentTick=x.commandTick=200;x.allow=true;x.intent.active=1;x.intent.targetSlip=.65f;x.intent.pathYaw=.5f;
   for(unsigned t=201;t<212;++t)x.step(input,t);
   CHECK(x.controller.phase==mcd::Phase::Drift);
 }
 // Different map requests must not change the physical output for the same
 // actual input. No route steering is injected into a neutral native sample.
 for(unsigned mode=1;mode<=2;++mode){
  Entry left,right;for(auto* car:{&left,&right}){
   car->used=car->allow=true;car->binding.mode=mode;car->controller.select(mcd::DrivingMode(mode));
   car->controller.sign=car->controller.special.sign=1;car->commandTick=car->intentTick=100;car->intent.active=1;
  }
  left.intent.targetSlip=.65f;left.intent.pathYaw=1.4f;right.intent.targetSlip=-.65f;right.intent.pathYaw=-1.4f;
  left.pathStrength=.25f;right.pathStrength=1;
  mcd::Sample input;input.safe=true;input.dt=1.f/60;input.speed=25;input.gas=1;input.nativeTurnValid=true;input.nativeTurn=.3f;
  CHECK(!left.step(input,101).apply&&!right.step(input,101).apply);
  bool applied=false;
  for(unsigned i=0;i<240;++i){input.steer=.6f;input.handbrake=i<8?.7f:0;input.beta=.2f;
   auto lo=left.step(input,102),hi=right.step(input,102);applied|=lo.apply;
   CHECK(lo.apply==hi.apply&&lo.deltaYaw==hi.deltaYaw&&lo.deltaSpeed==hi.deltaSpeed);
   CHECK(lo.deltaDirection==0&&hi.deltaDirection==0);
  }
  CHECK(applied);
  input.steer=0;CHECK(!left.step(input,103).apply);
  input.safe=false;CHECK(!right.step(input,103).apply&&right.controller.phase==mcd::Phase::Grip);
 }
 Polarity polarity;mcd::Sample measured;measured.safe=true;measured.dt=1.f/120;measured.speed=20;measured.steer=.1f;measured.yaw=.2f;
 float sign=0;for(unsigned i=0;i<61;++i)sign=polarity.Observe(measured);CHECK(sign==1);
 Polarity opposite;measured.yaw=-.2f;for(unsigned i=0;i<61;++i)sign=opposite.Observe(measured);CHECK(sign==-1);
 auto human=a;human.scope=LocalAutodriveRace;human.race=0x40000;CHECK(validBinding(human));human.race=0;CHECK(!validBinding(human));human.race=0x40000;human.scope=3;CHECK(!validBinding(human));
 puts("PASS fixed ABI, lease ownership/expiry, duplicate/capacity, stale/reset, per-car isolation, optional NeoAI client, and 24000 frozen-controller parity samples");
 printf("PASS %u AI bridge checks\n",checks);return 0;
}
