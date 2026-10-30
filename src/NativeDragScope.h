#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace mcd {
// EN1.3 Suspension base constructor 006B1A85 creates chassis at +70.
// Native aerodynamic force 0068D91B reads its layout (+78), then +70
// DRAG_COEFFICIENT, and applies a force opposing the measured velocity.
// This controls aerodynamic loss only, not tyre scrubbing or impact loss.
struct NativeDragScope {
 std::uint32_t slot=0,original=0;
 alignas(4) unsigned char layout[116]{};
 bool changed=false;
 bool begin(std::uint32_t primary,float scale){
  if(changed||primary<0x10000||primary>UINT32_MAX-0xcc||!std::isfinite(scale)||scale<0||scale>1)return false;
  __try{
   const auto type=*reinterpret_cast<std::uint32_t*>(primary),susp=*reinterpret_cast<std::uint32_t*>(primary+0x4c);
   if(!((type==0x8abb48&&susp==0x8abac8)||(type==0x8abcb0&&susp==0x8abc30)))return false;
   slot=primary+0x78;original=*reinterpret_cast<std::uint32_t*>(slot);
   // Both independently constructed chassis instances must refer to the
   // same original collection before installing this call-local copy.
   if(original<0x10000||original>UINT32_MAX-sizeof(layout)||*reinterpret_cast<std::uint32_t*>(primary+0xc8)!=original)return false;
   std::memcpy(layout,reinterpret_cast<void*>(original),sizeof(layout));
   float coefficient;std::memcpy(&coefficient,layout+112,4);
   if(!std::isfinite(coefficient)||coefficient<0||coefficient>10)return false;
   coefficient*=scale;std::memcpy(layout+112,&coefficient,4);
   *reinterpret_cast<std::uint32_t*>(slot)=reinterpret_cast<std::uint32_t>(layout);changed=true;return true;
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
 bool restore(){
  if(!changed)return true;
  __try{
   if(*reinterpret_cast<std::uint32_t*>(slot)!=reinterpret_cast<std::uint32_t>(layout))return false;
   *reinterpret_cast<std::uint32_t*>(slot)=original;changed=false;return true;
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 }
};
}
