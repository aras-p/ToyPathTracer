#pragma once

#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_1.h>

namespace d3d11
{
    HRESULT Init(HWND wnd);
    void Shutdown();

    extern ID3D11Device* device;
    extern ID3D11DeviceContext* ctx;
    extern IDXGISwapChain* swapChain;
    extern ID3D11RenderTargetView* renderTarget;

    extern ID3D11SamplerState* samplerLinear;
    extern ID3D11RasterizerState* rasterSolidNoCull;
    extern ID3D11BlendState* blendOff;
    extern ID3D11BlendState* blendAdditive;

    struct Texture
    {
        enum
        {
            kFlagDynamic = (1 << 0),
            kFlagUAV = (1 << 1),
            kFlagRenderTarget = (1 << 2),
        };
        Texture(int width, int height, DXGI_FORMAT format, int flags);
        ~Texture();
        ID3D11Texture2D* tex;
        ID3D11ShaderResourceView* srv;
        ID3D11RenderTargetView* rtv;
        ID3D11UnorderedAccessView* uav;
    };

    struct Buffer
    {
        enum Type
        {
            kTypeStructured,
            kTypeRaw,
        };
        enum Flags
        {
            kFlagUAV = (1<<0),
            kFlagCPUReadable = (1<<1),
            kFlagIndirectArgs = (1<<2),
        };
        Buffer(Type type, uint32_t elemCount, uint32_t elemSize, int flags);
        ~Buffer();

        ID3D11Buffer* buffer;
        ID3D11ShaderResourceView* srv;
        ID3D11UnorderedAccessView* uav;
    };
};

#define SAFE_RELEASE(x) do { if (x) (x)->Release(); } while(0)
