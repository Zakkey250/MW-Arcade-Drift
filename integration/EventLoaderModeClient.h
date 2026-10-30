#pragma once
#include "../src/DriftModeAPI.h"
// Optional consumer example, not installed into Event Loader by this change.
// Call Begin/Update/End on the event lifecycle thread, never from DllMain.
// End on race stop/world teardown. Also include the host/world generation in
// the nonzero owner value; do not reuse an owner for overlapping events.
class EventLoaderSpecialModeLease {
 MWArcadeDriftGetModeV1Fn get=nullptr;
 MWArcadeDriftAcquireSpecialV1Fn acquire=nullptr;
 MWArcadeDriftRenewSpecialV1Fn renew=nullptr;
 MWArcadeDriftReleaseSpecialV1Fn release=nullptr;
 std::uint64_t owner=0,token=0,refreshAt=0;
public:
 enum class Status { Lost,Pending,Applied };
 EventLoaderSpecialModeLease()=default;
 EventLoaderSpecialModeLease(const EventLoaderSpecialModeLease&)=delete;
 EventLoaderSpecialModeLease& operator=(const EventLoaderSpecialModeLease&)=delete;
 bool Begin(std::uint64_t eventOwner){
  if(token||!eventOwner)return false;
  auto module=GetModuleHandleW(L"MWArcadeDrift.asi");if(!module)return false;
#pragma warning(push)
#pragma warning(disable:4191)
  get=reinterpret_cast<MWArcadeDriftGetModeV1Fn>(GetProcAddress(module,"MWArcadeDriftGetModeV1"));
  acquire=reinterpret_cast<MWArcadeDriftAcquireSpecialV1Fn>(GetProcAddress(module,"MWArcadeDriftAcquireSpecialV1"));
  renew=reinterpret_cast<MWArcadeDriftRenewSpecialV1Fn>(GetProcAddress(module,"MWArcadeDriftRenewSpecialV1"));
  release=reinterpret_cast<MWArcadeDriftReleaseSpecialV1Fn>(GetProcAddress(module,"MWArcadeDriftReleaseSpecialV1"));
#pragma warning(pop)
  if(!get||!acquire||!renew||!release)return false;
  MWArcadeDriftModeV1 state{};state.size=sizeof(state);state.version=1;
  if(!get(&state)||!state.ready)return false;
  std::uint64_t acquired=0;if(!acquire(eventOwner,1500,&acquired))return false;
  owner=eventOwner;token=acquired;refreshAt=GetTickCount64()+500;return true;
 }
 Status Update(){
  if(!token)return Status::Lost;const auto now=GetTickCount64();
  if(now>=refreshAt){if(!renew(owner,token,1500)){End();return Status::Lost;}refreshAt=now+500;}
  MWArcadeDriftModeV1 state{};state.size=sizeof(state);state.version=1;
  if(!get(&state)||!state.ready||!state.forced||state.owner!=owner||state.token!=token){End();return Status::Lost;}
  return state.effectiveMode==2&&state.appliedMode==2&&state.vehicleReady?Status::Applied:Status::Pending;
 }
 void End(){if(token&&release)release(owner,token);owner=token=refreshAt=0;}
};
