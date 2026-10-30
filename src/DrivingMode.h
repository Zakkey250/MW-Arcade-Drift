#pragma once
#include <cstdint>
namespace mcd {
enum class DrivingMode : std::uint32_t { Normal=0,Arcade=1,Special=2 };
// Pure policy. Host serializes access; API calls never touch game objects.
struct ModeSelection {
 DrivingMode manual=DrivingMode::Arcade;
 std::uint64_t owner=0,token=0,expires=0,sequence=0;
 static bool validLease(std::uint32_t ms){return ms>=250&&ms<=10000;}
 void expire(std::uint64_t now){if(token&&now>=expires)owner=token=expires=0;}
 DrivingMode effective(std::uint64_t now){expire(now);return token?DrivingMode::Special:manual;}
 bool cycle(std::uint64_t now){expire(now);if(token)return false;manual=DrivingMode((std::uint32_t(manual)+1)%3);return true;}
 std::uint64_t acquire(std::uint64_t who,std::uint64_t now,std::uint32_t ms){
  expire(now);if(!who||token||!validLease(ms)||now>UINT64_MAX-ms)return 0;
  if(++sequence==0)++sequence;owner=who;token=sequence;expires=now+ms;return token;
 }
 bool renew(std::uint64_t who,std::uint64_t id,std::uint64_t now,std::uint32_t ms){
  expire(now);if(!who||!id||owner!=who||token!=id||!validLease(ms)||now>UINT64_MAX-ms)return false;expires=now+ms;return true;
 }
 bool release(std::uint64_t who,std::uint64_t id,std::uint64_t now){
  expire(now);if(!who||!id||owner!=who||token!=id)return false;owner=token=expires=0;return true;
 }
};
}
