#include "../src/ProfileFiles.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int wmain(int argc,wchar_t** argv){
 try{
  check(argc==2,"Usage: CorePackageTests <extracted-core-root>");
  const mcd::fs::path package=argv[1],root=package/L"scripts"/L"MWArcadeDrift";
  for(auto& entry:mcd::fs::recursive_directory_iterator(package))if(entry.is_regular_file())check(entry.path().extension()!=L".exe","core fixture contains an EXE");
  check(!mcd::fs::exists(root/L"fields.json"),"editor metadata present in core");
  auto baseline=mcd::parseProfile(mcd::readProfileJson(root/L"default_drift.json"),mcd::readProfileJson(root/L"default_camera.json"));
  auto preferences=mcd::readPreferences(root);check(preferences.language=="auto","default language");
  auto expected=mcd::fs::exists(root/L"vehicles"/L"PACKAGE_TEST"/L"drift.json")?mcd::loadVehicleProfile(root,"PACKAGE_TEST",false):baseline;
  auto created=mcd::loadVehicleProfile(root,"PACKAGE_TEST",true);
  check(created.drift.targetYawMax==expected.drift.targetYawMax&&created.camera.orbitGain==expected.camera.orbitGain,"entry copied baseline");
  auto path=root/L"vehicles"/L"PACKAGE_TEST";
  auto d=mcd::readProfileJson(path/L"drift.json"),c=mcd::readProfileJson(path/L"camera.json");d["enabled"]=false;d["targetYawMax"]=.65;c["enabled"]=false;
  {std::ofstream f(path/L"drift.json");f<<d.dump();}{std::ofstream f(path/L"camera.json");f<<c.dump();}
  auto reloaded=mcd::loadVehicleProfile(root,"PACKAGE_TEST",true);
  check(!reloaded.enabled&&!reloaded.camera.enabled&&reloaded.drift.targetYawMax==.65f,"manual edit reload and existing profile preservation");
  d["enabled"]=true;c["enabled"]=true;
  {std::ofstream f(path/L"drift.json");f<<d.dump();}{std::ofstream f(path/L"camera.json");f<<c.dump();}
  check(mcd::loadVehicleProfile(root,"PACKAGE_TEST",true).enabled,"manual re-enable");
  d["targetYawMax"]=99;{std::ofstream f(path/L"drift.json");f<<d.dump();}
  bool rejected=false;try{mcd::loadVehicleProfile(root,"PACKAGE_TEST",true);}catch(const std::exception&){rejected=true;}check(rejected,"invalid edit rejected inside native parser");
  d["targetYawMax"]=.65;{std::ofstream f(path/L"drift.json");f<<d.dump();}
  check(mcd::loadVehicleProfile(root,"PACKAGE_TEST",true).drift.targetYawMax==.65f,"corrected edit reload");
  puts("PASS core-only native defaults, auto-create, manual-edit reload, disable, re-enable, preserve, invalid/corrected reload; no editor EXEs present");return 0;
 }catch(const std::exception& e){printf("FAIL %s\n",e.what());return 1;}
}
