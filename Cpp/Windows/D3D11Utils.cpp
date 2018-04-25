#include "D3D11Utils.h"
#include <assert.h>

static D3D_FEATURE_LEVEL s_FeatureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* d3d11::device = nullptr;
ID3D11DeviceContext* d3d11::ctx = nullptr;
IDXGISwapChain* d3d11::swapChain = nullptr;
ID3D11RenderTargetView* d3d11::renderTarget = nullptr;
ID3D11SamplerState* d3d11::samplerLinear = nullptr;
ID3D11RasterizerState* d3d11::rasterSolidNoCull = nullptr;
ID3D11BlendState* d3d11::blendOff = nullptr;
ID3D11BlendState* d3d11::blendAdditive = nullptr;


HRESULT d3d11::Init(HWND wnd)
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(wnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &d3d11::device, &s_FeatureLevel, &d3d11::ctx);
    if (FAILED(hr))
        return hr;

    // Get DXGI factory
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = d3d11::device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr))
            {
                hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = wnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    hr = dxgiFactory->CreateSwapChain(d3d11::device, &sd, &swapChain);

    // Prevent Alt-Enter
    dxgiFactory->MakeWindowAssociation(wnd, DXGI_MWA_NO_ALT_ENTER);
    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    // RTV
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr))
        return hr;
    hr = d3d11::device->CreateRenderTargetView(pBackBuffer, nullptr, &d3d11::renderTarget);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    // Commonly used states
    D3D11_SAMPLER_DESC smpDesc = {};
    smpDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    smpDesc.AddressU = smpDesc.AddressV = smpDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    d3d11::device->CreateSamplerState(&smpDesc, &d3d11::samplerLinear);

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    d3d11::device->CreateRasterizerState(&rasterDesc, &d3d11::rasterSolidNoCull);

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
    d3d11::device->CreateBlendState(&blendDesc, &blendOff);

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
    d3d11::device->CreateBlendState(&blendDesc, &blendAdditive);

    // RT and viewport
    d3d11::ctx->OMSetRenderTargets(1, &d3d11::renderTarget, nullptr);
    D3D11_VIEWPORT vp;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    d3d11::ctx->RSSetViewports(1, &vp);

    return S_OK;
}


void d3d11::Shutdown()
{
    if (d3d11::ctx) d3d11::ctx->ClearState();

    SAFE_RELEASE(d3d11::blendOff);
    SAFE_RELEASE(d3d11::blendAdditive);
    SAFE_RELEASE(d3d11::rasterSolidNoCull);
    SAFE_RELEASE(d3d11::samplerLinear);
    SAFE_RELEASE(d3d11::renderTarget);
    SAFE_RELEASE(d3d11::swapChain);
    SAFE_RELEASE(d3d11::ctx);
    SAFE_RELEASE(d3d11::device);
}


d3d11::Texture::Texture(int width, int height, DXGI_FORMAT format, int flags)
    : tex(NULL), srv(NULL), rtv(NULL), uav(NULL)
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = (flags & kFlagDynamic) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (flags & kFlagUAV)
        texDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    if (flags & kFlagRenderTarget)
        texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = (flags & kFlagDynamic) ? D3D11_CPU_ACCESS_WRITE : 0;
    texDesc.MiscFlags = 0;
    device->CreateTexture2D(&texDesc, NULL, &tex);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    device->CreateShaderResourceView(tex, &srvDesc, &srv);

    if (flags & kFlagUAV)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        device->CreateUnorderedAccessView(tex, &uavDesc, &uav);
    }

    if (flags & kFlagRenderTarget)
    {
        device->CreateRenderTargetView(tex, nullptr, &rtv);
    }
}

d3d11::Texture::~Texture()
{
    SAFE_RELEASE(uav);
    SAFE_RELEASE(srv);
    SAFE_RELEASE(rtv);
    SAFE_RELEASE(tex);
}

d3d11::Buffer::Buffer(Type type, uint32_t elemCount, uint32_t elemSize, int flags)
    : buffer(NULL), srv(NULL), uav(NULL)
{
    D3D11_BUFFER_DESC bdesc = {};
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    bdesc.ByteWidth = elemCount * elemSize;
    bdesc.Usage = D3D11_USAGE_DEFAULT;
    bdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bdesc.CPUAccessFlags = (flags & kFlagCPUReadable) ? D3D11_CPU_ACCESS_READ : 0;
    bdesc.MiscFlags = 0;
    switch (type)
    {
    case kTypeStructured:
        bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = elemCount;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.Flags = 0;
        break;
    case kTypeRaw:
        bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.BufferEx.FirstElement = 0;
        srvDesc.BufferEx.NumElements = elemCount;
        srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        break;
    default: assert(false);
    }
    if (flags & kFlagUAV)
        bdesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    if (flags & kFlagIndirectArgs)
    {
        bdesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    bdesc.StructureByteStride = elemSize;
    d3d11::device->CreateBuffer(&bdesc, NULL, &buffer);

    d3d11::device->CreateShaderResourceView(buffer, &srvDesc, &srv);

    if (flags & kFlagUAV)
    {
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = elemCount;
        d3d11::device->CreateUnorderedAccessView(buffer, &uavDesc, &uav);
    }
}

d3d11::Buffer::~Buffer()
{
    SAFE_RELEASE(srv);
    SAFE_RELEASE(uav);
    SAFE_RELEASE(buffer);
}

