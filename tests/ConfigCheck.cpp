#include "../src/ProfileFiles.h"
#include <cstdio>
int wmain(int argc,wchar_t** argv){
 if(argc!=2){puts("Usage: ConfigCheck <configuration-directory>");return 2;}
 try{
  const mcd::fs::path root=argv[1];unsigned count=0;
  if(mcd::fs::exists(root/L"default_drift.json")){
   mcd::parseProfile(mcd::readProfileJson(root/L"default_drift.json"),mcd::readProfileJson(root/L"default_camera.json"));
   mcd::readPreferences(root);
   if(mcd::fs::exists(root/L"vehicles"))for(auto& d:mcd::fs::directory_iterator(root/L"vehicles")){
    if(!d.is_directory())continue;
    mcd::loadVehicleProfile(root,d.path().filename().string(),false);++count;
   }
  }else mcd::parseProfile(mcd::readProfileJson(root/L"drift.json"),mcd::readProfileJson(root/L"camera.json"));
  printf("PASS schema=2 vehicle-pairs=%u\n",count);return 0;
 }catch(const std::exception& e){printf("REJECTED %s\n",e.what());return 1;}
}
