#include "platform.h"
#include "export.h"
#include <windows.h>
#include <d3d11.h>

namespace seedlens {
Platform make_windows_platform(void* native_window,void* native_device){
    auto window=static_cast<HWND>(native_window);
    auto device=static_cast<ID3D11Device*>(native_device);
    Platform p;
    p.wake=[window]{PostMessageW(window,WM_APP+1,0,0);};
    p.create_texture=[device](const uint32_t* pixels,int w,int h)->TextureHandle{
        D3D11_TEXTURE2D_DESC desc{};desc.Width=UINT(w);desc.Height=UINT(h);
        desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels;data.SysMemPitch=UINT(w*4);
        ID3D11Texture2D* texture=nullptr;ID3D11ShaderResourceView* view=nullptr;
        if(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture))){
            device->CreateShaderResourceView(texture,nullptr,&view);texture->Release();
        }
        return reinterpret_cast<TextureHandle>(view);
    };
    p.release_texture=[](TextureHandle handle){if(handle)reinterpret_cast<ID3D11ShaderResourceView*>(handle)->Release();};
    p.image_resource=[](int id){ImageData image;read_png_resource(id,image.pixels,image.width,image.height);return image;};
    p.resource_data=[](int id){
        std::vector<uint8_t> bytes;auto module=GetModuleHandleW(nullptr);
        auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
        if(resource){auto data=LoadResource(module,resource);if(data){auto* start=static_cast<const uint8_t*>(LockResource(data));bytes.assign(start,start+SizeofResource(module,resource));}}
        return bytes;
    };
    p.choose_save=[window](const wchar_t* ext,const std::wstring& name){return save_dialog(window,ext,name);};
    return p;
}
}
