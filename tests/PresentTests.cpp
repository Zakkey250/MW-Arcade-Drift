#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include "../src/PresentHud.h"
#include "../src/PresentHooks.h"
unsigned checks=0,calls=0,draws=0;
void Check(bool value,const char* label){++checks;if(!value){printf("FAIL %s\n",label);exit(1);}}
void Draw(IDirect3DDevice9* d){++calls;if(mcd::drawPresentHud(d,mcd::HudTuning{},1))++draws;}
template<class T>void Release(T*& p){if(p){p->Release();p=nullptr;}}
int main(){
 HWND window=CreateWindowW(L"STATIC",L"MW Arcade Drift Present test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 auto api=Direct3DCreate9(D3D_SDK_VERSION);Check(api&&window,"D3D/window");
 IDirect3DDevice9* d=nullptr;D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferWidth=1920;pp.BackBufferHeight=1080;
 Check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&d)),"real D3D device");
 Check(MH_Initialize()==MH_OK,"MinHook initialize");
 auto target=(*reinterpret_cast<void***>(d))[17];
 Check(mcd::PresentHooks::install(target,Draw)==MH_OK,"hook real Present implementation");
 Check(mcd::PresentHooks::install(target,Draw)==MH_OK,"same target idempotent");
 for(auto width:{1920u,3840u}){
  pp.BackBufferWidth=width;pp.BackBufferHeight=width==1920?1080:1600;
  Check(SUCCEEDED(d->Reset(&pp)),"reset while Present hooked");
  IDirect3DSurface9* off=nullptr;IDirect3DSurface9* back=nullptr;IDirect3DSurface9* depth=nullptr;
  Check(SUCCEEDED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)),"backbuffer");
  Check(SUCCEEDED(d->CreateRenderTarget(128,128,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&off,nullptr)),"offscreen target");
  Check(SUCCEEDED(d->CreateDepthStencilSurface(128,128,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,&depth,nullptr)),"depth target");
  d->SetRenderTarget(0,back);d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
  d->SetRenderTarget(0,off);d->SetDepthStencilSurface(depth);
  const D3DVIEWPORT9 vp{3,4,80,90,.2f,.8f};d->SetViewport(&vp);
  const RECT clip{5,6,75,80};d->SetScissorRect(&clip);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
  const auto before=calls;
  Check(SUCCEEDED(d->Present(nullptr,nullptr,nullptr,nullptr)),"forward original Present HRESULT");
  Check(calls==before+1&&draws==calls,"one successful HUD callback per Present");
  IDirect3DSurface9* got=nullptr;d->GetRenderTarget(0,&got);Check(got==off,"offscreen RT restored");Release(got);
  d->GetDepthStencilSurface(&got);Check(got==depth,"depth restored");Release(got);
  D3DVIEWPORT9 current{};d->GetViewport(&current);Check(!memcmp(&current,&vp,sizeof(vp)),"viewport restored");
  DWORD blend=1;d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);Check(blend==FALSE,"render state restored");
  RECT currentClip{};d->GetScissorRect(&currentClip);Check(EqualRect(&clip,&currentClip),"scissor restored");
  // Pixel assertion before presentation (DISCARD makes post-Present contents undefined).
  d->SetRenderTarget(0,back);d->SetDepthStencilSurface(nullptr);d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
  Check(mcd::drawPresentHud(d,mcd::HudTuning{},1),"draw before readback");
  IDirect3DSurface9* copy=nullptr;Check(SUCCEEDED(d->CreateOffscreenPlainSurface(pp.BackBufferWidth,pp.BackBufferHeight,pp.BackBufferFormat,D3DPOOL_SYSTEMMEM,&copy,nullptr)),"readback allocation");
  Check(SUCCEEDED(d->GetRenderTargetData(back,copy)),"backbuffer readback");D3DLOCKED_RECT lock{};Check(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"readback lock");
  unsigned green=0;
  for(unsigned y=0;y<pp.BackBufferHeight;y++)for(unsigned x=0;x<pp.BackBufferWidth;x++){
   auto pixel=reinterpret_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4;
   if(pixel[1]>100&&pixel[1]>pixel[0]&&pixel[1]>pixel[2])++green;
  }
  Check(green>100,"green icon pixels present on backbuffer");copy->UnlockRect();Release(copy);
  Check(SUCCEEDED(d->BeginScene()),"native scene begins");
  Check(!mcd::drawPresentHud(d,mcd::HudTuning{},1),"reject nested scene without ending native scene");
  Check(SUCCEEDED(d->EndScene()),"native scene remains owned by caller");
  Release(off);Release(depth);Release(back);
 }
 // Device replacement must work when the implementation is shared.
 Release(d);Check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&d)),"replacement device");
 Check(mcd::PresentHooks::install((*reinterpret_cast<void***>(d))[17],Draw)==MH_OK,"replacement binding");
 auto before=calls;Check(SUCCEEDED(d->Present(nullptr,nullptr,nullptr,nullptr))&&calls==before+1,"replacement callback");
 MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();Release(d);Release(api);DestroyWindow(window);
 printf("PASS %u Present/backbuffer/state/reset checks; game acceptance pending\n",checks);
}
