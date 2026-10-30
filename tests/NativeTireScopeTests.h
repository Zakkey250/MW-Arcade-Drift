#pragma once
#include "../src/NativeTireScope.h"
#include "../src/NativeDragScope.h"
#include "../src/NativeHandlingAPI.h"

namespace native_tire_test {
struct Fixture {
 alignas(4) unsigned char primary[0x200]{},wheels[4][0x140]{},layout[80]{};
 unsigned layoutOffset=0xa0;
 static void put(void* base,unsigned offset,std::uint32_t v){std::memcpy(static_cast<unsigned char*>(base)+offset,&v,4);}
 static std::uint32_t address(void* p){return reinterpret_cast<std::uint32_t>(p);}
 Fixture(bool player=false){
  layoutOffset=player?0xb4:0xa0;
  put(primary,0,player?0x8abcb0:0x8abb48);put(primary,0x4c,player?0x8abc30:0x8abac8);put(primary,layoutOffset,address(layout));
  for(unsigned n=0;n<4;++n){put(primary,(player?0x12c:0x1b4)+4*n,address(wheels[n]));put(wheels[n],player?0xf8:0x100,address(primary)+layoutOffset-8);put(wheels[n],player?0x104:0x10c,n/2);}
  const float grips[]{2.f,2.4f,2.2f,2.6f};
  std::memcpy(layout+32,grips,8);std::memcpy(layout+56,grips+2,8);
  const float scales[]{1.175f,1.225f};std::memcpy(layout+24,scales,8);
 }
 std::uint32_t pointer(){std::uint32_t p;std::memcpy(&p,primary+layoutOffset,4);return p;}
};
// Mirrors native SEH unwinding, not a C++ destructor that cannot be used in
// the hook's __try function. The temporary pointer must never escape it.
inline bool exceptionProbe(Fixture* f){
 mcd::NativeTireScope scope;
 __try {
  __try {if(!scope.begin(Fixture::address(f->primary),.8f,.5f))return false;RaiseException(0xe0421234,0,0,nullptr);}
  __finally {scope.restore();}
 } __except(EXCEPTION_EXECUTE_HANDLER){}
 return f->pointer()==Fixture::address(f->layout);
}
inline bool dragExceptionProbe(Fixture* f,unsigned char* original){
 mcd::NativeDragScope scope;
 __try {__try {if(!scope.begin(Fixture::address(f->primary),.5f))return false;RaiseException(0xe0421234,0,0,nullptr);}
 __finally {scope.restore();}}__except(EXCEPTION_EXECUTE_HANDLER){}
 std::uint32_t ptr=0;std::memcpy(&ptr,f->primary+0x78,4);return ptr==Fixture::address(original);
}

}
inline void nativeTireScopeTests(){

 {MWArcadeDriftNativeHandlingV1 v;require(validNativeHandling(v),"stock handling contract is valid");
  v.front=std::numeric_limits<float>::quiet_NaN();require(!validNativeHandling(v),"handling ABI rejects NaN before native layout mutation");
  v.front=85;v.rearLateral=9;require(!validNativeHandling(v),"handling ABI enforces bounded adjustment envelope");
  v.rearLateral=50;v.size=24;require(!validNativeHandling(v),"incompatible handling ABI cannot supply partial values");}
 using namespace native_tire_test;
 for(bool player:{false,true}){
  Fixture car(player);alignas(4) unsigned char chassis[116]{};float coefficient=.32f;std::memcpy(chassis+112,&coefficient,4);
  Fixture::put(car.primary,0x78,Fixture::address(chassis));Fixture::put(car.primary,0xc8,Fixture::address(chassis));
  mcd::NativeDragScope drag;float scaled=0;
  require(drag.begin(Fixture::address(car.primary),.5f),"native aero layout accepted for original suspension class");
  std::memcpy(&scaled,drag.layout+112,4);
  require(std::abs(scaled-.16f)<1e-6&&std::memcmp(chassis,drag.layout,112)==0,"aero scope changes only private drag coefficient");
  require(drag.restore()&&dragExceptionProbe(&car,chassis),"aero layout restored after normal return and SEH");
  Fixture::put(car.primary,0xc8,1);
  require(!drag.begin(Fixture::address(car.primary),.5f),"mismatched chassis collection is rejected");
 }
 Fixture car,other;unsigned char before[80];std::memcpy(before,car.layout,80);
 mcd::NativeTireScope scope;
 require(scope.begin(Fixture::address(car.primary),.8f,.5f),"native tyre verified instance accepts scoped values");
 require(car.pointer()==Fixture::address(scope.layout)&&other.pointer()==Fixture::address(other.layout),"native tyre override is isolated to one suspension instance");
 require(!scope.begin(Fixture::address(other.primary),.8f,.5f)&&car.pointer()==Fixture::address(scope.layout),"active scope cannot be rebound to another car");
 require(!std::memcmp(before,car.layout,80),"shared VLT layout is never modified");
 float actual[4];std::memcpy(actual,scope.layout+32,8);std::memcpy(actual+2,scope.layout+56,8);
 require(std::abs(actual[0]-1.6f)<1e-5&&std::abs(actual[1]-1.2f)<1e-5&&std::abs(actual[2]-1.76f)<1e-5&&std::abs(actual[3]-1.3f)<1e-5,"front and rear native friction remain independently scaled");
 require(scope.restore()&&scope.restore()&&car.pointer()==Fixture::address(car.layout),"normal return restores pointer and repeat restore is harmless");
 require(exceptionProbe(&car)&&!std::memcmp(before,car.layout,80),"SEH exception restores native layout without corrupting shared data");
 {Fixture human(true);mcd::NativeTireScope playerScope;
  require(playerScope.begin(Fixture::address(human.primary),.85f,.4f,.6f,.5f),"rear force limit and sliding coefficient can be tested independently");
  float rearDynamic=0,rearStatic=0;std::memcpy(&rearDynamic,playerScope.layout+36,4);std::memcpy(&rearStatic,playerScope.layout+60,4);
  require(std::abs(rearDynamic-2.4f*.4f)<1e-6&&std::abs(rearStatic-2.6f*.6f)<1e-6&&playerScope.restore(),"native force-circle trial does not change dynamic coefficient or shared data");
  require(playerScope.begin(Fixture::address(human.primary),.85f,.3f,0,.65f),"player lateral coefficient accepts isolated native parameter trial");
  float scale=0;std::memcpy(&scale,playerScope.layout+28,4);
  require(std::abs(scale-1.225f*.65f)<1e-6&&playerScope.restore(),"native rear scale preserves original layout and restores");
  require(playerScope.begin(Fixture::address(human.primary),.85f,.3f)&&playerScope.restore()&&exceptionProbe(&human),"player tyre layout and exception restoration independently verified");
  Fixture::put(human.primary,0x4c,0x8abac8);
  require(!playerScope.begin(Fixture::address(human.primary),.85f,.3f),"mixed player/racer layout rejected");}
 for(int failure=0;failure<6;++failure){
  Fixture invalid;mcd::NativeTireScope attempt;
  if(failure==0)Fixture::put(invalid.primary,0,0);
  if(failure==1)Fixture::put(invalid.wheels[2],0x10c,0);
  if(failure==2)Fixture::put(invalid.wheels[1],0x100,Fixture::address(other.primary)+0x98);
  if(failure==3)Fixture::put(invalid.primary,0x1b4,1);
  if(failure==4){float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(invalid.layout+60,&nan,4);}
  require(!attempt.begin(Fixture::address(invalid.primary),failure==5?1.1f:.8f,.5f)&&invalid.pointer()==Fixture::address(invalid.layout),"unknown native layout or invalid friction fails without pointer mutation");
 }
}
