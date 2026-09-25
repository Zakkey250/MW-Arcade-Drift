#include <windows.h>
#include <wincodec.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/DriftHud.h"
unsigned checks=0;
void Check(bool b,const char* s){++checks;if(!b){printf("FAIL %s\n",s);exit(1);}}
template<class T>void Release(T*& p){if(p){p->Release();p=nullptr;}}
bool Save(IDirect3DDevice9* d,const wchar_t* path){
    IDirect3DSurface9* back=nullptr;IDirect3DSurface9* copy=nullptr;
    Check(SUCCEEDED(d->GetRenderTarget(0,&back)),"render target");D3DSURFACE_DESC desc{};back->GetDesc(&desc);
    Check(SUCCEEDED(d->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr)),"readback allocation");
    Check(SUCCEEDED(d->GetRenderTargetData(back,copy)),"readback");
    D3DLOCKED_RECT lock{};Check(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY)),"readback lock");
    IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(path,GENERIC_WRITE);
    if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);
    if(SUCCEEDED(hr))hr=encoder->Initialize(stream,WICBitmapEncoderNoCache);
    if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(&frame,nullptr);
    if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);
    if(SUCCEEDED(hr))hr=frame->SetSize(desc.Width,desc.Height);
    WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;
    if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);
    std::vector<BYTE> rgb(desc.Width*desc.Height*3);
    for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x)
        std::memcpy(rgb.data()+(y*desc.Width+x)*3,static_cast<BYTE*>(lock.pBits)+y*lock.Pitch+x*4,3);
    if(SUCCEEDED(hr))hr=frame->WritePixels(desc.Height,desc.Width*3,static_cast<UINT>(rgb.size()),rgb.data());
    if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();
    Release(frame);Release(encoder);Release(stream);Release(factory);copy->UnlockRect();Release(copy);Release(back);return SUCCEEDED(hr);
}

int wmain(int argc,wchar_t** argv){
 Check(argc==2,"output directory argument");CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 HWND w=CreateWindowW(L"STATIC",L"MW Arcade Drift isolated HUD test",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Check(w!=nullptr,"hidden window");
 auto api=Direct3DCreate9(D3D_SDK_VERSION);Check(api!=nullptr,"D3D9");
 IDirect3DDevice9* d=nullptr;D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=w;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferWidth=1920;pp.BackBufferHeight=1080;
 Check(SUCCEEDED(api->CreateDevice(0,D3DDEVTYPE_HAL,w,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&d)),"device creation");
 for(auto dim:{std::pair<UINT,UINT>{1920,1080},{3840,1600},{5120,1440}}){
  pp.BackBufferWidth=dim.first;pp.BackBufferHeight=dim.second;Check(SUCCEEDED(d->Reset(&pp)),"reset without retained resources");
  for(unsigned active:{1u,2u,3u}){
   mcd::HudTuning cfg;auto layout=mcd::hudLayout(float(dim.first),float(dim.second),cfg);
   Check(layout.x>=0&&layout.y>=0&&layout.x+layout.size<=dim.first&&layout.y+layout.size<=dim.second,"icon fits aspect");
   d->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(20,25,32),1,0);Check(SUCCEEDED(d->BeginScene()),"begin scene");
   IDirect3DVertexBuffer9* vb=nullptr;d->CreateVertexBuffer(256,0,D3DFVF_XYZ,D3DPOOL_MANAGED,&vb,nullptr);d->SetStreamSource(0,vb,0,12);
   d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_SCISSORTESTENABLE,TRUE);d->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_TEMP);
   Check(mcd::drawDriftHud(d,cfg,active),"icon rendered");DWORD n=0;d->GetRenderState(D3DRS_ALPHABLENDENABLE,&n);Check(n==FALSE,"blend restored");d->GetRenderState(D3DRS_SCISSORTESTENABLE,&n);Check(n==TRUE,"scissor restored");d->GetTextureStageState(0,D3DTSS_RESULTARG,&n);Check(n==D3DTA_TEMP,"stage destination restored");
   IDirect3DVertexBuffer9* got=nullptr;UINT offset=0,stride=0;d->GetStreamSource(0,&got,&offset,&stride);Check(got==vb&&stride==12,"stream restored");Release(got);d->SetStreamSource(0,nullptr,0,0);Release(vb);
   cfg.enabled=false;Check(!mcd::drawDriftHud(d,cfg,active),"disabled icon not rendered");
   d->EndScene();wchar_t path[MAX_PATH];swprintf_s(path,L"%s/hud-%ux%u-%u.png",argv[1],dim.first,dim.second,unsigned(active));Check(Save(d,path),"PNG saved");
  }
 }
 // Larger diagnostic view of the exact same geometry for visual review.
 pp.BackBufferWidth=512;pp.BackBufferHeight=256;Check(SUCCEEDED(d->Reset(&pp)),"preview reset");d->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(20,25,32),1,0);d->BeginScene();mcd::HudTuning c;c.size=400;c.right=100;c.bottom=100;mcd::drawDriftHud(d,c,2);d->EndScene();wchar_t path[MAX_PATH];swprintf_s(path,L"%s/icon-detail.png",argv[1]);Check(Save(d,path),"detail saved");
 Release(d);Release(api);DestroyWindow(w);CoUninitialize();printf("PASS %u isolated HUD checks; live game acceptance pending\n",checks);
}
