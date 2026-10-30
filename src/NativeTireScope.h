#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <initializer_list>

namespace mcd {
// EN1.3 SuspensionRacer: primary+0x98 is Attrib::Gen::tires, whose
// Instance::mLayoutPtr is +8. Wheel+0x100 refers to that same instance.
// 0069DAD1 / 0069DAD9 consume DYNAMIC_GRIP / STATIC_GRIP by axle.
// Use a private layout only during one native suspension call. The shared
// collection and all saved car data remain untouched, including on failure.
struct NativeTireScope {
 std::uint32_t instance=0,original=0;
 alignas(4) unsigned char layout[80]{};
 bool changed=false;
 bool begin(std::uint32_t primary,float front,float rear,float rearStatic=0,float playerRearScale=1){
  if(changed)return false;
  if(!std::isfinite(playerRearScale)||playerRearScale<.1f||playerRearScale>1.f)return false;
  if(rearStatic==0)rearStatic=rear;
  if(!std::isfinite(rearStatic)||rearStatic<.2f||rearStatic>1.f)return false;
  if(primary<0x10000||primary>UINT32_MAX-0x1c4||!std::isfinite(front)||!std::isfinite(rear)||front<.2f||front>1.f||rear<.2f||rear>1.f)return false;
  __try{
   const auto primaryType=*reinterpret_cast<std::uint32_t*>(primary),suspensionType=*reinterpret_cast<std::uint32_t*>(primary+0x4c);
   const bool player=primaryType==0x8abcb0&&suspensionType==0x8abc30;
   if(!player&&(primaryType!=0x8abb48||suspensionType!=0x8abac8))return false;
   instance=primary+(player?0xac:0x98);original=*reinterpret_cast<std::uint32_t*>(instance+8);
   if(original<0x10000||original>UINT32_MAX-sizeof(layout))return false;
   // Do not assume a wheel type or bind another car's attribute instance.
   // SuspensionPlayer uses TirePlayer (+12c, wheel+f8/+104), whereas
   // SuspensionRacer uses Tire ( +1b4, wheel+100/+10c ). Verified separately
   // at 0069F6F0/0068FF00 and each native grip consumer; never reinterpret one.
   for(unsigned n=0;n<4;++n){const auto wheel=*reinterpret_cast<std::uint32_t*>(primary+(player?0x12c:0x1b4)+n*4);
    if(wheel<0x10000||wheel>UINT32_MAX-0x110||*reinterpret_cast<std::uint32_t*>(wheel+(player?0xf8:0x100))!=instance||*reinterpret_cast<unsigned*>(wheel+(player?0x104:0x10c))!=n/2)return false;}
   std::memcpy(layout,reinterpret_cast<void*>(original),sizeof(layout));
   // TirePlayer consumes GRIP_SCALE for lateral force at 0069F2E0,
   // and again in native skid-drag compensation at 0069F52D. Preserve
   // that coupling; never replace either computed force. Racer's different
   // tyre model is deliberately not assigned this unverified parameter.
   if(player&&playerRearScale!=1){
    float value;std::memcpy(&value,layout+28,4);
    if(!std::isfinite(value)||value<.1f||value>10.f)return false;
    value*=playerRearScale;std::memcpy(layout+28,&value,4);
   }
   for(unsigned offset:{32u,56u})for(unsigned axle=0;axle<2;++axle){
    float value;std::memcpy(&value,layout+offset+axle*4,4);
    if(!std::isfinite(value)||value<.1f||value>10.f)return false;
    value*=axle?(offset==56?rearStatic:rear):front;std::memcpy(layout+offset+axle*4,&value,4);
   }
   *reinterpret_cast<std::uint32_t*>(instance+8)=reinterpret_cast<std::uint32_t>(layout);changed=true;return true;
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
 bool restore(){
  if(!changed)return true;
  __try{if(*reinterpret_cast<std::uint32_t*>(instance+8)!=reinterpret_cast<std::uint32_t>(layout))return false;
   *reinterpret_cast<std::uint32_t*>(instance+8)=original;changed=false;return true;
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
};
}
