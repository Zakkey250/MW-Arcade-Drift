#pragma once
#include "DrivingMode.h"
#include "DriftModeAPI.h"
namespace mcd {
class ModeHost {
 SRWLOCK lock=SRWLOCK_INIT;
 ModeSelection selection;
 bool ready=false,vehicleReady=false;
 DrivingMode applied=DrivingMode::Normal;
 std::uint64_t sampled=0;
public:
 void initialize(DrivingMode initial){AcquireSRWLockExclusive(&lock);selection.manual=initial;ReleaseSRWLockExclusive(&lock);}
 void activate(){AcquireSRWLockExclusive(&lock);ready=true;ReleaseSRWLockExclusive(&lock);}
 void publish(std::uint64_t now,bool valid,DrivingMode mode){AcquireSRWLockExclusive(&lock);sampled=now;vehicleReady=valid;applied=mode;ReleaseSRWLockExclusive(&lock);}
 DrivingMode mode(std::uint64_t now){AcquireSRWLockExclusive(&lock);auto result=selection.effective(now);ReleaseSRWLockExclusive(&lock);return result;}
 bool cycle(std::uint64_t now){AcquireSRWLockExclusive(&lock);bool changed=selection.cycle(now);ReleaseSRWLockExclusive(&lock);return changed;}
 MWArcadeDriftModeV1 state(std::uint64_t now){
  AcquireSRWLockExclusive(&lock);auto effective=selection.effective(now);
  MWArcadeDriftModeV1 result{sizeof(MWArcadeDriftModeV1),1,std::uint32_t(selection.manual),std::uint32_t(effective),selection.token?1u:0u,ready?1u:0u,
   vehicleReady&&applied==effective&&sampled&&now>=sampled&&now-sampled<=150?1u:0u,std::uint32_t(applied),now,selection.owner,selection.token,selection.expires};
  ReleaseSRWLockExclusive(&lock);return result;
 }
 std::uint64_t acquire(std::uint64_t owner,std::uint64_t now,std::uint32_t ttl){AcquireSRWLockExclusive(&lock);auto token=ready?selection.acquire(owner,now,ttl):0;ReleaseSRWLockExclusive(&lock);return token;}
 bool renew(std::uint64_t owner,std::uint64_t token,std::uint64_t now,std::uint32_t ttl){AcquireSRWLockExclusive(&lock);bool ok=ready&&selection.renew(owner,token,now,ttl);ReleaseSRWLockExclusive(&lock);return ok;}
 bool release(std::uint64_t owner,std::uint64_t token,std::uint64_t now){AcquireSRWLockExclusive(&lock);bool ok=selection.release(owner,token,now);ReleaseSRWLockExclusive(&lock);return ok;}
};
}
