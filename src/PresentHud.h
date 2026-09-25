#pragma once
#include "DriftHud.h"
namespace mcd {
// Present runs outside the game's scene. Keep no device resources across frames
// or Reset. Explicitly preserve targets: D3DSBT_ALL does not capture them.
struct PresentSurfaces {
 IDirect3DDevice9* d;
 IDirect3DSurface9* back=nullptr;
 IDirect3DSurface9* target[4]{};
 IDirect3DSurface9* depth=nullptr;
 D3DVIEWPORT9 viewport{};
 RECT scissor{};
 unsigned count=0;
 bool changed=false;
 explicit PresentSurfaces(IDirect3DDevice9* device):d(device){}
 ~PresentSurfaces(){
  if(changed){
   d->SetDepthStencilSurface(nullptr);
   for(unsigned i=1;i<count;i++)d->SetRenderTarget(i,nullptr);
   d->SetRenderTarget(0,target[0]);
   for(unsigned i=1;i<count;i++)d->SetRenderTarget(i,target[i]);
   d->SetDepthStencilSurface(depth);d->SetViewport(&viewport);d->SetScissorRect(&scissor);
  }
  for(auto t:target)if(t)t->Release();
  if(depth)depth->Release();if(back)back->Release();
 }
 bool selectBackbuffer(){
  D3DCAPS9 caps{};D3DSURFACE_DESC desc{};
  if(FAILED(d->GetDeviceCaps(&caps))||caps.NumSimultaneousRTs>4||!caps.NumSimultaneousRTs)return false;
  count=caps.NumSimultaneousRTs;
  if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(back->GetDesc(&desc))||FAILED(d->GetViewport(&viewport))||FAILED(d->GetScissorRect(&scissor)))return false;
  for(unsigned i=0;i<count;i++){
   const auto hr=d->GetRenderTarget(i,&target[i]);
   if(FAILED(hr)&&(i==0||hr!=D3DERR_NOTFOUND))return false;
  }
  const auto hr=d->GetDepthStencilSurface(&depth);
  if(FAILED(hr)&&hr!=D3DERR_NOTFOUND)return false;
  changed=true;
  if(FAILED(d->SetDepthStencilSurface(nullptr)))return false;
  for(unsigned i=1;i<count;i++)if(FAILED(d->SetRenderTarget(i,nullptr)))return false;
  if(FAILED(d->SetRenderTarget(0,back)))return false;
  const D3DVIEWPORT9 full{0,0,desc.Width,desc.Height,0,1};
  return SUCCEEDED(d->SetViewport(&full));
 }
};
inline bool drawPresentHud(IDirect3DDevice9* d,const HudTuning& cfg,unsigned mode){
 if(!d||!cfg.enabled||FAILED(d->TestCooperativeLevel()))return false;
 // Failure means a nested scene or lost device: leave scene ownership alone.
 if(FAILED(d->BeginScene()))return false;
 bool drawn=false;
 try {PresentSurfaces surfaces(d);if(surfaces.selectBackbuffer())drawn=drawDriftHud(d,cfg,mode);}
 catch(...){drawn=false;}
 const auto hr=d->EndScene();
 return drawn&&SUCCEEDED(hr);
}
}
