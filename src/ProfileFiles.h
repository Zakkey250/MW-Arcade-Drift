#pragma once
#include "Settings.h"
#include <filesystem>
#include <fstream>
namespace mcd {
namespace fs=std::filesystem;
struct Profile {bool enabled=true;DriftTuning drift;CameraTuning camera;};
struct Preferences {std::string language="auto";HudTuning hud;};
inline Json readProfileJson(const fs::path& p){
 if(fs::file_size(p)>65536)throw std::runtime_error("profile exceeds 64 KiB");
 std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("cannot open profile");return Json::parse(f);
}
inline void profileVersion(const Json& j){if(!j.is_object()||!j.contains("schemaVersion")||!j["schemaVersion"].is_number_integer()||j["schemaVersion"]!=2)throw std::runtime_error("schemaVersion must be 2");}
inline Profile parseProfile(Json d,Json c){
 profileVersion(d);profileVersion(c);Profile p;p.enabled=boolean(d,"enabled",true);
 d.erase("schemaVersion");d.erase("enabled");p.drift=drift(d);
#define MCD_PKEY(n,v,l,h) #n,
 keys(c,{"schemaVersion","enabled",MCD_CAMERA_FIELDS(MCD_PKEY)});p.camera.enabled=boolean(c,"enabled",true);
#define MCD_PCAM(n,v,l,h) p.camera.n=number(c,#n,p.camera.n,l,h);
 MCD_CAMERA_FIELDS(MCD_PCAM)
#undef MCD_PCAM
#undef MCD_PKEY
 return p;
}
inline Preferences readPreferences(const fs::path& root){
 Preferences p;if(!fs::exists(root/L"settings.json"))return p;
 auto j=readProfileJson(root/L"settings.json");profileVersion(j);keys(j,{"schemaVersion","language","hud"});
 if(j.contains("language")){if(!j["language"].is_string())throw std::runtime_error("language must be auto, ja or en");p.language=j["language"].get<std::string>();}
 if(p.language!="auto"&&p.language!="ja"&&p.language!="en")throw std::runtime_error("language must be auto, ja or en");
 if(j.contains("hud")){auto& h=j["hud"];
#define MCD_HKEY(n,v,l,u) #n,
  keys(h,{"enabled",MCD_HUD_FIELDS(MCD_HKEY)});p.hud.enabled=boolean(h,"enabled",true);
#define MCD_HVAL(n,v,l,u) p.hud.n=number(h,#n,p.hud.n,l,u);
  MCD_HUD_FIELDS(MCD_HVAL)
#undef MCD_HKEY
#undef MCD_HVAL
 }return p;
}
inline fs::path vehicleFolder(const fs::path& root,const std::string& model){
 auto name=vehicleName(model);
 if(name=="CON"||name=="PRN"||name=="AUX"||name=="NUL"||name=="CLOCK$"||
  (name.size()==4&&(name.substr(0,3)=="COM"||name.substr(0,3)=="LPT")&&name[3]>='0'&&name[3]<='9'))throw std::runtime_error("reserved vehicle directory");
 auto p=root/L"vehicles"/name;
 auto rel=fs::relative(fs::weakly_canonical(p),fs::weakly_canonical(root));
 if(rel.empty()||*rel.begin()==".."||rel.is_absolute())throw std::runtime_error("vehicle directory escapes profile root");return p;
}
inline Profile loadVehicleProfile(const fs::path& root,const std::string& model,bool create){
 auto folder=vehicleFolder(root,model);
 if(create){
  // Validate both defaults before creating anything; never overwrite user profiles.
  if(!fs::exists(folder/L"drift.json")||!fs::exists(folder/L"camera.json")){
   parseProfile(readProfileJson(root/L"default_drift.json"),readProfileJson(root/L"default_camera.json"));
   fs::create_directories(folder);
   fs::copy_file(root/L"default_drift.json",folder/L"drift.json",fs::copy_options::skip_existing);
   fs::copy_file(root/L"default_camera.json",folder/L"camera.json",fs::copy_options::skip_existing);
  }
 }
 for(auto file:{L"drift.json",L"camera.json"})if(fs::is_symlink(folder/file))throw std::runtime_error("linked profile files are unsupported");
 return parseProfile(readProfileJson(folder/L"drift.json"),readProfileJson(folder/L"camera.json"));
}
}
