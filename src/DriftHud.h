#pragma once
#include <d3d9.h>
#include <algorithm>
#include <vector>
#include "Tuning.h"
namespace mcd {
struct HudLayout {float x,y,size;};
inline HudLayout hudLayout(float width,float height,const HudTuning& c){
 const float scale=height/1080.f,size=std::min({c.size*scale,width,height});
 return {std::clamp(width-(c.right+c.size)*scale,0.f,width-size),std::clamp(height-(c.bottom+c.size)*scale,0.f,height-size),size};
}
struct HudVertex {float x,y,z,rhw;DWORD color;};
// Original car-and-skid pictogram, no game texture/resource modifications.
inline std::vector<HudVertex> hudGeometry(HudLayout p,DWORD color){
 const char* glyph[]={
 "      ########      ","     ##########     ","    ##        ##    ","    ##        ##    ",
 "   ##############   ","   ##############   ","   ##  ####  ##     ","   ##############   ",
 "   ##############   ","    ##      ##      ","    ##      ##      ","                    ",
 "     ##      ##     ","    ##      ##      ","   ##      ##       ","    ##      ##      ",
 "     ##      ##     ","    ##      ##      ","   ##      ##       ","                    "};
 std::vector<HudVertex> v;v.reserve(2400);const float u=p.size/20;
 for(int y=0;y<20;++y)for(int x=0;x<20;++x)if(glyph[y][x]=='#'){
  float l=p.x+x*u-.5f,t=p.y+y*u-.5f,r=l+u,b=t+u;
  v.insert(v.end(),{{l,t,0,1,color},{r,t,0,1,color},{l,b,0,1,color},{l,b,0,1,color},{r,t,0,1,color},{r,b,0,1,color}});
 }return v;
}
inline bool drawDriftHud(IDirect3DDevice9* d,const HudTuning& cfg,unsigned mode){
 if(!d||!cfg.enabled||FAILED(d->TestCooperativeLevel()))return false;
 D3DVIEWPORT9 vp{};if(FAILED(d->GetViewport(&vp))||!vp.Width||!vp.Height)return false;
 auto p=hudLayout(float(vp.Width),float(vp.Height),cfg);p.x+=float(vp.X);p.y+=float(vp.Y);
 const auto color=mode==2?D3DCOLOR_ARGB(DWORD(255*cfg.opacity),255,180,40):mode==1?D3DCOLOR_ARGB(DWORD(255*cfg.opacity),65,220,100):D3DCOLOR_ARGB(DWORD(130*cfg.opacity),150,150,150);
 auto vertices=hudGeometry(p,color);
 IDirect3DStateBlock9* state=nullptr;
 if(FAILED(d->CreateStateBlock(D3DSBT_ALL,&state))||!state)return false;
 IDirect3DVertexBuffer9* stream=nullptr;UINT offset=0,stride=0;d->GetStreamSource(0,&stream,&offset,&stride);
 d->SetVertexShader(nullptr);d->SetPixelShader(nullptr);d->SetTexture(0,nullptr);
 d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
 d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
 d->SetRenderState(D3DRS_LIGHTING,FALSE);d->SetRenderState(D3DRS_FOGENABLE,FALSE);
 d->SetRenderState(D3DRS_STENCILENABLE,FALSE);d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
 d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
 d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
 d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
 d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
 d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
 d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT);
 d->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
 d->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);d->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
 d->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);d->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
 const HRESULT result=d->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),vertices.data(),sizeof(HudVertex));
 state->Apply();state->Release();d->SetStreamSource(0,stream,offset,stride);if(stream)stream->Release();return SUCCEEDED(result);
}
}
