#include "../src/SpecialProfile.h"
#include <cstdio>
unsigned Validate(const mcd::fs::path& root,bool special){
 unsigned count=0;auto parse=[&](const mcd::fs::path& d,const mcd::fs::path& c){if(special)mcd::parseSpecialProfile(mcd::readProfileJson(d),mcd::readProfileJson(c));else mcd::parseProfile(mcd::readProfileJson(d),mcd::readProfileJson(c));};
 if(mcd::fs::exists(root/L"default_drift.json")){
  parse(root/L"default_drift.json",root/L"default_camera.json");if(!special)mcd::readPreferences(root);
  if(mcd::fs::exists(root/L"vehicles"))for(auto& d:mcd::fs::directory_iterator(root/L"vehicles")){
   if(!d.is_directory())continue;if(special)mcd::loadSpecialVehicleProfile(root,d.path().filename().string(),false);else mcd::loadVehicleProfile(root,d.path().filename().string(),false);++count;
  }
 }else parse(root/L"drift.json",root/L"camera.json");return count;
}
int wmain(int argc,wchar_t** argv){
 bool special=argc==3&&wcscmp(argv[1],L"--special")==0;
 if(argc!=2&&!special){puts("Usage: ConfigCheck [--special] <configuration-directory>");return 2;}
 try{
  mcd::fs::path root=argv[special?2:1];auto count=Validate(root,special);
  unsigned sc=0;if(!special&&mcd::fs::exists(root/L"modes"/L"special"/L"default_drift.json"))sc=Validate(root/L"modes"/L"special",true);
  printf("PASS schema=%u vehicle-pairs=%u special-pairs=%u\n",special?1u:2u,count,sc);return 0;
 }catch(const std::exception& e){printf("REJECTED %s\n",e.what());return 1;}
}
