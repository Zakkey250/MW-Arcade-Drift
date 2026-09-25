#pragma once
#include "Tuning.h"
#include "../third_party/nlohmann/json.hpp"
#include <map>
#include <string>
#include <set>
#include <stdexcept>
#include <cmath>
namespace mcd {
using Json=nlohmann::json;
struct VehicleRule {bool enabled=true;std::string profile="default";};
struct Settings {
 std::map<std::string,DriftTuning> profiles{{"default",{}}};
 CameraTuning camera;HudTuning hud;VehicleRule fallback;
 std::map<std::string,VehicleRule> vehicles;
 VehicleRule select(const std::string& name)const {
  auto i=vehicles.find(name);return i==vehicles.end()?fallback:i->second;
 }
};
inline std::string vehicleName(std::string s){
 if(s.empty()||s.size()>63)throw std::runtime_error("vehicle name must have 1..63 ASCII characters");
 for(char& c:s){if(c>='a'&&c<='z')c=char(c-'a'+'A');
  if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))throw std::runtime_error("invalid vehicle name");}
 return s;
}
inline void keys(const Json& j,const std::set<std::string>& allowed){
 if(!j.is_object())throw std::runtime_error("expected JSON object");
 for(auto i=j.begin();i!=j.end();++i)if(!allowed.count(i.key()))throw std::runtime_error("unknown setting: "+i.key());
}
inline float number(const Json& j,const char* k,float d,float lo,float hi){
 if(!j.contains(k))return d;
 if(!j[k].is_number())throw std::runtime_error(std::string(k)+": expected number");
 const float n=j[k].get<float>();if(!std::isfinite(n)||n<lo||n>hi)throw std::runtime_error(std::string(k)+": out of range");return n;
}
inline bool boolean(const Json& j,const char* k,bool d){
 if(!j.contains(k))return d;if(!j[k].is_boolean())throw std::runtime_error(std::string(k)+": expected boolean");return j[k].get<bool>();
}
inline void version(const Json& j){if(!j.contains("schemaVersion")||!j["schemaVersion"].is_number_integer()||j["schemaVersion"]!=1)throw std::runtime_error("schemaVersion must be 1");}
inline DriftTuning drift(const Json& j,DriftTuning d={}){
#define MCD_KEY(n,v,l,h) #n,
 keys(j,{MCD_DRIFT_FIELDS(MCD_KEY)});
#define MCD_READ(n,v,l,h) d.n=number(j,#n,d.n,l,h);
 MCD_DRIFT_FIELDS(MCD_READ)
#undef MCD_READ
 if(d.boostEndKmh<=d.boostStartKmh)throw std::runtime_error("boostEndKmh must exceed boostStartKmh");
 if(d.baseSlipRad+d.slideSlipRad+d.handbrakeSlipRad>.65f)throw std::runtime_error("combined slip target exceeds 0.65 radians");
 return d;
}
inline VehicleRule rule(const Json& j,VehicleRule d={}){
 keys(j,{"enabled","profile"});d.enabled=boolean(j,"enabled",d.enabled);
 if(j.contains("profile")){if(!j["profile"].is_string())throw std::runtime_error("profile must be a string");d.profile=j["profile"].get<std::string>();}return d;
}
inline Settings parseSettings(const Json& dj,const Json& cj,const Json& vj){
 Settings s;keys(dj,{"schemaVersion","profiles"});version(dj);
 if(!dj.contains("profiles")||!dj["profiles"].is_object()||!dj["profiles"].contains("default")||dj["profiles"].size()>128)throw std::runtime_error("profiles requires default, maximum 128");
 const auto base=drift(dj["profiles"]["default"]);s.profiles.clear();
 for(auto i=dj["profiles"].begin();i!=dj["profiles"].end();++i){if(i.key().empty()||i.key().size()>64)throw std::runtime_error("invalid profile name");s.profiles[i.key()]=drift(i.value(),base);}
 keys(cj,{"schemaVersion","enabled",MCD_CAMERA_FIELDS(MCD_KEY)});version(cj);s.camera.enabled=boolean(cj,"enabled",true);
#define MCD_READ_CAM(n,v,l,h) s.camera.n=number(cj,#n,s.camera.n,l,h);
 MCD_CAMERA_FIELDS(MCD_READ_CAM)
#undef MCD_READ_CAM
 keys(vj,{"schemaVersion","default","vehicles","hud"});version(vj);
 if(vj.contains("default"))s.fallback=rule(vj["default"]);
 if(vj.contains("vehicles")){
  if(!vj["vehicles"].is_object()||vj["vehicles"].size()>2048)throw std::runtime_error("vehicles must be an object, maximum 2048");
  for(auto i=vj["vehicles"].begin();i!=vj["vehicles"].end();++i)
   if(!s.vehicles.emplace(vehicleName(i.key()),rule(i.value(),s.fallback)).second)throw std::runtime_error("duplicate normalized vehicle name");
 }
 if(!s.profiles.count(s.fallback.profile))throw std::runtime_error("unknown default profile");
 for(const auto& v:s.vehicles)if(!s.profiles.count(v.second.profile))throw std::runtime_error("unknown profile for "+v.first);
 if(vj.contains("hud")){
  const auto& h=vj["hud"];keys(h,{"enabled",MCD_HUD_FIELDS(MCD_KEY)});s.hud.enabled=boolean(h,"enabled",true);
#define MCD_READ_HUD(n,v,l,u) s.hud.n=number(h,#n,s.hud.n,l,u);
  MCD_HUD_FIELDS(MCD_READ_HUD)
#undef MCD_READ_HUD
 }
#undef MCD_KEY
 return s;
}
}
