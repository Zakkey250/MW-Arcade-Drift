#pragma once
#include "ProfileFiles.h"
#include "SpecialTuning.h"
namespace mcd {
struct SpecialProfile {bool enabled=true;SpecialTuning drift;CameraTuning camera;};
inline SpecialProfile parseSpecialProfile(Json d,Json c){
 if(!d.is_object()||!d.contains("schemaVersion")||!d["schemaVersion"].is_number_integer()||d["schemaVersion"]!=1)throw std::runtime_error("special schemaVersion must be 1");
#define MCD_SPECIAL_KEY(n,v,l,h) #n,
 keys(d,{"schemaVersion","enabled",MCD_SPECIAL_FIELDS(MCD_SPECIAL_KEY)});
#undef MCD_SPECIAL_KEY
 SpecialProfile p;p.enabled=boolean(d,"enabled",true);
#define MCD_SPECIAL_READ(n,v,l,h) p.drift.n=number(d,#n,p.drift.n,l,h);
 MCD_SPECIAL_FIELDS(MCD_SPECIAL_READ)
#undef MCD_SPECIAL_READ
 if(p.drift.exitSpeedMps>=p.drift.entrySpeedMps)throw std::runtime_error("special exit speed must be below entry speed");
 if(p.drift.baseSlipRad+p.drift.slideSlipRad+p.drift.handbrakeSlipRad>.65f)throw std::runtime_error("special slip target exceeds 0.65 radians");
 // Reuse the strict existing camera validator, not special keys in schema 2.
 p.camera=parseProfile(Json{{"schemaVersion",2}},c).camera;return p;
}
inline SpecialProfile loadSpecialVehicleProfile(const fs::path& root,const std::string& model,bool create){
 auto folder=vehicleFolder(root,model);
 if(create&&(!fs::exists(folder/L"drift.json")||!fs::exists(folder/L"camera.json"))){
  parseSpecialProfile(readProfileJson(root/L"default_drift.json"),readProfileJson(root/L"default_camera.json"));
  fs::create_directories(folder);
  fs::copy_file(root/L"default_drift.json",folder/L"drift.json",fs::copy_options::skip_existing);
  fs::copy_file(root/L"default_camera.json",folder/L"camera.json",fs::copy_options::skip_existing);
 }
 for(auto file:{L"drift.json",L"camera.json"})if(fs::is_symlink(folder/file))throw std::runtime_error("linked special profile unsupported");
 return parseSpecialProfile(readProfileJson(folder/L"drift.json"),readProfileJson(folder/L"camera.json"));
}
}
