#include <windows.h>

#include <bcrypt.h>

#include <cstdio>

#include <cstdarg>

#include <cstring>

#include <atomic>

#include <share.h>

#include <fstream>

#include <filesystem>

#include "ProfileFiles.h"

#include "MessageEncoding.h"

#include "DriftHud.h"
#include "PresentHud.h"
#include "PresentHooks.h"

#include <Xinput.h>

#include <intrin.h>

#include "Presentation.h"

#include "DriftCamera.h"

#include "PedalMapping.h"

#include "Controller.h"

#include "MotionHistory.h"

#include "SurfaceMotion.h"

#include "MinHook.h"

namespace {

HMODULE module;

wchar_t ini[MAX_PATH],logfile[MAX_PATH];

FILE* telemetry=nullptr;

mcd::Profile profile;

mcd::Preferences preferences;

SRWLOCK settingsLock=SRWLOCK_INIT;

mcd::HudTuning hudConfig;

bool vehicleEnabled=false;

std::string selectedName,selectedProfile;

std::atomic<unsigned> hudMode{0};

std::atomic<ULONGLONG> hudTick{0};

mcd::Controller controller;

mcd::MotionHistory motion;

std::atomic<bool> fault{false};

std::atomic_flag busy=ATOMIC_FLAG_INIT;

bool enabled=true,observe=false,independentTriggers=false,autoTriggers=true;

mcd::PedalMapping pedals;

std::atomic<bool> presentationEnabled{false};

struct VisualState {uint32_t suspension=0,engine=0;ULONGLONG tick=0;float redline=0,nativeRPM=0;int gear=-1;bool cameraValid=false;mcd::Vector forward{};mcd::DriftCamera camera;mcd::Presentation effect;mcd::FrontSteering front;};

SRWLOCK visualLock=SRWLOCK_INIT;

VisualState visual;

mcd::Presentation visualEnvelope;

mcd::FrontSteering frontEnvelope;

mcd::DriftCamera cameraEnvelope;

std::atomic<unsigned> cameraViews{0},cameraCalls{0},cameraFresh{0},cameraScoped{0};

uint32_t visualVehicle=0;

std::atomic<unsigned> wheelViews{0},steerViews{0},rpmViews{0},audioChanges{0},hudChanges{0};

using WheelGetter=float(__thiscall*)(void*,unsigned);

using RPMGetter=float(__thiscall*)(void*);

WheelGetter originalWheel=nullptr,originalSteer=nullptr;

RPMGetter originalRPM=nullptr;

struct RPMTrace {uint32_t caller=0;unsigned count=0;};

RPMTrace rpmTrace[32]{};

SRWLOCK rpmTraceLock=SRWLOCK_INIT;

uint32_t identity=0,bodyIdentity=0;

unsigned rows=0,steps=0,applied=0,refunded=0;

ULONGLONG lastSeen=0,lastLog=0,lastRow=0;

using Integrate=void(__thiscall*)(void*,float);

Integrate original=nullptr;

using Vec=mcd::Vector;

struct Snapshot {uint32_t vehicle=0,body=0,data=0;Vec velocity{},angular{},forward{},up{};mcd::Sample s;float nativeGas=0,nativeBrake=0;uint32_t wheels=0;unsigned char collisionFlags=0;DWORD pad=4;};

void Log(const char* fmt,...){FILE* f=nullptr;if(_wfopen_s(&f,logfile,L"a")||!f)return;fprintf(f,"[%llu] ",GetTickCount64());va_list a;va_start(a,fmt);vfprintf(f,fmt,a);va_end(a);fputc('\n',f);fclose(f);}

bool Read(uint32_t p,void* out,size_t n){SIZE_T got=0;return p>=0x10000&&p<=UINT32_MAX-n&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got)&&got==n;}

template<class T>bool Get(uint32_t p,T& out){return Read(p,&out,sizeof(out));}

bool Match(uint32_t p,const char* hex){unsigned char b[64]{};size_t n=strlen(hex)/2;if(n>64||!Read(p,b,n))return false;for(size_t i=0;i<n;i++){unsigned x=0;if(sscanf_s(hex+2*i,"%2x",&x)!=1||b[i]!=x)return false;}return true;}

bool Hash(char (&out)[65],HMODULE target=nullptr){

 wchar_t path[MAX_PATH];if(!GetModuleFileNameW(target,path,MAX_PATH))return false;

 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return false;

 LARGE_INTEGER size{};if(!GetFileSizeEx(f,&size)||(!target&&size.QuadPart!=6029312)){CloseHandle(f);return false;}

 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;

 bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;

 if(ok)ok=BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)>=0;

 BYTE buffer[32768],digest[32];DWORD n=0;

 while(ok){if(!ReadFile(f,buffer,sizeof(buffer),&n,nullptr)){ok=false;break;}if(!n)break;ok=BCryptHashData(h,buffer,n,0)>=0;}

 if(ok)ok=BCryptFinishHash(h,digest,32,0)>=0;

 if(h)BCryptDestroyHash(h);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(f);

 if(ok)for(size_t i=0;i<32;i++)sprintf_s(out+i*2,65-i*2,"%02x",digest[i]);return ok;

}

bool Guards(){

 return Match(0x6ba510,"81ec30050000538bd98b4378558b28")&&

 Match(0x671190,"8b41308b0083c020c3")&&Match(0x6711a0,"8b41308b0083c030c3")&&

 Match(0x671270,"8b41308b008b4c24040590000000")&&

 Match(0x696fd0,"8b41308b088b5424048b0283c1208901")&&

 Match(0x696ff0,"8b41308b088b5424048b0283c1308901");

}

bool Running(){

 uint32_t flow=0,time=0,state=0;unsigned char overlay=0;int a=0,b=0;DWORD pid=0;

 GetWindowThreadProcessId(GetForegroundWindow(),&pid);

 return pid==GetCurrentProcessId()&&Get(0x925e90,flow)&&flow==6&&Get(0x9885e0,time)&&time&&Get(time+0x2c,state)&&state!=4&&Get(0x9b0fb9,overlay)&&!overlay&&Get(0x9b41fc,a)&&a<=0&&Get(0x9b4220,b)&&b<=0;

}

std::filesystem::path ProfileRoot(){return std::filesystem::path(ini).parent_path()/L"MWArcadeDrift";}

struct ProfileFileLock {

 HANDLE h=INVALID_HANDLE_VALUE;

 ProfileFileLock(){h=CreateFileW((ProfileRoot()/L"profiles.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("profiles busy or not writable");}

 ~ProfileFileLock(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}

};

void ApplyPreferences(const mcd::Preferences& p){preferences=p;AcquireSRWLockExclusive(&settingsLock);hudConfig=p.hud;ReleaseSRWLockExclusive(&settingsLock);}

bool LoadSettings(){

 try{ProfileFileLock lock;const auto root=ProfileRoot();

  auto candidate=mcd::parseProfile(mcd::readProfileJson(root/L"default_drift.json"),mcd::readProfileJson(root/L"default_camera.json"));

  auto prefs=mcd::readPreferences(root);profile=candidate;ApplyPreferences(prefs);Log("CONFIG_READY schema=2 reload=vehicle-entry-or-Ctrl+D-ON");return true;

 }catch(const std::exception& e){Log("CONFIG_REJECTED %s",e.what());return false;}

}

bool ReloadVehicle(){

 try{ProfileFileLock lock;const auto root=ProfileRoot();auto candidate=mcd::loadVehicleProfile(root,selectedName,true);auto prefs=mcd::readPreferences(root);

  profile=candidate;ApplyPreferences(prefs);vehicleEnabled=profile.enabled;controller.tuning=profile.drift;

  Log("PROFILE_LOADED vehicle=%s enabled=%u",selectedName.c_str(),unsigned(vehicleEnabled));return true;

 }catch(const std::exception& e){vehicleEnabled=false;Log("PROFILE_REJECTED vehicle=%s reason=%s",selectedName.c_str(),e.what());return false;}

}

int pendingNotice=0;ULONGLONG noticeUntil=0,lastNoticeAttempt=0;

bool noticeReady=false;

void QueueNotice(int kind){pendingNotice=kind;noticeUntil=GetTickCount64()+3000;lastNoticeAttempt=0;}

bool SendNotice(const std::string& text){

 uint32_t count=0,list=0,player=0,object=0;

 if(!Get(0x92d884,count)||!count||count>16||!Get(0x92d87c,list)||!Get(list,player)||!player)return false;

 auto hud=reinterpret_cast<uint32_t(__thiscall*)(void*)>(0x6f8f10)(reinterpret_cast<void*>(player));

 if(!hud||!Get(hud+4,object)||!object)return false;

 auto generic=reinterpret_cast<void*(__thiscall*)(void*,void*)>(0x5d59f0)(reinterpret_cast<void*>(object),reinterpret_cast<void*>(0x5650b0));

 if(!generic)return false;

 return reinterpret_cast<bool(__thiscall*)(void*,const char*,bool,unsigned,unsigned,unsigned,unsigned)>(0x568030)(generic,text.c_str(),false,0x8ab83edb,0,0,2);

}

void TryNotice(){

 if(!pendingNotice||!noticeReady)return;const auto now=GetTickCount64();if(now>noticeUntil){pendingNotice=0;return;}if(now-lastNoticeAttempt<150)return;lastNoticeAttempt=now;

 const char* en[]={"","Drift mode ON - profile loaded","Drift mode OFF","Profile reload failed - drift OFF","Drift disabled in vehicle profile"};

 const wchar_t* ja[]={L"",L"ドリフトモード ON 設定を反映しました",L"ドリフトモード OFF",L"設定を読み込めません ドリフト OFF",L"車両設定でドリフトが無効です"};

 std::string text=en[pendingNotice];

 if(preferences.language=="ja"||(preferences.language=="auto"&&PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_JAPANESE)){

  uint32_t map=0,count=0;if(Get(0x91cf84,map)&&Get(map,count)&&count>=256&&count<=32768){std::vector<uint16_t> table(count);std::string encoded;

   if(Read(map+4,table.data(),table.size()*2)&&tire::Encode(ja[pendingNotice],table.data(),table.size(),encoded))text=encoded;

  }

 }

 if(SendNotice(text)){Log("MODE_NOTICE kind=%d",pendingNotice);pendingNotice=0;}

}

void GuardedNotice(){__try{TryNotice();}__except(EXCEPTION_EXECUTE_HANDLER){noticeReady=false;Log("NOTICE_DISABLED invalid native HUD");}}

void SelectVehicle(uint32_t vehicle){

 uint32_t attributes=0,nameAddress=0;char name[64]{};bool ok=false;

 // Verified GetVehicleName at 00688090: [IVehicle+2C] -> [+24].

 if(Get(vehicle+0x2c,attributes)&&Get(attributes+0x24,nameAddress)){

  for(unsigned i=0;i<sizeof(name);++i){if(!Get(nameAddress+i,name[i]))break;if(!name[i]){ok=i>0;break;}}

 }

 std::string model;

 try{if(ok)model=mcd::vehicleName(name);}catch(const std::exception&){ok=false;}

 selectedName=ok?model:"<unresolved>";selectedProfile=selectedName;

 if(ok)ReloadVehicle();else vehicleEnabled=false;

 Log("VEHICLE_CONFIG name=%s enabled=%u",selectedName.c_str(),unsigned(vehicleEnabled));

}

std::atomic<bool> hudFailed{false},hudDrawn{false};

bool DrawCurrentHud(IDirect3DDevice9* d,unsigned mode){mcd::HudTuning cfg;AcquireSRWLockShared(&settingsLock);cfg=hudConfig;ReleaseSRWLockShared(&settingsLock);return mcd::drawPresentHud(d,cfg,mode);}

void RenderHud(IDirect3DDevice9* d){
 __try {
  static unsigned calls=0;static ULONGLONG nextLog=0;++calls;
  const auto tick=hudTick.load();const auto mode=hudMode.load();const auto now=GetTickCount64();
  D3DDEVICE_CREATION_PARAMETERS creation{};DWORD owner=0;
  if(d&&SUCCEEDED(d->GetCreationParameters(&creation)))GetWindowThreadProcessId(creation.hFocusWindow,&owner);
  const bool fresh=tick&&now>=tick&&now-tick<250;
  const char* reason=hudFailed?"disabled":!mode?"no-player":!fresh?"stale-physics":!Running()?"not-driving":owner!=GetCurrentProcessId()?"foreign-device":"ready";
  if(!strcmp(reason,"ready")){
   if(DrawCurrentHud(d,mode)){reason="drawn";if(!hudDrawn.exchange(true))Log("HUD_ACTIVE independentSlipIcon=1 source=D3D9-Present-backbuffer");}
   else reason="draw-rejected";
  }
  if(now>=nextLog){nextLog=now+5000;Log("HUD_STATUS calls=%u mode=%u age=%llu reason=%s device=%08X",calls,mode,tick&&now>=tick?now-tick:0,reason,reinterpret_cast<uint32_t>(d));}
 }__except(EXCEPTION_EXECUTE_HANDLER){hudFailed=true;Log("HUD_DISABLED rendering exception; physics retained");}
}

void RefreshHud(ULONGLONG now){
 // Called only after a real player physics sample, never from ASI startup.
 // Re-read the live device on a slow cadence, supporting wrapper/device replacement.
 static ULONGLONG next=0;static uint32_t lastTarget=0;
 if(observe||hudFailed||now<next)return;next=now+1000;
 uint32_t d=0,vt=0,target=0;
 if(!Get(0x982bdc,d)||!d||!Get(d,vt)||!Get(vt+17*4,target)||!target)return;
 if(target==lastTarget)return;
 MEMORY_BASIC_INFORMATION page{};
 if(!VirtualQuery(reinterpret_cast<void*>(target),&page,sizeof(page))||page.State!=MEM_COMMIT||
    page.AllocationBase==GetModuleHandleW(nullptr)||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){
  hudFailed=true;Log("HUD_DISABLED Present target is not external executable memory");return;
 }
 const auto status=mcd::PresentHooks::install(reinterpret_cast<void*>(target),RenderHud);
 lastTarget=target;
 if(status==MH_OK)Log("HUD_READY source=D3D9-Present target=%08X device=%08X deferred=1",target,d);
 else {hudFailed=true;Log("HUD_DISABLED Present hook status=%d; physics retained",status);}
}

bool Method(uint32_t obj,unsigned slot,uint32_t expected){uint32_t vt=0,f=0;return Get(obj,vt)&&vt>=0x890000&&vt<0x8f0000&&Get(vt+slot*4,f)&&f==expected;}

bool VisualRead(VisualState& v){

 if(!presentationEnabled||fault.load()||!TryAcquireSRWLockShared(&visualLock))return false;

 v=visual;ReleaseSRWLockShared(&visualLock);

 return v.tick&&GetTickCount64()-v.tick<100;

}

float __fastcall WheelHook(void* self,void*,unsigned index){

 const auto caller=reinterpret_cast<uint32_t>(_ReturnAddress());

 const float native=originalWheel(self,index);VisualState v;

 if(caller!=0x6b71c8||!VisualRead(v)||v.suspension!=reinterpret_cast<uint32_t>(self)||index>=4||!std::isfinite(native))return native;

 ++wheelViews;return v.effect.wheel(native,index);

}

float __fastcall SteerHook(void* self,void*,unsigned index){

 const auto caller=reinterpret_cast<uint32_t>(_ReturnAddress());

 const float native=originalSteer(self,index);VisualState v;

 if(caller!=0x6b7144||!VisualRead(v)||v.suspension!=reinterpret_cast<uint32_t>(self)||index>=2||!std::isfinite(native))return native;

 ++steerViews;return v.front.active?v.front.angle[index]:native;

}

float __fastcall RPMHook(void* self,void*){

 const auto caller=reinterpret_cast<uint32_t>(_ReturnAddress());

 const float native=originalRPM(self);VisualState v;

 if(!VisualRead(v)||v.engine!=reinterpret_cast<uint32_t>(self))return native;

 if(TryAcquireSRWLockExclusive(&rpmTraceLock)){

  for(auto& t:rpmTrace)if(t.caller==caller||!t.caller){t.caller=caller;++t.count;break;}

  ReleaseSRWLockExclusive(&rpmTraceLock);

 }

 if(!mcd::presentationRPMCaller(caller))return native;

 ++rpmViews;const float shown=v.effect.rpm(native,v.redline);

 if(shown>native+.01f){if(caller==0x69419d)++audioChanges;if(caller==0x6f1536||caller==0x6f1568)++hudChanges;}

 return shown;

}

using LookAt=void*(__cdecl*)(void*,Vec*,Vec*,Vec*);

LookAt originalLookAt=nullptr;

bool CameraCaller(uint32_t caller){

 if(caller==0x47dcc1)return true;

 const HMODULE ws=GetModuleHandleW(L"NFSMostWanted.WidescreenFix.asi");if(!ws)return false;

 const uint32_t base=reinterpret_cast<uint32_t>(ws),rva=caller-base;

 if(rva!=0x2f064&&rva!=0x2f0a4)return false;

 static const bool known=[ws](){char hash[65]{};return Hash(hash,ws)&&!strcmp(hash,"af263e997168b7b6663ec7fc72e54ccfda0e85b1d9f507fa69ce3504d145a0b4");}();

 uint32_t pointer=0,callee=0;unsigned char behind=1;

 return known&&Match(caller-6,"ff15")&&Get(caller-4,pointer)&&pointer==base+0x1eb7d8&&Get(pointer,callee)&&callee==0x6cf0a0&&

  Match(caller,"83c4105f5e8be55dc3")&&Get(base+0x1eb7b3,behind)&&!behind;

}

bool CameraSite(){

 if(Match(0x47dcbc,"e8df1325008b4e10"))return true;

 if(!Match(0x47dcbc,"e8")||!Match(0x47dcc1,"8b4e10"))return false;

 uint32_t rel=0;if(!Get(0x47dcbd,rel))return false;

 HMODULE ws=GetModuleHandleW(L"NFSMostWanted.WidescreenFix.asi");

 if(!ws||0x47dcc1+rel!=reinterpret_cast<uint32_t>(ws)+0x2eac0)return false;

 char hash[65]{};return Hash(hash,ws)&&!strcmp(hash,"af263e997168b7b6663ec7fc72e54ccfda0e85b1d9f507fa69ce3504d145a0b4");

}

void* __cdecl CameraHook(void* matrix,Vec* eye,Vec* center,Vec* up){

 const auto caller=reinterpret_cast<uint32_t>(_ReturnAddress());

 VisualState v;

 const bool fresh=VisualRead(v);

 // Bounded diagnostics: distinguish hook calls, caller filtering and geometry.

 static std::atomic<ULONGLONG> nextTrace{0};

 const bool scoped=CameraCaller(caller);

 ++cameraCalls;if(fresh)++cameraFresh;if(scoped)++cameraScoped;

 ULONGLONG now=GetTickCount64(),due=nextTrace.load();

 if(fresh&&v.cameraValid&&std::abs(v.camera.orbit)>.005f&&now>=due&&nextTrace.compare_exchange_strong(due,now+5000)){

  const Vec offset=mcd::add(*eye,mcd::mul(*center,-1));

  Log("CAMERA_TRACE calls=%u fresh=%u scoped=%u applied=%u caller=%08X distance=%.3f up=(%.3f,%.3f,%.3f) forward=(%.3f,%.3f,%.3f) offset=(%.3f,%.3f,%.3f) orbit=%.4f",

   cameraCalls.load(),cameraFresh.load(),cameraScoped.load(),cameraViews.load(),caller,mcd::length(offset),up->x,up->y,up->z,v.forward.x,v.forward.y,v.forward.z,offset.x,offset.y,offset.z,v.camera.orbit);

 }

 if(scoped&&fresh&&v.cameraValid&&Running()){

  Vec e=*eye,u=*up,c=*center;

  if(v.camera.transform(e,c,u,mcd::cameraForward(v.forward))){

   if(++cameraViews==1)Log("CAMERA_APPLIED caller=%08X chaseGeometry=1 renderCoordinates=1",caller);

   return originalLookAt(matrix,&e,&c,&u);

  }

 }

 return originalLookAt(matrix,eye,center,up);

}

void PublishVisual(const Snapshot& s,ULONGLONG now){

 if(!presentationEnabled)return;

 uint32_t suspension=0,engine=0,vt=0,attributes=0;float redline=0;

 const bool interfaces=Get(s.vehicle+0x44,suspension)&&Get(suspension,vt)&&vt==0x8abac8&&

  Get(s.vehicle+0x48,engine)&&Get(engine,vt)&&vt==0x8ab6e0&&Get(engine+0xf8,attributes)&&Get(attributes+0x58,redline)&&std::isfinite(redline)&&redline>1000&&redline<20000;

 if(visualVehicle!=s.vehicle||now-lastSeen>100){visualEnvelope={};frontEnvelope={};cameraEnvelope={};visualVehicle=s.vehicle;}

 const bool safe=interfaces&&enabled&&vehicleEnabled&&!observe&&s.s.safe&&s.s.dt>0&&s.s.dt<=.05f;

 if(!safe){visualEnvelope={};frontEnvelope={};}else {
  // ISuspension=SuspensionRacer+0x4c; mTransInfo data=primary+0xdc.
  // Verified native rear/front drive getters read [primary+dc]+70 (front share).
  uint32_t transData=0;float split=-1.f;
  static const bool layoutGuard=Match(0x6b1c14,"c7464cc8ba8a00")&&Match(0x68de00,"8b81dc000000d94070")&&Match(0x68de20,"8b81dc000000d94070");
  const auto drive=layoutGuard&&Get(suspension+0x90,transData)&&Get(transData+0x70,split)?mcd::driveLayout(split):mcd::DriveLayout::Unknown;
  static uint32_t loggedVehicle=0;static auto loggedDrive=mcd::DriveLayout::Unknown;
  if(loggedVehicle!=s.vehicle||loggedDrive!=drive){Log("DRIVE_LAYOUT vehicle=%08X type=%s frontShare=%.4f counterSteer=1",s.vehicle,mcd::driveName(drive),split);loggedVehicle=s.vehicle;loggedDrive=drive;}
  visualEnvelope.step(s.s.dt,controller.phase==mcd::Phase::Drift&&std::abs(s.s.steer)>=.08f,s.s.beta,s.s.gas,s.s.brake,drive);
 }

 if(safe){

  const float front[2]={originalSteer(reinterpret_cast<void*>(suspension),0),originalSteer(reinterpret_cast<void*>(suspension),1)};

  if(std::isfinite(front[0])&&std::isfinite(front[1])&&std::abs(front[0])<.5f&&std::abs(front[1])<.5f)

   frontEnvelope.step(s.s.dt,front,visualEnvelope.blend);

  else frontEnvelope={};

 }

 const bool cameraValid=profile.camera.enabled&&interfaces&&enabled&&vehicleEnabled&&!observe&&s.s.dt>0&&s.s.dt<=.05f;

 cameraEnvelope.tuning=profile.camera;

 if(cameraValid&&profile.camera.enabled)cameraEnvelope.step(s.s.dt,safe&&controller.phase==mcd::Phase::Drift&&std::abs(s.s.steer)>=.08f,s.s.beta);

 else cameraEnvelope={};

 VisualState next;next.cameraValid=cameraValid;next.forward=s.forward;next.camera=cameraEnvelope;next.front=frontEnvelope;next.suspension=safe?suspension:0;next.engine=safe?engine:0;next.tick=now;next.redline=redline;next.effect=visualEnvelope;

 if(interfaces){Get(engine+0x12c,next.nativeRPM);uint32_t transmission=0,table=0;

  if(Get(s.vehicle+0x50,transmission)&&Get(transmission,table)&&table==0x8ab720)Get(transmission+0x38,next.gear);

 }

 AcquireSRWLockExclusive(&visualLock);visual=next;ReleaseSRWLockExclusive(&visualLock);

}

bool Finite(Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}

bool Capture(uint32_t primary,float dt,Snapshot& s){

 // IRigidBody::GetOwner is mov eax,[ecx-14h]; primary+48h is IRigidBody.

 // IVehicle::GetSimable maps IVehicle-80h to that owner. Reject non-player

 // bodies before scanning the vehicle list (avoid quadratic per-world work).

 uint32_t owner=0,candidateVT=0,driverClass=0;

 if(!Get(primary+0x34,owner)||owner>UINT32_MAX-0x114||!Get(owner+0x80,candidateVT)||candidateVT!=0x8aa828||!Get(owner+0x114,driverClass)||driverClass!=0)return false;

 if(!Running()){controller.reset();motion.clear();return false;}

 uint32_t list=0,count=0;if(!Get(0x92cd1c,list)||!Get(0x92cd24,count)||!list||!count||count>256)return false;

 uint32_t players=0;

 for(uint32_t i=0;i<count;i++){

  uint32_t p=0,vt=0,driver=0,active=0,body=0;unsigned char animation=0;

  if(!Get(list+i*4,p)||!p||!Get(p,vt)||vt!=0x8aa828||!Get(p+0x94,driver)||driver!=0||!Get(p+0xac,active)||!active||!Get(p+0xb0,animation)||animation)continue;

  ++players;

  if(Get(p-0x34,body)&&body==primary+0x48){s.vehicle=p;s.body=body;}

 }

 if(players!=1||!s.vehicle)return false;

 if(!Method(s.body,9,0x671190)||!Method(s.body,10,0x6711a0)||!Method(s.body,13,0x671270)||!Method(s.body,24,0x696fd0)||!Method(s.body,25,0x696ff0))return false;

 uint32_t ref=0,input=0,vt=0,getter=0,wheels=0,collision=0,cr=0,cd=0;unsigned char flags=0;

 if(!Get(s.body+0x30,ref)||!Get(ref,s.data)||!Get(s.data+0x20,s.velocity)||!Get(s.data+0x30,s.angular)||!Get(s.data+0x90,s.forward)||!Get(s.data+0x80,s.up)||

 !Get(s.vehicle+0x84,wheels)||!Get(s.vehicle+0x40,collision)||!Method(collision,17,0x670fb0)||!Get(collision+0x28,cr)||!Get(cr,cd)||!Get(cd+0x1c,flags)||

 !Get(s.vehicle+0x3c,input)||!Get(input,vt)||vt<0x890000||vt>=0x8f0000||!Get(vt+8,getter)||getter<0x401000||getter>=0x890000)return false;

 unsigned char code[7];uint32_t offset=0;if(!Read(getter,code,7)||code[0]!=0x8d)return false;

 if(code[1]==0x41&&code[3]==0xc3&&code[2]<0x80)offset=code[2];

 else if(code[1]==0x81&&code[6]==0xc3){memcpy(&offset,code+2,4);if(offset>0x200)return false;}else return false;

 float controls[8];if(!Read(input+offset,controls,sizeof(controls)))return false;

 for(float v:controls)if(!std::isfinite(v)||std::abs(v)>1.01f)return false;

 if(!Finite(s.velocity)||!Finite(s.angular)||!Finite(s.forward)||!Finite(s.up))return false;

 const float upLength=mcd::length(s.up);

 if(upLength<.95f||upLength>1.05f)return false;

 s.up=mcd::mul(s.up,1.f/upLength);

 const float norm=mcd::dot(s.forward,s.forward);

 if(norm<.5f||norm>1.1f)return false;

 s.s.dt=dt;s.s.speed=mcd::length(mcd::tangent(s.velocity,s.up));

 s.s.steer=controls[1];s.s.gas=controls[5];s.s.brake=controls[6];s.s.handbrake=controls[7];s.s.yaw=mcd::dot(s.angular,s.up);

 s.nativeGas=s.s.gas;s.nativeBrake=s.s.brake;

 // Optional legacy RT/LT compatibility only; default uses mapped game actions.

 // Use the selected pad; fallback only when exactly one XInput device exists.

 // Its versioned read-only API preserves the existing input route unchanged.

 using SelectedPad=DWORD(__cdecl*)(DWORD,DWORD*,XINPUT_STATE*);

 using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);

 XINPUT_STATE state{};DWORD index=4;bool available=false;

 const HMODULE provider=GetModuleHandleW(L"NFSMWRumble.asi");

 if(provider){

#pragma warning(suppress:4191)

  auto selected=reinterpret_cast<SelectedPad>(GetProcAddress(provider,"NFSMWRumbleGetSelectedPad"));

  available=selected&&selected(1,&index,&state)==ERROR_SUCCESS&&index<4;

 }else if(autoTriggers||independentTriggers){

  static HMODULE xi=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);

#pragma warning(suppress:4191)

  static auto getState=xi?reinterpret_cast<GetState>(GetProcAddress(xi,"XInputGetState")):nullptr;

  unsigned connectedPads=0;if(getState)for(DWORD i=0;i<4;++i){XINPUT_STATE candidate{};if(getState(i,&candidate)==ERROR_SUCCESS){++connectedPads;state=candidate;index=i;}}

  available=connectedPads==1;

 }

 static DWORD mappingPad=4;const DWORD current=available?index:4;

 if(mappingPad!=current){pedals.reset();mappingPad=current;}

 const int beforeGas=pedals.gasAxis,beforeBrake=pedals.brakeAxis;

 if(independentTriggers&&available)mcd::independentPedals(s.s,true,float(state.Gamepad.bLeftTrigger)/255.f,float(state.Gamepad.bRightTrigger)/255.f);

 else if(autoTriggers)pedals.step(s.s,available,float(state.Gamepad.bLeftTrigger)/255.f,float(state.Gamepad.bRightTrigger)/255.f);

 if(available&&(autoTriggers||independentTriggers))s.pad=index;

 if(beforeGas!=pedals.gasAxis||beforeBrake!=pedals.brakeAxis)Log("PEDAL_MAPPING pad=%lu gasAxis=%d brakeAxis=%d (0=LT 1=RT -1=learning)",current,pedals.gasAxis,pedals.brakeAxis);

 s.s.beta=mcd::surfaceBeta(s.forward,s.velocity,s.up);

 s.wheels=wheels;s.collisionFlags=flags;

 const float upNorm=s.up.x*s.up.x+s.up.y*s.up.y+s.up.z*s.up.z;

 s.s.safe=wheels>=3&&wheels<=4&&s.up.y>.85f&&upNorm>.9f&&upNorm<1.1f&&(flags&12)==0;

 return true;

}

void Step(uint32_t primary,float dt,float speedBefore){

 // Most calls are non-player bodies. The captured player handles pause reset

 // via the elapsed-time guard on resumption; no writes occur while paused.

 Snapshot s{};if(!Capture(primary,dt,s)){if(bodyIdentity==primary+0x48)motion.clear();return;}

 const auto now=GetTickCount64();

 const bool newVehicle=identity!=s.vehicle||bodyIdentity!=s.body;
 if(newVehicle||now-lastSeen>250){visualEnvelope={};frontEnvelope={};cameraEnvelope={};controller.reset(identity==s.vehicle&&bodyIdentity==s.body);identity=s.vehicle;bodyIdentity=s.body;motion.clear();if(newVehicle)SelectVehicle(s.vehicle);}

 if(now-lastSeen>100){visualEnvelope={};frontEnvelope={};cameraEnvelope={};}

 lastSeen=now;++steps;

 static DWORD previousPad=5;

 if(s.pad!=previousPad){controller.reset();motion.clear();previousPad=s.pad;Log("PEDAL_SOURCE %s pad=%lu",s.pad<4?"MappedPadTriggers":"NativeControls",s.pad);}

 DWORD foregroundProcess=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
 const bool key=foregroundProcess==GetCurrentProcessId()&&(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&(GetAsyncKeyState('D')&0x8000);static bool prior=false;

 if(key&&!prior){

  if(enabled&&vehicleEnabled){enabled=false;QueueNotice(2);}else {const bool loaded=ReloadVehicle();enabled=loaded&&vehicleEnabled;QueueNotice(!loaded?3:vehicleEnabled?1:4);}

  controller.reset();motion.clear();cameraEnvelope={};visualEnvelope={};frontEnvelope={};Log("TOGGLE enabled=%u key=Ctrl+D",unsigned(enabled));

 }prior=key;GuardedNotice();

 const auto before=controller.phase;const float oldSign=controller.sign;

 static bool priorBrake=false;

 const bool brake=s.s.brake>.25f;

 if(brake&&!priorBrake)Log("BRAKE_EDGE enabled=%u safe=%u calibrated=%u speed=%.2f steer=%.3f gas=%.3f brake=%.3f",unsigned(enabled),unsigned(s.s.safe),unsigned(controller.sign!=0),s.s.speed,s.s.steer,s.s.gas,s.s.brake);

 priorBrake=brake;

 const float upLength=std::sqrt(s.up.x*s.up.x+s.up.y*s.up.y+s.up.z*s.up.z);

 const float normalSpeed=upLength>.001f?(s.velocity.x*s.up.x+s.velocity.y*s.up.y+s.velocity.z*s.up.z)/upLength:999.f;

 motion.sample(s.s,s.body,now,s.velocity.x,s.velocity.y,s.velocity.z,normalSpeed,s.up.x,s.up.y,s.up.z);

 Vec actualVelocity=s.velocity;

 mcd::Output o{};if(enabled&&vehicleEnabled)o=controller.step(s.s);else controller.reset();

 PublishVisual(s,now);

 hudMode.store(enabled&&vehicleEnabled&&!observe?(controller.phase==mcd::Phase::Drift?2u:1u):3u);hudTick.store(now);RefreshHud(now);

 if(o.handbrakeEvent)Log("HANDBRAKE_PULSE kind=%s speed=%.3f beta=%.4f steer=%.3f",o.handbrakeEvent==1?"entry":"add",s.s.speed,s.s.beta,s.s.steer);

 if(controller.sign!=oldSign)Log("STEERING_CALIBRATED sign=%.0f",controller.sign);

 if(controller.phase!=before)Log("PHASE %u -> %u speed=%.2f beta=%.3f reason=%u dt=%.6f wheels=%u flags=%u upY=%.4f handbrake=%.3f yaw=%.4f steer=%.3f gas=%.3f brake=%.3f",unsigned(before),unsigned(controller.phase),s.s.speed,s.s.beta,controller.exitReason,dt,s.wheels,unsigned(s.collisionFlags),s.up.y,s.s.handbrake,s.s.yaw,s.s.steer,s.s.gas,s.s.brake);

 if(o.apply&&!observe){

  // Native setters, verified above, on the integrator's own thread only.

  Vec v=mcd::rotateSurface(s.velocity,s.up,o.deltaDirection,o.deltaSpeed);

  Vec w=mcd::add(s.angular,mcd::mul(s.up,o.deltaYaw));

  reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(0x696fd0)(reinterpret_cast<void*>(s.body),&v);

  reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(0x696ff0)(reinterpret_cast<void*>(s.body),&w);++applied;

  actualVelocity=v;

  if(o.deltaSpeed>0)++refunded;

 }

 const float actualNormal=upLength>.001f?(actualVelocity.x*s.up.x+actualVelocity.y*s.up.y+actualVelocity.z*s.up.z)/upLength:999.f;

 if(enabled&&vehicleEnabled)motion.commit(s.s,s.body,now,actualVelocity.x,actualVelocity.y,actualVelocity.z,controller.phase==mcd::Phase::Drift,actualNormal);else motion.clear();

 if(telemetry&&rows<120000&&now-lastRow>=(controller.phase==mcd::Phase::Drift?8u:50u)){lastRow=now;++rows;fprintf(telemetry,"%llu,%u,%u,%.6f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%.6f,%.6f,%.4f,%.4f,%lu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u,%.6f,%.6f,%.6f,%.6f,%u,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%d,%.4f,%.4f\n",now,enabled,unsigned(controller.phase),dt,s.s.speed,s.s.steer,s.s.gas,s.s.brake,s.s.beta,s.s.yaw,controller.sign,unsigned(s.s.safe),o.deltaYaw,o.deltaDirection,s.nativeGas,s.nativeBrake,s.pad,s.s.speedBeforeStep,o.deltaSpeed,o.targetYaw,o.targetSlip,speedBefore,s.s.nativeTurn,unsigned(s.s.nativeTurnValid),controller.exitReason,s.velocity.x,s.velocity.y,s.velocity.z,normalSpeed,s.wheels,unsigned(s.collisionFlags),s.up.y,s.s.handbrake,controller.entrySpeed,controller.slide,controller.pathAssist,controller.recoverySoft,visualEnvelope.blend,visualEnvelope.spin,visual.nativeRPM,visual.redline,visual.effect.rpm(visual.nativeRPM,visual.redline),visual.gear,controller.handbrakePower,controller.handbrakeWindow);if(rows%20==0)fflush(telemetry);}

 if(now-lastLog>5000){

 Log("CAMERA_STATUS calls=%u fresh=%u scoped=%u applied=%u orbit=%.5f orbitVelocity=%.5f roll=%.5f zoom=%.5f zoomVelocity=%.5f physicsSafe=%u",cameraCalls.load(),cameraFresh.load(),cameraScoped.load(),cameraViews.load(),cameraEnvelope.orbit,cameraEnvelope.orbitVelocity,cameraEnvelope.roll,cameraEnvelope.zoom,cameraEnvelope.zoomVelocity,unsigned(s.s.safe));

  Log("PRESENTATION_EFFECT audioChanges=%u hudChanges=%u suspension=%08X engine=%08X",audioChanges.load(),hudChanges.load(),visual.suspension,visual.engine);

  Log("PRESENTATION steerReads=%u wheelReads=%u rpmReads=%u blend=%.3f spin=%.3f pathAssist=%.4f recoverySoft=%.3f",steerViews.load(),wheelViews.load(),rpmViews.load(),visualEnvelope.blend,visualEnvelope.spin,controller.pathAssist,controller.recoverySoft);

  if(TryAcquireSRWLockExclusive(&rpmTraceLock)){for(auto& t:rpmTrace)if(t.caller){Log("RPM_CALLER return=%08X count=%u override=%u",t.caller,t.count,unsigned(mcd::presentationRPMCaller(t.caller)));}ReleaseSRWLockExclusive(&rpmTraceLock);}

 lastLog=now;Log("PLAYER_SAMPLES steps=%u applied=%u speedRefundSamples=%u mode=%s calibrated=%u",steps,applied,refunded,observe?"Observe":"Assist",unsigned(controller.sign!=0));}

}

void GuardedStep(uint32_t p,float dt,float speedBefore){__try{Step(p,dt,speedBefore);}__except(EXCEPTION_EXECUTE_HANDLER){fault.store(true);Log("FAULT assistance disabled restart required");}}

void __fastcall Hook(void* self,void*,float dt){

 const bool acquired=!busy.test_and_set();float speedBefore=0;

 if(acquired&&!fault.load()&&bodyIdentity==reinterpret_cast<uint32_t>(self)+0x48&&controller.phase==mcd::Phase::Drift){

  uint32_t ref=0,data=0;Vec v{};

  if(Get(reinterpret_cast<uint32_t>(self)+0x78,ref)&&Get(ref,data)&&Get(data+0x20,v)&&Finite(v))speedBefore=std::hypot(v.x,v.z);

 }

 original(self,dt);

 if(!acquired)return;

 if(!fault.load())GuardedStep(reinterpret_cast<uint32_t>(self),dt,speedBefore);busy.clear();

}

DWORD WINAPI Initialize(void*){

 GetModuleFileNameW(module,ini,MAX_PATH);wchar_t* dot=wcsrchr(ini,L'.');if(!dot)return 0;wcscpy_s(dot,5,L".ini");wcscpy_s(logfile,ini);wcscpy_s(wcsrchr(logfile,L'.'),5,L".log");

 Log("MW Arcade Drift 0.1.0 pid=%lu",GetCurrentProcessId());

 if(!GetPrivateProfileIntW(L"Drift",L"Enabled",1,ini)){Log("DISABLED");return 0;}

 autoTriggers=GetPrivateProfileIntW(L"Input",L"AutoIndependentTriggers",1,ini)!=0;

 observe=GetPrivateProfileIntW(L"Drift",L"ObserveOnly",0,ini)!=0;

 independentTriggers=GetPrivateProfileIntW(L"Input",L"LegacyIndependentTriggers",0,ini)!=0;

 Log("INPUT_MAPPING source=IInput::GetControls steering=1 gas=5 brake=6 handbrake=7 legacyRTLT=%u",unsigned(independentTriggers));

 char hash[65]{};if(!Hash(hash)||(strcmp(hash,"80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c")&&strcmp(hash,"b248271bf8eac8c9b283b8c95e3add672b713bf529b05f1780e58268493b9d06"))||reinterpret_cast<uint32_t>(GetModuleHandleW(nullptr))!=0x400000){Log("REJECTED exe=%s",hash);return 0;}

 if(!LoadSettings())return 0;

 noticeReady=Match(0x6f8f10,"8b4128c3")&&Match(0x5d59f0,"83ec08568b71088b4904578b")&&Match(0x5650b0,"b8b0505600c3")&&Match(0x568030,"568bf18b4614578b7c24203b");

 Log("NOTICE_READY active=%u",unsigned(noticeReady));

 if(!Match(0x688090,"8b412c8b4024c3")){Log("REJECTED vehicle name getter");return 0;}

 if(!Guards()){Log("REJECTED hook or setter bytes");return 0;}

 HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Initialize),&pinned))return 0;

 wchar_t csv[MAX_PATH];wcscpy_s(csv,ini);wcscpy_s(wcsrchr(csv,L'.'),5,L".csv");

 if(GetPrivateProfileIntW(L"Drift",L"Telemetry",1,ini))telemetry=_wfsopen(csv,L"w",_SH_DENYNO);

 if(telemetry){fprintf(telemetry,"ms,enabled,phase,dt,speed_mps,steer,gas,brake,beta_rad,yaw_radps,steering_sign,safe,delta_yaw,delta_direction,native_gas,native_brake,selected_pad,speed_before_step,delta_speed,target_yaw,target_slip,hook_speed_before,native_turn,native_turn_valid,exit_reason,vx,vy,vz,normal_speed,wheels,collision_flags,up_y,handbrake,reference_speed,slide_state,path_assist,recovery_soft,visual_blend,visual_spin,physics_rpm,redline,visual_rpm,gear,handbrake_power,handbrake_window\n");fflush(telemetry);}

 if(MH_Initialize()!=MH_OK||MH_CreateHook(reinterpret_cast<void*>(0x6ba510),&Hook,reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(0x6ba510))!=MH_OK){Log("REJECTED hook installation");if(telemetry){fclose(telemetry);telemetry=nullptr;}return 0;}

 if(GetPrivateProfileIntW(L"Presentation",L"Enabled",1,ini)&&!observe){

  const bool guards=Match(0x6a96d0,"8b44240483f8027310")&&Match(0x68e570,"8b4424048b8c8168010000")&&Match(0x6a03a0,"d9812c010000c3")&&

   Match(0x6b7141,"ff5058d80dd8ab8a00")&&Match(0x6b71c5,"ff5050d95c2420")&&

   Match(0x69419a,"ff5004d95c2458")&&Match(0x6f1565,"ff5204d95c2420")&&Match(0x6f1533,"ff5004d95c2420")&&Match(0x6b73f8,"ff520451d91c24");

  if(guards&&MH_CreateHook(reinterpret_cast<void*>(0x6a96d0),&SteerHook,reinterpret_cast<void**>(&originalSteer))==MH_OK&&

   MH_CreateHook(reinterpret_cast<void*>(0x68e570),&WheelHook,reinterpret_cast<void**>(&originalWheel))==MH_OK&&

   MH_CreateHook(reinterpret_cast<void*>(0x6a03a0),&RPMHook,reinterpret_cast<void**>(&originalRPM))==MH_OK&&

   MH_QueueEnableHook(reinterpret_cast<void*>(0x6a96d0))==MH_OK&&MH_QueueEnableHook(reinterpret_cast<void*>(0x68e570))==MH_OK&&

   MH_QueueEnableHook(reinterpret_cast<void*>(0x6a03a0))==MH_OK&&MH_ApplyQueued()==MH_OK){presentationEnabled=true;Log("PRESENTATION_READY callerScoped=1 physicsRPMWrites=0 gearboxWrites=0 awaiting runtime read counts");}

  else {Log("PRESENTATION_DISABLED guard or hook failure; native presentation retained");}

 }

 if(presentationEnabled&&GetPrivateProfileIntW(L"Camera",L"Enabled",1,ini)){

  if(Match(0x6cf0a0,"558bec83e4f081ec8c000000")&&CameraSite()&&

   MH_CreateHook(reinterpret_cast<void*>(0x6cf0a0),&CameraHook,reinterpret_cast<void**>(&originalLookAt))==MH_OK&&

   MH_EnableHook(reinterpret_cast<void*>(0x6cf0a0))==MH_OK)Log("CAMERA_READY orbitMaxDeg=28 rollMaxDeg=2 awaiting runtime camera calls");

  else Log("CAMERA_DISABLED incompatible bytes or hook conflict");

 }

 Log("HUD_WAIT waiting for player/device; no game render code patch");

 Log("ACTIVE mode=%s hook=006BA510 playerOnly=1 handlingWrites=0; awaiting player samples",observe?"Observe":"Assist");return 0;

}

}

BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);HANDLE thread=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
