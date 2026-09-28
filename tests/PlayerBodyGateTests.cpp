#include "../src/PlayerBodyGate.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void Check(bool ok){if(!ok){std::puts("FAIL player body gate");std::exit(1);}}
int main(){
 static_assert(sizeof(void*)==4,"Win32 test required");
 auto* primary=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x200,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 auto* owner=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x200,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 Check(primary&&owner);
 auto word=[](unsigned char* base,unsigned offset)->std::uint32_t&{return *reinterpret_cast<std::uint32_t*>(base+offset);};
 word(primary,0x34)=reinterpret_cast<std::uint32_t>(owner);
 word(owner,0x80)=0x8aa828;word(owner,0x114)=0;
 const auto address=reinterpret_cast<std::uint32_t>(primary);
 Check(mcd::playerBodySafe(address)&&mcd::playerBody(address));
 word(owner,0x114)=2;Check(!mcd::playerBodySafe(address)&&!mcd::playerBody(address));
 word(owner,0x114)=0;word(owner,0x80)=0x12345678;Check(!mcd::playerBodySafe(address)&&!mcd::playerBody(address));
 word(owner,0x80)=0x8aa828;word(primary,0x34)=0x12345;
 Check(!mcd::playerBodySafe(address)&&!mcd::playerBody(address));
 Check(!mcd::playerBody(0x12345));
 word(primary,0x34)=reinterpret_cast<std::uint32_t>(owner);
 LARGE_INTEGER frequency{},begin{},end{};QueryPerformanceFrequency(&frequency);
 constexpr unsigned iterations=100000;
 volatile unsigned hits=0;
 QueryPerformanceCounter(&begin);
 for(unsigned i=0;i<iterations;++i)hits+=unsigned(mcd::playerBodySafe(address));
 QueryPerformanceCounter(&end);
 const double safeMs=1000.0*double(end.QuadPart-begin.QuadPart)/double(frequency.QuadPart);
 QueryPerformanceCounter(&begin);
 for(unsigned i=0;i<iterations;++i)hits+=unsigned(mcd::playerBody(address));
 QueryPerformanceCounter(&end);
 const double fastMs=1000.0*double(end.QuadPart-begin.QuadPart)/double(frequency.QuadPart);
 Check(hits==iterations*2);
 VirtualFree(primary,0,MEM_RELEASE);VirtualFree(owner,0,MEM_RELEASE);
 std::puts("PASS player/nonplayer/invalid-pointer gate parity");
 std::printf("BENCH isolated player gate iterations=%u safe_ms=%.2f fast_ms=%.2f\n",iterations,safeMs,fastMs);
}
