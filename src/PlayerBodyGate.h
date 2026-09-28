#pragma once
#include <windows.h>
#include <cstdint>

namespace mcd {
inline bool safeWord(std::uint32_t address,std::uint32_t& value){
 SIZE_T got=0;
 return address>=0x10000&&address<=UINT32_MAX-sizeof(value)&&
  ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,sizeof(value),&got)&&got==sizeof(value);
}
inline bool playerBodySafe(std::uint32_t primary){
 std::uint32_t owner=0,vt=0,driver=0;
 return safeWord(primary+0x34,owner)&&owner>=0x10000&&owner<=UINT32_MAX-0x114&&
  safeWord(owner+0x80,vt)&&vt==0x8aa828&&safeWord(owner+0x114,driver)&&driver==0;
}
inline bool playerBody(std::uint32_t primary){
 if(primary<0x10000||primary>UINT32_MAX-0x34)return false;
 // Called on the native integrator thread. Most bodies are not the player;
 // avoid kernel ReadProcessMemory calls for the common case. An unexpected
 // invalid pointer falls back to the previous safe-reader behavior.
 __try{
  const auto owner=*reinterpret_cast<const std::uint32_t*>(primary+0x34);
  if(owner<0x10000||owner>UINT32_MAX-0x114)return false;
  if(*reinterpret_cast<const std::uint32_t*>(owner+0x80)!=0x8aa828)return false;
  return *reinterpret_cast<const std::uint32_t*>(owner+0x114)==0;
 }__except(EXCEPTION_EXECUTE_HANDLER){return playerBodySafe(primary);}
}
}
