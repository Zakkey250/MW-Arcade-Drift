#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <share.h>
#include <fstream>
#include "Registry.h"
#include "DriftEvidence.h"
#include "../src/DriftModeAPI.h"
#include "../src/ProfileFiles.h"
#include "../src/SpecialProfile.h"
#include "../src/NativeHandlingAPI.h"
#include "NativeRaceTuning.h"
#include "MinHook.h"

namespace {
using U=std::uint32_t;using Vec=mcd::Vector;
// RacecarPhysics common simulation dispatch covers 00699CC0 and 006BA510.
// Body mode alone does not select the suspension behavior. Never force flags.
constexpr U PhysicsStep=0x6bb4d0;
HMODULE module=nullptr;wchar_t ini[MAX_PATH]{},logfile[MAX_PATH]{};
bool logging=false,enabled=true;
std::atomic<bool> ready{false},fault{false},leased{false};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
SRWLOCK lock=SRWLOCK_INIT;mcdai::Registry registry;
mcdai::DriftEvidence evidence[mcdai::Capacity];
mcdai::DriftEvidence lateralEvidence[mcdai::Capacity];float lateralSpinSeconds[mcdai::Capacity]{};
using Integrate=void(__thiscall*)(void*,float);Integrate original=nullptr;
SRWLOCK logLock=SRWLOCK_INIT;FILE* logStream=nullptr;ULONGLONG logFlush=0;
void Log(const char* format,...){
 if(!logging)return;AcquireSRWLockExclusive(&logLock);
 if(!logStream){logStream=_wfsopen(logfile,L"a",_SH_DENYNO);if(logStream)setvbuf(logStream,nullptr,_IOFBF,65536);}
 if(logStream){const auto now=GetTickCount64();fprintf(logStream,"[%llu] ",now);va_list a;va_start(a,format);vfprintf(logStream,format,a);va_end(a);fputc('\n',logStream);
  if(now-logFlush>=200||strncmp(format,"AI_SAMPLE",9)&&strncmp(format,"AI_YAW_RESPONSE",15)){fflush(logStream);logFlush=now;}}
 ReleaseSRWLockExclusive(&logLock);
}
#define Log(...) do{if(logging)(Log)(__VA_ARGS__);}while(false)
bool Read(U address,void* value,size_t bytes){SIZE_T got=0;return address>=0x10000&&address<=UINT32_MAX-bytes&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),value,bytes,&got)&&got==bytes;}
template<class T>bool Get(U address,T& value){return Read(address,&value,sizeof(value));}
bool Method(U obj,U slot,U expected){U vt=0,f=0;return Get(obj,vt)&&vt>=0x890000&&vt<0x8f0000&&Get(vt+slot*4,f)&&f==expected;}
bool Finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool Match(U address,const char* expected,size_t bytes){char data[32]{};return bytes<=sizeof(data)&&Read(address,data,bytes)&&memcmp(data,expected,bytes)==0;}
float NativeFrontShare(U vehicle){
 // Same verified ISuspension/mTransInfo consumer as the existing score and
 // presentation readers. Decline foreign layouts; never infer from car names.
 // Binding.vehicle is the 8AA828 race interface, also stored in 92CD1C's pool;
 // its ISuspension pointer is +44. It is not the native owner base at -80.
 U suspension=0,type=0,trans=0;float split=std::numeric_limits<float>::quiet_NaN();
 if(vehicle<0x10080||!Match(0x68de00,"\x8b\x81\xdc\x00\x00\x00\xd9\x40\x70",9)||
    !Get(vehicle+0x44,suspension)||!Get(suspension,type)||
    (type!=0x8abac8&&(type!=0x8abc30||!Match(0x6b2694,"\xc7\x46\x4c\x30\xbc\x8a\x00",7)))||
    !Get(suspension+0x90,trans)||!trans||!Get(trans+0x70,split)||!std::isfinite(split)||split<0||split>1)
  return std::numeric_limits<float>::quiet_NaN();
 return split;
}
std::string Digest(HMODULE target){
 wchar_t file[MAX_PATH]{};if(!GetModuleFileNameW(target,file,MAX_PATH))return {};
 std::ifstream f(file,std::ios::binary);if(!f)return {};
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;bool ok=BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)>=0;
 char buffer[32768];while(ok&&f){f.read(buffer,sizeof(buffer));auto n=f.gcount();if(n>0)ok=BCryptHashData(h,reinterpret_cast<BYTE*>(buffer),static_cast<ULONG>(n),0)>=0;}
 BYTE data[32]{};if(ok)ok=BCryptFinishHash(h,data,32,0)>=0;if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);
 if(!ok)return {};char text[65]{};for(U i=0;i<32;i++)sprintf_s(text+i*2,65-i*2,"%02x",data[i]);return text;
}
bool Running(){U flow=0,time=0,state=0;unsigned char overlay=0;int a=0,b=0;
 // An owned AI keeps simulating when the native race keeps simulating in the
 // background. The manual player's foreground guard does not apply to AI.
 // Local human automation still requires its explicit no-save owner contract.
 return Get(0x925e90,flow)&&flow==6&&Get(0x9885e0,time)&&time&&Get(time+0x2c,state)&&state!=4&&Get(0x9b0fb9,overlay)&&!overlay&&Get(0x9b41fc,a)&&a<=0&&Get(0x9b4220,b)&&b<=0;
}
bool Member(U vehicle){U list=0,count=0;if(!Get(0x92cd1c,list)||!Get(0x92cd24,count)||!list||count>256)return false;for(U i=0;i<count;i++){U p=0;if(Get(list+i*4,p)&&p==vehicle)return true;}return false;}
// Explicit no-save local automation only. Native human drivers never opt in implicitly.
bool LocalHumanOwner(const mcdai::BindingV1& b){
 if(b.scope!=mcdai::LocalAutodriveRace||b.owner!=0x4d57454c44524654ull)return false;
 auto events=GetModuleHandleW(L"MWEventLoader.asi"),player=GetModuleHandleW(L"MWArcadeDrift.asi");
 using Test=U(__cdecl*)();using Owned=U(__cdecl*)(void*);
 auto test=events?reinterpret_cast<Test>(GetProcAddress(events,"MWEL_LocalRaceTestV1")):nullptr;
 auto owned=events?reinterpret_cast<Owned>(GetProcAddress(events,"MWEL_NeoPowerOwnedV1")):nullptr;
 U ai=0;if(!test||!test()||!owned||!Get(b.vehicle+0x54,ai)||!owned(reinterpret_cast<void*>(ai)))return false;
 auto mode=player?reinterpret_cast<MWArcadeDriftGetModeV1Fn>(GetProcAddress(player,"MWArcadeDriftGetModeV1")):nullptr;
 MWArcadeDriftModeV1 st{};st.size=sizeof(st);st.version=1;
 // Refuse a second controller on a human body already corrected by the player module.
 return mode&&mode(&st)&&st.ready&&st.effectiveMode==0&&!st.forced;
}
bool RaceScope(const mcdai::BindingV1& b){
 if(b.scope==mcdai::FreeRoam)return true;
 if(b.scope==mcdai::LocalAutodriveRace&&!LocalHumanOwner(b))return false;
 U race=0,count=0,play=0,context=0;unsigned char running=0,go=0;
 if(!Get(0x91e000,race)||race!=b.race||!Get(race+0x1958,count)||count>16||!Get(race+0x1960,play)||play!=1||!Get(race+0x1964,context)||(context!=0&&context!=2)||!Get(race+0x198a,running)||!running||!Get(race+0x1978,go)||!go)return false;
 for(U i=0;i<count;i++){U handle=0,dead=0;unsigned char finished=0;if(!Get(race+0x18+i*0x194,handle)||handle!=b.simHandle)continue;
  return Get(race+0x18+i*0x194+0x1c,dead)&&!dead&&Get(race+0x18+i*0x194+0x21,finished)&&!finished;}
 return false;
}
mcdai::Result Identity(const mcdai::BindingV1& b,U& body,bool list){
 U vt=0,driver=0,simvt=0,handle=0,mode=0;unsigned char animation=0;
 if(b.vehicle<0x10000||!Get(b.vehicle,vt)||vt!=0x8aa828||!Get(b.vehicle+0x94,driver)||(driver!=3&&!(driver==0&&LocalHumanOwner(b))))return mcdai::NotRacer;
 if(!Get(b.vehicle-0x80,simvt)||simvt!=0x8aa940||!Get(b.vehicle-0x78,handle)||handle!=b.simHandle||
    !Get(b.vehicle-0x34,body)||body<0x10048||!Get(b.vehicle+0xac,mode)||mode!=1||
    !Get(b.vehicle+0xb0,animation)||animation||(list&&!Member(b.vehicle))||!RaceScope(b))return mcdai::StaleVehicle;
 U owner=0;if(!Get(body-0x14,owner)||owner!=b.vehicle-0x80)return mcdai::StaleVehicle;return mcdai::Ok;
}
struct Snapshot {Vec velocity{},angular{},forward{},up{},position{},roadNormal{};mcd::Sample sample;U wheels=0;unsigned char flags=0;bool roadRead=false,steeringRead=false;float steeringLock=0,frontSteer[2]{};};
bool ReadSteering(U vehicle,float& maximum,float* front){
 U suspension=0,vt=0;if(!Get(vehicle+0x44,suspension)||!Get(suspension,vt)||(vt!=0x8abac8&&vt!=0x8abc30))return false;
 const U angle=vt==0x8abc30?0x6ac3e0:0x6a96d0,lockMethod=vt==0x8abc30?0x6ac410:0x6a9700;
 if(!Method(suspension,22,angle)||!Method(suspension,27,lockMethod))return false;
 const auto object=reinterpret_cast<void*>(suspension);
 // Native methods return turns: degrees/360 and radians/(2*pi), respectively.
 maximum=reinterpret_cast<float(__thiscall*)(void*)>(lockMethod)(object)*2*mcd::pi;
 for(U wheel=0;wheel<2;++wheel)front[wheel]=reinterpret_cast<float(__thiscall*)(void*,U)>(angle)(object,wheel)*2*mcd::pi;
 return std::isfinite(maximum)&&maximum>.1f&&maximum<1.5f&&std::isfinite(front[0])&&std::isfinite(front[1]);
}
bool ReadRoadNormal(U vehicle,Vec& normal){
 U suspension=0,vt=0;if(!Get(vehicle+0x44,suspension)||!Get(suspension,vt)||(vt!=0x8abac8&&vt!=0x8abc30))return false;
 const U normalMethod=vt==0x8abc30?0x6ac340:0x6a9630,contactMethod=vt==0x8abc30?0x6ac360:0x6a9650;
 if(!Method(suspension,16,normalMethod)||!Method(suspension,9,contactMethod))return false;
 using Contact=bool(__thiscall*)(void*,U);using Normal=const Vec&(__thiscall*)(void*,U);
 const auto object=reinterpret_cast<void*>(suspension);Vec sum{};U count=0;
 for(U wheel=0;wheel<4;++wheel)if(reinterpret_cast<Contact>(contactMethod)(object,wheel)){
  const Vec n=reinterpret_cast<Normal>(normalMethod)(object,wheel);const float length=mcd::length(n);
  if(!Finite(n)||length<.9f||length>1.1f||n.y<.5f)return false;
  sum=mcd::add(sum,mcd::mul(n,1.f/length));++count;
 }
 const float length=mcd::length(sum);if(count<3||length<2.5f)return false;
 normal=mcd::mul(sum,1.f/length);return true;
}
bool Capture(U body,float dt,Snapshot& s){
 if(!Method(body,9,0x671190)||!Method(body,10,0x6711a0)||!Method(body,13,0x671270)||!Method(body,24,0x696fd0)||!Method(body,25,0x696ff0))return false;
 U owner=0,ref=0,data=0,input=0,vt=0,getter=0,wheels=0,collision=0,cr=0,cd=0;unsigned char flags=0;
 if(!Get(body-0x14,owner)||!Get(body+0x30,ref)||!Get(ref,data)||!Get(data+0x10,s.position)||!Get(data+0x20,s.velocity)||!Get(data+0x30,s.angular)||!Get(data+0x90,s.forward)||!Get(data+0x80,s.up)||
    !Get(owner+0x104,wheels)||!Get(owner+0xc0,collision)||!Method(collision,17,0x670fb0)||!Get(collision+0x28,cr)||!Get(cr,cd)||!Get(cd+0x1c,flags)||
    !Get(owner+0xbc,input)||!Get(input,vt)||(vt!=0x8ab5c8&&vt!=0x8ac670)||!Get(vt+8,getter)||getter<0x401000||getter>=0x890000)return false;
 unsigned char code[7]{};U offset=0;if(!Read(getter,code,7)||code[0]!=0x8d)return false;
 if(code[1]==0x41&&code[3]==0xc3&&code[2]<0x80)offset=code[2];else if(code[1]==0x81&&code[6]==0xc3){memcpy(&offset,code+2,4);if(offset>0x200)return false;}else return false;
 float controls[8]{};if(!Read(input+offset,controls,sizeof(controls)))return false;for(auto v:controls)if(!std::isfinite(v)||std::abs(v)>1.01f)return false;
 if(!Finite(s.velocity)||!Finite(s.angular)||!Finite(s.forward)||!Finite(s.up)||!Finite(s.position))return false;
 float upLength=mcd::length(s.up),fn=mcd::dot(s.forward,s.forward);if(upLength<.95f||upLength>1.05f||fn<.5f||fn>1.1f)return false;
 s.up=mcd::mul(s.up,1.f/upLength);auto& v=s.sample;v.dt=dt;v.speed=mcd::length(mcd::tangent(s.velocity,s.up));v.steer=controls[1];v.gas=controls[5];v.brake=controls[6];v.handbrake=controls[7];v.yaw=mcd::dot(s.angular,s.up);v.beta=mcd::surfaceBeta(s.forward,s.velocity,s.up);
 s.wheels=wheels;s.flags=flags;v.safe=wheels>=3&&wheels<=4&&s.up.y>.85f&&(flags&12)==0;return true;
}
bool WheelEvidence(U vehicle,float* slip,float* rim,float& ratio,U& vt,Vec forward,Vec up,Vec& wheelVelocity,float& rearAngle){
 U suspension=0;if(!Get(vehicle+0x44,suspension)||!Get(suspension,vt)||vt!=0x8abac8)return false;
 if(!Method(suspension,12,0x6a94f0)||!Method(suspension,20,0x68e570)||!Method(suspension,26,0x6a94d0)||
    !Method(suspension,18,0x6a96a0)||!Method(suspension,9,0x6a9650))return false;
 using Wheel=float(__thiscall*)(void*,U);using Velocity=const Vec&(__thiscall*)(void*,U);using Contact=bool(__thiscall*)(void*,U);
 auto object=reinterpret_cast<void*>(suspension);ratio=0;bool contact=false;rearAngle=mcd::pi;
 for(U w=0;w<2;++w){
  slip[w]=reinterpret_cast<Wheel>(0x6a94f0)(object,w+2);
  rim[w]=reinterpret_cast<Wheel>(0x68e570)(object,w+2)*reinterpret_cast<Wheel>(0x6a94d0)(object,w+2);
  const auto velocity=reinterpret_cast<Velocity>(0x6a96a0)(object,w+2);
  if(w==0)wheelVelocity=velocity;
  const float speed=std::abs(mcd::dot(velocity,forward));
  if(!Finite(velocity)||!std::isfinite(rim[w]))return false;
  if(speed>=1&&reinterpret_cast<Contact>(0x6a9650)(object,w+2)){
   ratio=std::max(ratio,std::abs(rim[w])/speed);contact=true;
   rearAngle=std::min(rearAngle,std::atan2(std::abs(mcd::dot(velocity,mcd::cross(up,forward))),speed));
  }
 }return contact;
}
bool RacerBody(U primary){if(primary<0x10000)return false;__try{U owner=*reinterpret_cast<U*>(primary+0x34);return owner>=0x10000&&*reinterpret_cast<U*>(owner+0x80)==0x8aa828&&(*reinterpret_cast<U*>(owner+0x114)==3||*reinterpret_cast<U*>(owner+0x114)==0);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void ForgetEvidence(const mcdai::Entry& e){if(logging){
 const auto i=size_t(&e-&registry.entries[0]);const auto& power=evidence[i];const auto& lateral=lateralEvidence[i];
 // Finalize at the LAST accepted physics position before a pause/release.
 // Previously these exact continuous intervals disappeared on a safety pause.
 if(power.token==e.token&&power.seconds>=.5f)Log("AI_POWER_SLIDE handle=%u token=%llu simulation=%.6f seconds=%.6f metres=%.3f start=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f threshold_degrees=30 rear_power_ratio_min=1.05 end_reason=9",e.binding.simHandle,e.token,power.simulation,power.seconds,power.metres,power.start.x,power.start.y,power.start.z,power.end.x,power.end.y,power.end.z);
 if(lateral.token==e.token&&lateral.seconds>=.5f)Log("AI_LATERAL_POWER_SLIDE handle=%u token=%llu simulation=%.6f seconds=%.6f metres=%.3f longitudinal_spin_seconds=%.6f start=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f body_threshold_degrees=30 rear_threshold_degrees=8 minimum_throttle=0.5 end_reason=9",e.binding.simHandle,e.token,lateral.simulation,lateral.seconds,lateral.metres,lateralSpinSeconds[i],lateral.start.x,lateral.start.y,lateral.start.z,lateral.end.x,lateral.end.y,lateral.end.z);
 evidence[i]={};lateralEvidence[i]={};lateralSpinSeconds[i]=0;
}}
void FlushExpiredEvidence(std::uint64_t now){if(logging)for(const auto& e:registry.entries)if(e.used&&now>=e.deadline)ForgetEvidence(e);}
void Process(U primary,float dt){
 auto now=GetTickCount64();FlushExpiredEvidence(now);registry.expire(now);bool any=false;
 for(auto& e:registry.entries){
  if(!e.used)continue;any=true;if(e.body!=primary+0x48)continue;
  auto& st=e.state;st.eligible=0;st.deltaYaw=st.deltaDirection=st.deltaSpeed=0;st.sampleTick=now;
  if(!Running()){ForgetEvidence(e);e.reset();st.reason=mcdai::Paused;continue;}
  U body=0;if(Identity(e.binding,body,false)!=mcdai::Ok||body!=e.body){Log("AI_RELEASE handle=%u reason=identity_changed",e.binding.simHandle);ForgetEvidence(e);e={};continue;}
  if(!e.fresh(now)){ForgetEvidence(e);e.reset();st.reason=e.allow?mcdai::StaleCommand:mcdai::Paused;continue;}
  Snapshot s;if(!Capture(e.body,dt,s)){ForgetEvidence(e);e.reset();st.reason=mcdai::Unsafe;continue;}
  if(logging){s.roadRead=ReadRoadNormal(e.binding.vehicle,s.roadNormal);s.steeringRead=ReadSteering(e.binding.vehicle,s.steeringLock,s.frontSteer);}
  auto& sample=s.sample;float normal=mcd::dot(s.velocity,s.up);
  e.motion.sample(sample,e.body,now,s.velocity.x,s.velocity.y,s.velocity.z,normal,s.up.x,s.up.y,s.up.z);
  if(!e.controller.sign){auto sign=e.polarity.Observe(sample);if(sign){e.controller.sign=sign;e.controller.special.sign=sign;Log("AI_POLARITY handle=%u sign=%.0f measured_input_yaw=1 player_controller_unchanged=1",e.binding.simHandle,sign);}}
  // Only the actual pedals/steering enter the shared controller.
  if(logging)e.yawResponse.observe(sample);
  auto out=e.step(sample,now);
  // Keep native restoring acceleration; no cancellation by the helper.
  Vec velocity=s.velocity;
  st.eligible=sample.safe?1u:0u;st.reason=sample.safe?mcdai::Grip:mcdai::Unsafe;
  if(out.apply){
   if(out.deltaDirection!=0||out.deltaSpeed!=0)velocity=mcd::rotateSurface(s.velocity,s.up,out.deltaDirection,out.deltaSpeed);
   Vec angular=mcd::add(s.angular,mcd::mul(s.up,out.deltaYaw));
   if(!Finite(velocity)||!Finite(angular)){e.reset();st.reason=mcdai::Fault;continue;}
   // Only this integrator's own verified Racer body. No input/AI/handling writes.
   if(out.deltaDirection!=0||out.deltaSpeed!=0)reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(0x696fd0)(reinterpret_cast<void*>(e.body),&velocity);
   if(out.deltaYaw!=0)reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(0x696ff0)(reinterpret_cast<void*>(e.body),&angular);
   ++st.applied;st.reason=mcdai::Applied;st.deltaYaw=out.deltaYaw;st.deltaDirection=out.deltaDirection;st.deltaSpeed=out.deltaSpeed;
  }
  e.motion.commit(sample,e.body,now,velocity.x,velocity.y,velocity.z,e.controller.phase==mcd::Phase::Drift,mcd::dot(velocity,s.up));
  if(logging)e.yawResponse.commit(sample,out.deltaYaw,out.apply&&e.intentFresh(now));
  ++st.steps;st.phase=U(e.controller.phase);st.calibrated=e.controller.sign!=0;st.speed=sample.speed;st.beta=sample.beta;st.yaw=sample.yaw;st.steer=sample.steer;st.gas=sample.gas;st.brake=sample.brake;st.handbrake=sample.handbrake;st.position[0]=s.position.x;st.position[1]=s.position.y;st.position[2]=s.position.z;
  float rearSlip[2]{},rearRim[2]{},spinRatio=0,rearAngle=0;U evidenceVT=0;bool wheelRead=false;Vec rearVelocity{};
  if(logging){
   auto& measured=evidence[size_t(&e-&registry.entries[0])];
   wheelRead=WheelEvidence(e.binding.vehicle,rearSlip,rearRim,spinRatio,evidenceVT,s.forward,s.up,rearVelocity,rearAngle);
   if(!measured.Step(e.token,sample,s.position,wheelRead&&spinRatio>1.05f)){
    if(measured.seconds>=.5f)Log("AI_POWER_SLIDE handle=%u token=%llu simulation=%.6f seconds=%.6f metres=%.3f start=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f threshold_degrees=30 rear_power_ratio_min=1.05 end_reason=%u",st.simHandle,e.token,measured.simulation,measured.seconds,measured.metres,measured.start.x,measured.start.y,measured.start.z,measured.end.x,measured.end.y,measured.end.z,measured.rejection);
    measured.End();
   }
   const auto index=size_t(&e-&registry.entries[0]);auto& lateral=lateralEvidence[index];
   if(lateral.token!=e.token)lateralSpinSeconds[index]=0;
   if(lateral.Step(e.token,sample,s.position,wheelRead&&rearAngle>=8*mcd::pi/180,.5f)){
    if(spinRatio>1.05f)lateralSpinSeconds[index]+=sample.dt;
   }else{
    if(lateral.seconds>=.5f)Log("AI_LATERAL_POWER_SLIDE handle=%u token=%llu simulation=%.6f seconds=%.6f metres=%.3f longitudinal_spin_seconds=%.6f start=%.3f,%.3f,%.3f end=%.3f,%.3f,%.3f body_threshold_degrees=30 rear_threshold_degrees=8 minimum_throttle=0.5 end_reason=%u",st.simHandle,e.token,lateral.simulation,lateral.seconds,lateral.metres,lateralSpinSeconds[index],lateral.start.x,lateral.start.y,lateral.start.z,lateral.end.x,lateral.end.y,lateral.end.z,lateral.rejection);
    lateral.End();lateralSpinSeconds[index]=0;
   }
  }
  if(logging){static std::uint64_t next[16]{};auto i=static_cast<size_t>(&e-&registry.entries[0]);if(now>=next[i]){next[i]=now+100;
   Log("AI_YAW_RESPONSE handle=%u native_acceleration=%.5f filtered=%.5f committed_yaw=%.5f",st.simHandle,e.yawResponse.nativeAcceleration,e.yawResponse.filteredResistance,e.yawResponse.committed);
   Log("AI_ROAD_PLANE handle=%u valid=%u normal=%.5f,%.5f,%.5f body_normal_speed=%.5f road_normal_speed=%.5f agreement=%.5f",st.simHandle,unsigned(s.roadRead),s.roadNormal.x,s.roadNormal.y,s.roadNormal.z,mcd::dot(s.velocity,s.up),mcd::dot(s.velocity,s.roadNormal),mcd::dot(s.up,s.roadNormal));
   Log("AI_STEERING_SUSPENSION handle=%u valid=%u maximum_radians=%.6f front_radians=%.6f,%.6f input=%.4f beta=%.5f",st.simHandle,unsigned(s.steeringRead),s.steeringLock,s.frontSteer[0],s.frontSteer[1],sample.steer,sample.beta);
   U physics=0;unsigned char branch=255;
   if(Get(primary+0x84,physics))Get(physics+0xff,branch);
   Log("AI_SAMPLE handle=%u mode=%u phase=%u eligible=%u reason=%u calibrated=%u speed=%.4f beta=%.5f yaw=%.5f steer=%.3f gas=%.3f brake=%.3f handbrake=%.3f dyaw=%.6f ddir=%.6f dspeed=%.6f steps=%u applied=%u position=%.3f,%.3f,%.3f target_beta=%.5f target_turn=%.5f native_turn=%.5f turn_valid=%u dt=%.6f physics_branch=%u wheel_read=%u rear_slip=%.5f,%.5f rear_rim_speed=%.5f,%.5f susp_vt=%08X simulation=%.6f power_seconds=%.6f spin_ratio=%.5f rear_velocity=%.4f,%.4f,%.4f forward=%.4f,%.4f,%.4f rear_angle=%.5f lateral_power_seconds=%.6f wheels=%u collision_flags=%u up_y=%.5f path_weight=%.4f path_target=%.4f",st.simHandle,st.mode,st.phase,st.eligible,st.reason,st.calibrated,st.speed,st.beta,st.yaw,st.steer,st.gas,st.brake,st.handbrake,st.deltaYaw,st.deltaDirection,st.deltaSpeed,st.steps,st.applied,st.position[0],st.position[1],st.position[2],out.targetSlip,out.targetYaw,sample.nativeTurn,unsigned(sample.nativeTurnValid),dt,unsigned(branch),unsigned(wheelRead),rearSlip[0],rearSlip[1],rearRim[0],rearRim[1],evidenceVT,evidence[i].simulation,evidence[i].seconds,spinRatio,rearVelocity.x,rearVelocity.y,rearVelocity.z,s.forward.x,s.forward.y,s.forward.z,rearAngle,lateralEvidence[i].seconds,s.wheels,unsigned(s.flags),s.up.y,e.pathWeight,e.pathStrength);}}
 }
 leased.store(any);
}
void __fastcall Hook(void* self,void*,float dt){
 original(self,dt);
 if(!leased.load()||fault.load()||!RacerBody(reinterpret_cast<U>(self))||busy.test_and_set())return;
 if(TryAcquireSRWLockExclusive(&lock)){
  __try{Process(reinterpret_cast<U>(self),dt);}__except(EXCEPTION_EXECUTE_HANDLER){fault=true;ready=false;Log("AI_FAULT disabled; player runtime unchanged");}
  ReleaseSRWLockExclusive(&lock);
 }busy.clear();
}
bool PlayerDependency(HMODULE player){
 // The player Special engine is now shared with this module. Compatibility
 // follows the fixed-size versioned mode API, not one historic build hash.
 if(!player||!GetProcAddress(player,"MWArcadeDriftNativeHandlingVersionV1")||!GetProcAddress(player,"MWArcadeDriftAcquireSpecialV1")||
    !GetProcAddress(player,"MWArcadeDriftRenewSpecialV1")||
    !GetProcAddress(player,"MWArcadeDriftReleaseSpecialV1"))return false;
#pragma warning(suppress:4191)
 auto get=reinterpret_cast<MWArcadeDriftGetModeV1Fn>(GetProcAddress(player,"MWArcadeDriftGetModeV1"));MWArcadeDriftModeV1 state{};state.size=sizeof(state);state.version=1;
 if(!get||!get(&state)||!state.ready)return false;
 return true;
}
DWORD WINAPI Initialize(void*){
 GetModuleFileNameW(module,ini,MAX_PATH);auto dot=wcsrchr(ini,L'.');if(!dot)return 0;wcscpy_s(dot,5,L".ini");wcscpy_s(logfile,ini);wcscpy_s(wcsrchr(logfile,L'.'),5,L".log");
 wchar_t value[8]{};GetPrivateProfileStringW(L"Diagnostics",L"Logging",L"",value,8,ini);logging=wcscmp(value,L"1")==0;enabled=GetPrivateProfileIntW(L"AI",L"Enabled",1,ini)==1;
 Log("MW Arcade Drift 0.2.0 (AI component) pid=%lu",GetCurrentProcessId());if(!enabled)return 0;
 auto digest=Digest(nullptr);if(GetModuleHandleW(nullptr)!=reinterpret_cast<HMODULE>(0x400000)||(digest!="80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c"&&digest!="b248271bf8eac8c9b283b8c95e3add672b713bf529b05f1780e58268493b9d06"&&digest!="0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342")){Log("AI_REJECTED host");return 0;}
 bool dependency=false;for(U i=0;i<150;i++){auto player=GetModuleHandleW(L"MWArcadeDrift.asi");if(PlayerDependency(player)){dependency=true;break;}Sleep(100);}if(!dependency){Log("AI_REJECTED player mode API/dependency not ready");return 0;}
 if(!Match(PhysicsStep,"\x56\x8b\xf1\x8b\x06\x57\xff\x50\x4c",9)||!Match(0x696fd0,"\x8b\x41\x30\x8b\x08\x8b\x54\x24\x04",9)||!Match(0x696ff0,"\x8b\x41\x30\x8b\x08\x8b\x54\x24\x04",9)||!Match(0x688090,"\x8b\x41\x2c\x8b\x40\x24\xc3",7)){Log("AI_REJECTED dispatch/setters/model getter");return 0;}
 HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Initialize),&pinned))return 0;
 if(MH_Initialize()!=MH_OK||MH_CreateHook(reinterpret_cast<void*>(PhysicsStep),&Hook,reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(PhysicsStep))!=MH_OK){Log("AI_REJECTED hook");return 0;}
 ready=true;Log("AI_READY hook=006BB4D0 native_branches=00699CC0/006BA510 opt_in=1 max_vehicles=16 driver=Racer+explicit_local_autodrive handling_writes=0");return 0;
}
mcdai::Result Acquire(const mcdai::BindingV1* input,std::uint64_t* token){
 if(!input||!token)return mcdai::BadRequest;*token=0;auto b=*input;
 if(!mcdai::validBinding(b))return mcdai::BadRequest;if(!ready.load()||fault.load())return mcdai::NotReady;
 U body=0;auto result=Identity(b,body,true);if(result!=mcdai::Ok)return result;
 mcd::Profile profile;mcd::SpecialProfile special;
 try{
  const char* name=reinterpret_cast<const char*(__thiscall*)(void*)>(0x688090)(reinterpret_cast<void*>(b.vehicle));
  if(!name||strnlen_s(name,64)>=64)return mcdai::ProfileError;
  auto root=std::filesystem::path(ini).parent_path()/L"MWArcadeDrift";
  if(b.mode==1){auto folder=mcd::vehicleFolder(root,name);profile=std::filesystem::exists(folder/L"drift.json")?mcd::loadVehicleProfile(root,name,false):mcd::parseProfile(mcd::readProfileJson(root/L"default_drift.json"),mcd::readProfileJson(root/L"default_camera.json"));if(!profile.enabled)return mcdai::Disabled;}
  else if(mcdai::usesNativeRaceTuning(b.mode,b.scope)){const auto frontShare=NativeFrontShare(b.vehicle);special.drift=mcdai::nativeRaceTuning(frontShare);
   const auto& tuning=special.drift;Log("AI_NATIVE_PROFILE handle=%u model=%s source=drive_layout_AI_policy player_profile_read=0 native_front_share=%.5f front=%.1f rear_dynamic=%.1f rear_static=%.1f rear_lateral=%.1f",b.simHandle,name,frontShare,tuning.frontGripPercent,tuning.rearGripPercent,tuning.rearStaticGripPercent,tuning.rearLateralGripPercent);}
  else{root/=L"modes/special";auto folder=mcd::vehicleFolder(root,name);special=std::filesystem::exists(folder/L"drift.json")?mcd::loadSpecialVehicleProfile(root,name,false):mcd::parseSpecialProfile(mcd::readProfileJson(root/L"default_drift.json"),mcd::readProfileJson(root/L"default_camera.json"));if(!special.enabled)return mcdai::Disabled;}
 }catch(const std::exception& error){Log("AI_PROFILE_REJECTED vehicle=%08X handle=%u mode=%u reason=%s",b.vehicle,b.simHandle,b.mode,error.what());return mcdai::ProfileError;}
 catch(...){Log("AI_PROFILE_REJECTED vehicle=%08X handle=%u mode=%u reason=unknown_exception",b.vehicle,b.simHandle,b.mode);return mcdai::ProfileError;}
 AcquireSRWLockExclusive(&lock);mcdai::Entry* e=nullptr;const auto acquiredAt=GetTickCount64();FlushExpiredEvidence(acquiredAt);result=registry.acquire(b,body,acquiredAt,e);
 if(e){e->controller.tuning=profile.drift;e->controller.special.tuning=special.drift;*token=e->token;leased=true;}
 ReleaseSRWLockExclusive(&lock);Log("AI_ACQUIRE vehicle=%08X handle=%u mode=%u result=%u",b.vehicle,b.simHandle,b.mode,result);return result;
}
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);auto thread=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
extern "C" U __cdecl MWArcadeDriftAI_GetCapabilitiesV1(mcdai::CapabilitiesV1* out){__try{if(!out||out->size!=sizeof(*out)||out->version!=1)return mcdai::BadRequest;mcdai::CapabilitiesV1 value;value.ready=ready.load()&&!fault.load();*out=value;return mcdai::Ok;}__except(EXCEPTION_EXECUTE_HANDLER){return mcdai::BadRequest;}}
extern "C" U __cdecl MWArcadeDriftAI_AcquireV1(const mcdai::BindingV1* b,std::uint64_t* token){__try{return Acquire(b,token);}__except(EXCEPTION_EXECUTE_HANDLER){return mcdai::BadRequest;}}
extern "C" U __cdecl MWArcadeDriftAI_RefreshV1(const mcdai::RefreshV1* r){__try{
 if(!r)return mcdai::BadRequest;const auto request=*r;if(!mcdai::validRefresh(request,GetTickCount64()))return mcdai::BadRequest;if(!ready.load()||fault.load())return mcdai::NotReady;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount64();FlushExpiredEvidence(now);
 if(const auto* e=registry.find(request.owner,request.token);e&&((request.flags&mcdai::ResetState)||!(request.flags&mcdai::AllowCorrection)||!e->fresh(now)))ForgetEvidence(*e);
 auto result=registry.refresh(request,now);ReleaseSRWLockExclusive(&lock);return result;
 }__except(EXCEPTION_EXECUTE_HANDLER){return mcdai::BadRequest;}}
extern "C" U __cdecl MWArcadeDriftAI_ReleaseV1(std::uint64_t owner,std::uint64_t token){AcquireSRWLockExclusive(&lock);if(const auto* e=registry.find(owner,token))ForgetEvidence(*e);auto result=registry.release(owner,token);ReleaseSRWLockExclusive(&lock);return result;}
extern "C" U __cdecl MWArcadeDriftAI_SetIntentV1(const mcdai::IntentV1* intent){__try{
 if(!intent)return mcdai::BadRequest;const auto request=*intent;if(!ready.load()||fault.load())return mcdai::NotReady;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount64();FlushExpiredEvidence(now);auto result=registry.setIntent(request,now);ReleaseSRWLockExclusive(&lock);return result;
 }__except(EXCEPTION_EXECUTE_HANDLER){return mcdai::BadRequest;}}
extern "C" U __cdecl MWArcadeDriftAI_SetPathAssistV1(const mcdai::PathAssistV1* input){__try{
 if(!input)return mcdai::BadRequest;const auto request=*input;
 AcquireSRWLockExclusive(&lock);const auto now=GetTickCount64();FlushExpiredEvidence(now);auto result=registry.setPathAssist(request,now);ReleaseSRWLockExclusive(&lock);return result;
 }__except(EXCEPTION_EXECUTE_HANDLER){return mcdai::BadRequest;}}
extern "C" U __cdecl MWArcadeDriftAI_GetStateV1(std::uint64_t owner,std::uint64_t token,mcdai::StateV1* out){__try{
 if(!out||out->size!=sizeof(*out)||out->version!=1)return mcdai::BadRequest;
 mcdai::StateV1 value;auto now=GetTickCount64();AcquireSRWLockExclusive(&lock);FlushExpiredEvidence(now);registry.expire(now);auto* e=registry.find(owner,token);
 if(e){value=e->view(now);if(fault.load()){value.eligible=0;value.reason=mcdai::Fault;value.deltaYaw=value.deltaDirection=value.deltaSpeed=0;}}
 ReleaseSRWLockExclusive(&lock);if(!e)return mcdai::NotOwned;*out=value;return mcdai::Ok;
 }__except(EXCEPTION_EXECUTE_HANDLER){return mcdai::BadRequest;}}
// Read-only eligibility for the shared native tyre hook hosted by the player
// module. No pointer supplied by the caller is dereferenced before matching an
// owned binding. Free roam and expired/released races retain native handling.
extern "C" U __cdecl MWArcadeDriftAI_NativeTireRaceV1(U vehicle){
 if(!ready.load()||fault.load()||!leased.load()||!Running()||!TryAcquireSRWLockShared(&lock))return 0;
 U result=0;const auto now=GetTickCount64();
 for(const auto& e:registry.entries)if(e.used&&e.binding.vehicle==vehicle&&e.binding.mode==2&&e.binding.scope!=mcdai::FreeRoam&&now<e.deadline){
  U body=0;if(Identity(e.binding,body,false)==mcdai::Ok&&body==e.body)result=e.fresh(now)?1u:2u;break;
 }
 ReleaseSRWLockShared(&lock);return result;
}
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_NativeTireRaceV1=_MWArcadeDriftAI_NativeTireRaceV1")
extern "C" U __cdecl MWArcadeDriftAI_NativeHandlingV1(U vehicle,MWArcadeDriftNativeHandlingV1* out){
 __try{if(!out||out->size!=sizeof(*out)||out->version!=1)return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
 if(!ready.load()||fault.load()||!leased.load()||!Running()||!TryAcquireSRWLockShared(&lock))return 0;
 U result=0;MWArcadeDriftNativeHandlingV1 value;const auto now=GetTickCount64();
 // Always release the lock, including invalidated game memory or destination.
 __try{
  for(const auto& e:registry.entries)if(e.used&&e.binding.vehicle==vehicle&&e.binding.mode==2&&e.binding.scope!=mcdai::FreeRoam&&now<e.deadline){
   U body=0;if(Identity(e.binding,body,false)==mcdai::Ok&&body==e.body){
    result=2;
    if(e.nativeHandlingFresh(now)){const auto& t=e.controller.special.tuning;
     value.front=t.frontGripPercent;value.rearDynamic=t.rearGripPercent;value.rearStatic=t.rearStaticGripPercent;value.rearLateral=t.rearLateralGripPercent;value.speedLoss=35;
     if(validNativeHandling(value))result=1;
    }
   }break;
  }
 }__finally{ReleaseSRWLockShared(&lock);}
 if(result==1){__try{*out=value;}__except(EXCEPTION_EXECUTE_HANDLER){return 0;}}
 return result;
}
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_NativeHandlingV1=_MWArcadeDriftAI_NativeHandlingV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_GetCapabilitiesV1=_MWArcadeDriftAI_GetCapabilitiesV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_AcquireV1=_MWArcadeDriftAI_AcquireV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_RefreshV1=_MWArcadeDriftAI_RefreshV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_ReleaseV1=_MWArcadeDriftAI_ReleaseV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_GetStateV1=_MWArcadeDriftAI_GetStateV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_SetIntentV1=_MWArcadeDriftAI_SetIntentV1")
#pragma comment(linker,"/EXPORT:MWArcadeDriftAI_SetPathAssistV1=_MWArcadeDriftAI_SetPathAssistV1")
