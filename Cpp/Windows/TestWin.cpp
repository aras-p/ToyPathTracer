#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_1.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>

#include "../Source/Config.h"
#include "../Source/Maths.h"
#include "../Source/Test.h"
#include "CompiledVertexShader.h"
#include "CompiledPixelShader.h"

static HINSTANCE g_HInstance;
static HWND g_Wnd;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static HRESULT InitD3DDevice();
static void ShutdownD3DDevice();
static void RenderFrame();

static float* g_Backbuffer;

static D3D_FEATURE_LEVEL g_D3D11FeatureLevel = D3D_FEATURE_LEVEL_11_0;
static ID3D11Device* g_D3D11Device = nullptr;
static ID3D11DeviceContext* g_D3D11Ctx = nullptr;
static IDXGISwapChain* g_D3D11SwapChain = nullptr;
static ID3D11RenderTargetView* g_D3D11RenderTarget = nullptr;
static ID3D11VertexShader* g_VertexShader;
static ID3D11PixelShader* g_PixelShader;
static ID3D11SamplerState* g_SamplerLinear;
static ID3D11RasterizerState* g_RasterState;
static ID3D11BlendState* g_BlendStateOff;
static ID3D11BlendState* g_BlendStateAdd;

struct D3D11Texture
{
    D3D11Texture(int width, int height, DXGI_FORMAT format);
    ~D3D11Texture();
    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* srv;
    ID3D11RenderTargetView* rtv;
    ID3D11UnorderedAccessView* uav;
};

D3D11Texture::D3D11Texture(int width, int height, DXGI_FORMAT format)
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
#if DO_COMPUTE_GPU
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
#else
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#endif
    texDesc.MiscFlags = 0;
    g_D3D11Device->CreateTexture2D(&texDesc, NULL, &tex);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    g_D3D11Device->CreateShaderResourceView(tex, &srvDesc, &srv);

#if DO_COMPUTE_GPU
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    g_D3D11Device->CreateUnorderedAccessView(tex, &uavDesc, &uav);
#else
    uav = NULL;
#endif

#if DO_COMPUTE_GPU
    g_D3D11Device->CreateRenderTargetView(tex, nullptr, &rtv);
#else
    rtv = NULL;
#endif
}

D3D11Texture::~D3D11Texture()
{
    if (uav) uav->Release();
    if (srv) srv->Release();
    if (tex) tex->Release();
    if (rtv) rtv->Release();
}

static D3D11Texture *g_BackbufferTex, *g_BackbufferTex2, *g_TmpTex;
static int g_BufferIndex;


#if DO_COMPUTE_GPU
#include "CompiledComputeShaderCameraRays.h"
#include "CompiledComputeShaderBounce.h"
#include "CompiledComputeShaderFinal.h"
#include "CompiledComputeShaderCopyCount.h"
#include "CompiledVertexShaderSplat.h"
#include "CompiledPixelShaderSplat.h"
struct ComputeParams
{
    Camera cam;
    int sphereCount;
    int screenWidth;
    int screenHeight;
    int frames;
    float invWidth;
    float invHeight;
    float lerpFac;
    int emissiveCount;
};
static ID3D11ComputeShader* g_CSCameraRays;
static ID3D11ComputeShader* g_CSBounce;
static ID3D11ComputeShader* g_CSFinal;
static ID3D11ComputeShader* g_CSCopyCount;
static ID3D11VertexShader* g_VSSplat;
static ID3D11PixelShader* g_PSSplat;
static ID3D11Buffer* g_DataSpheres;     static ID3D11ShaderResourceView* g_SRVSpheres;
static ID3D11Buffer* g_DataMaterials;   static ID3D11ShaderResourceView* g_SRVMaterials;
static ID3D11Buffer* g_DataParams;      static ID3D11ShaderResourceView* g_SRVParams;
static ID3D11Buffer* g_DataEmissives;   static ID3D11ShaderResourceView* g_SRVEmissives;
static ID3D11Buffer* g_DataRays;        static ID3D11ShaderResourceView* g_SRVRays;     static ID3D11UnorderedAccessView* g_UAVRays;
static ID3D11Buffer* g_DataRays2;       static ID3D11ShaderResourceView* g_SRVRays2;    static ID3D11UnorderedAccessView* g_UAVRays2;
static ID3D11Buffer* g_DataSplats;      static ID3D11ShaderResourceView* g_SRVSplats;   static ID3D11UnorderedAccessView* g_UAVSplats;
static ID3D11Buffer* g_DataCounter;     static ID3D11ShaderResourceView* g_SRVCounter;  static ID3D11UnorderedAccessView* g_UAVCounter;
static ID3D11Buffer* g_DataIndirectCounter;     static ID3D11UnorderedAccessView* g_UAVIndirectCounter;
static int g_SphereCount, g_ObjSize, g_MatSize;
static ID3D11Query *g_QueryBegin, *g_QueryEnd, *g_QueryDisjoint;
#endif // #if DO_COMPUTE_GPU

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    g_Backbuffer = new float[kBackbufferWidth * kBackbufferHeight * 4];
    memset(g_Backbuffer, 0, kBackbufferWidth * kBackbufferHeight * 4 * sizeof(g_Backbuffer[0]));

    InitializeTest(kBackbufferWidth, kBackbufferHeight);

    MyRegisterClass(hInstance);
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    if (FAILED(InitD3DDevice()))
    {
        ShutdownD3DDevice();
        return 0;
    }

    g_D3D11Device->CreateVertexShader(g_VSBytecode, ARRAYSIZE(g_VSBytecode), NULL, &g_VertexShader);
    g_D3D11Device->CreatePixelShader(g_PSBytecode, ARRAYSIZE(g_PSBytecode), NULL, &g_PixelShader);
#if DO_COMPUTE_GPU
    g_D3D11Device->CreateComputeShader(g_CSByteCodeCameraRays, ARRAYSIZE(g_CSByteCodeCameraRays), NULL, &g_CSCameraRays);
    g_D3D11Device->CreateComputeShader(g_CSByteCodeBounce, ARRAYSIZE(g_CSByteCodeBounce), NULL, &g_CSBounce);
    g_D3D11Device->CreateComputeShader(g_CSByteCodeFinal, ARRAYSIZE(g_CSByteCodeFinal), NULL, &g_CSFinal);
    g_D3D11Device->CreateComputeShader(g_CSByteCodeCopyCount, ARRAYSIZE(g_CSByteCodeCopyCount), NULL, &g_CSCopyCount);
    g_D3D11Device->CreateVertexShader(g_VSBytecodeSplat, ARRAYSIZE(g_VSBytecodeSplat), NULL, &g_VSSplat);
    g_D3D11Device->CreatePixelShader(g_PSBytecodeSplat, ARRAYSIZE(g_PSBytecodeSplat), NULL, &g_PSSplat);
#endif

    g_BackbufferTex = new D3D11Texture(kBackbufferWidth, kBackbufferHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
    g_BackbufferTex2 = new D3D11Texture(kBackbufferWidth, kBackbufferHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);
    g_TmpTex = new D3D11Texture(kBackbufferWidth, kBackbufferHeight, DXGI_FORMAT_R32G32B32A32_FLOAT);

    D3D11_SAMPLER_DESC smpDesc = {};
    smpDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    smpDesc.AddressU = smpDesc.AddressV = smpDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    g_D3D11Device->CreateSamplerState(&smpDesc, &g_SamplerLinear);

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    g_D3D11Device->CreateRasterizerState(&rasterDesc, &g_RasterState);

#if DO_COMPUTE_GPU
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
    g_D3D11Device->CreateBlendState(&blendDesc, &g_BlendStateOff);

    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
    g_D3D11Device->CreateBlendState(&blendDesc, &g_BlendStateAdd);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    int camSize;
    GetObjectCount(g_SphereCount, g_ObjSize, g_MatSize, camSize);
    assert(g_ObjSize == 20);
    assert(g_MatSize == 36);
    assert(camSize == 88);
    D3D11_BUFFER_DESC bdesc = {};
    bdesc.ByteWidth = g_SphereCount * g_ObjSize;
    bdesc.Usage = D3D11_USAGE_DEFAULT;
    bdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bdesc.CPUAccessFlags = 0;
    bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bdesc.StructureByteStride = g_ObjSize;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataSpheres);
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = g_SphereCount;
    g_D3D11Device->CreateShaderResourceView(g_DataSpheres, &srvDesc, &g_SRVSpheres);

    bdesc.ByteWidth = g_SphereCount * g_MatSize;
    bdesc.StructureByteStride = g_MatSize;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataMaterials);
    srvDesc.Buffer.NumElements = g_SphereCount;
    g_D3D11Device->CreateShaderResourceView(g_DataMaterials, &srvDesc, &g_SRVMaterials);

    bdesc.ByteWidth = sizeof(ComputeParams);
    bdesc.StructureByteStride = sizeof(ComputeParams);
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataParams);
    srvDesc.Buffer.NumElements = 1;
    g_D3D11Device->CreateShaderResourceView(g_DataParams, &srvDesc, &g_SRVParams);

    bdesc.ByteWidth = g_SphereCount * 4;
    bdesc.StructureByteStride = 4;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataEmissives);
    srvDesc.Buffer.NumElements = g_SphereCount;
    g_D3D11Device->CreateShaderResourceView(g_DataEmissives, &srvDesc, &g_SRVEmissives);

    const int kMaxShadowRays = 3;
    const size_t kRayDataSize = 28; // 28 for FP16, 40 without
    const size_t kMaxRays = kBackbufferWidth * kBackbufferHeight * DO_SAMPLES_PER_PIXEL * (1 + kMaxShadowRays);

    bdesc.ByteWidth = kMaxRays * kRayDataSize;
    bdesc.StructureByteStride = kRayDataSize;
    bdesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataRays);
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataRays2);
    srvDesc.Buffer.NumElements = kMaxRays;
    g_D3D11Device->CreateShaderResourceView(g_DataRays, &srvDesc, &g_SRVRays);
    g_D3D11Device->CreateShaderResourceView(g_DataRays2, &srvDesc, &g_SRVRays2);

    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = kMaxRays;
    uavDesc.Buffer.Flags = 0;
    g_D3D11Device->CreateUnorderedAccessView(g_DataRays, &uavDesc, &g_UAVRays);
    g_D3D11Device->CreateUnorderedAccessView(g_DataRays2, &uavDesc, &g_UAVRays2);

    const int kMaxSplats = kBackbufferWidth * kBackbufferHeight * DO_SAMPLES_PER_PIXEL * 3; //@TODO?
    const int kSplatDataSize = 16;
    bdesc.ByteWidth = kMaxSplats * kSplatDataSize;
    bdesc.StructureByteStride = kSplatDataSize;
    bdesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataSplats);
    srvDesc.Buffer.NumElements = kMaxSplats;
    g_D3D11Device->CreateShaderResourceView(g_DataSplats, &srvDesc, &g_SRVSplats);
    uavDesc.Buffer.NumElements = kMaxSplats;
    g_D3D11Device->CreateUnorderedAccessView(g_DataSplats, &uavDesc, &g_UAVSplats);


    bdesc.ByteWidth = 3 * 4;
    bdesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataCounter);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.BufferEx.NumElements = 3;
    srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    g_D3D11Device->CreateShaderResourceView(g_DataCounter, &srvDesc, &g_SRVCounter);

    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 3;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    g_D3D11Device->CreateUnorderedAccessView(g_DataCounter, &uavDesc, &g_UAVCounter);

    bdesc.ByteWidth = 7 * 4;
    bdesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    bdesc.CPUAccessFlags = 0;
    g_D3D11Device->CreateBuffer(&bdesc, NULL, &g_DataIndirectCounter);
    int drawArgs[] = { 1, 1, 1, 1, 1, 0, 0 };
    g_D3D11Ctx->UpdateSubresource(g_DataIndirectCounter, 0, NULL, &drawArgs, 0, 0);

    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 7;
    uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
    g_D3D11Device->CreateUnorderedAccessView(g_DataIndirectCounter, &uavDesc, &g_UAVIndirectCounter);

    D3D11_QUERY_DESC qDesc = {};
    qDesc.Query = D3D11_QUERY_TIMESTAMP;
    g_D3D11Device->CreateQuery(&qDesc, &g_QueryBegin);
    g_D3D11Device->CreateQuery(&qDesc, &g_QueryEnd);
    qDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    g_D3D11Device->CreateQuery(&qDesc, &g_QueryDisjoint);
#endif // #if DO_COMPUTE_GPU


    // Main message loop
    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            RenderFrame();
        }
    }

    delete g_TmpTex;
    delete g_BackbufferTex;
    delete g_BackbufferTex2;
    ShutdownTest();
    ShutdownD3DDevice();

    return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName  = L"TestClass";
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_HInstance = hInstance;
    RECT rc = { 0, 0, kBackbufferWidth, kBackbufferHeight };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);
    HWND hWnd = CreateWindowW(L"TestClass", L"Test", style, CW_USEDEFAULT, CW_USEDEFAULT, rc.right-rc.left, rc.bottom-rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
        return FALSE;
    g_Wnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

static uint64_t s_Time;
static int s_Count;
static char s_Buffer[200];

#if DO_COMPUTE_GPU
static void DoComputeCameraRays()
{
    int zeroCounts[] = { 0, 0, 0 };
    g_D3D11Ctx->UpdateSubresource(g_DataCounter, 0, NULL, &zeroCounts, 0, 0);

    ID3D11ShaderResourceView* srvs[] = {
        NULL,
        NULL,
        g_SRVSpheres,
        g_SRVMaterials,
        g_SRVParams,
        g_SRVEmissives,
        NULL
    };
    g_D3D11Ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
    ID3D11UnorderedAccessView* uavs[] = {
        g_TmpTex->uav,
        g_UAVCounter,
        g_UAVRays,
        NULL
    };
    g_D3D11Ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);
    g_D3D11Ctx->CSSetShader(g_CSCameraRays, NULL, 0);
    g_D3D11Ctx->Dispatch(kBackbufferWidth / kCSGroupSizeX, kBackbufferHeight / kCSGroupSizeY, 1);
}

static void DoComputeRayBounce(int depth)
{
    // copy indirect invocation count
    {
        ID3D11UnorderedAccessView* uavs[] = {
            g_UAVIndirectCounter,
            NULL,
            NULL,
            NULL,
        };
        g_D3D11Ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);

        ID3D11ShaderResourceView* srvs[] = {
            g_SRVCounter,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
        };
        g_D3D11Ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

        g_D3D11Ctx->CSSetShader(g_CSCopyCount, NULL, 0);
        g_D3D11Ctx->Dispatch(1, 1, 1);
        srvs[0] = NULL;
        g_D3D11Ctx->CSSetShaderResources(0, 1, srvs); // unbind counter
    }

    // set new rays count to zero before processing the next bounce
    //D3D11_MAPPED_SUBRESOURCE mapped;
    //g_D3D11Ctx->Map(g_DataCounter, 0, D3D11_MAP_READ, 0, &mapped);
    //g_D3D11Ctx->Unmap(g_DataCounter, 0);

    int zeroCount[] = { 0 };
    D3D11_BOX updateBox = {4, 0, 0, 8, 1, 1};
    g_D3D11Ctx->UpdateSubresource(g_DataCounter, 0, &updateBox, &zeroCount, 0, 0);

    //g_D3D11Ctx->Map(g_DataCounter, 0, D3D11_MAP_READ, 0, &mapped);
    //g_D3D11Ctx->Unmap(g_DataCounter, 0);

    // process ray bounce
    {
        ID3D11UnorderedAccessView* uavs[] = {
            g_TmpTex->uav,
            g_UAVCounter,
            (depth & 1) ? g_UAVRays : g_UAVRays2,
            g_UAVSplats,
        };
        g_D3D11Ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);

        ID3D11ShaderResourceView* srvs[] = {
            NULL,
            NULL,
            g_SRVSpheres,
            g_SRVMaterials,
            g_SRVParams,
            g_SRVEmissives,
            (depth & 1) ? g_SRVRays2 : g_SRVRays,
        };
        g_D3D11Ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

        g_D3D11Ctx->CSSetShader(g_CSBounce, NULL, 0);
        g_D3D11Ctx->DispatchIndirect(g_DataIndirectCounter, 0);
    }

    //g_D3D11Ctx->Map(g_DataCounter, 0, D3D11_MAP_READ, 0, &mapped);
    //g_D3D11Ctx->Unmap(g_DataCounter, 0);
}

static void DoComputeSplats()
{
    ID3D11UnorderedAccessView* uavs[] = {
        NULL,
        NULL,
        NULL,
        NULL
    };
    g_D3D11Ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);
    g_D3D11Ctx->OMSetRenderTargets(1, &g_TmpTex->rtv, nullptr);

    g_D3D11Ctx->VSSetShader(g_VSSplat, NULL, 0);
    g_D3D11Ctx->PSSetShader(g_PSSplat, NULL, 0);
    g_D3D11Ctx->VSSetShaderResources(0, 1, &g_SRVSplats);
    g_D3D11Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    g_D3D11Ctx->RSSetState(g_RasterState);
    g_D3D11Ctx->OMSetBlendState(g_BlendStateAdd, NULL, ~0);

    D3D11_BOX srcBox = { 8, 0, 0, 12, 1, 1 };
    g_D3D11Ctx->CopySubresourceRegion(g_DataIndirectCounter, 0, 12, 0, 0, g_DataCounter, 0, &srcBox);
    g_D3D11Ctx->DrawInstancedIndirect(g_DataIndirectCounter, 12);
    ID3D11ShaderResourceView* nullSRV = NULL;
    g_D3D11Ctx->VSSetShaderResources(0, 1, &nullSRV);

    g_D3D11Ctx->OMSetRenderTargets(1, &g_D3D11RenderTarget, nullptr);
}


static void DoComputeBlend()
{
    ID3D11UnorderedAccessView* uavs[] = {
        g_BufferIndex == 0 ? g_BackbufferTex->uav : g_BackbufferTex2->uav,
        g_UAVCounter,
        NULL,
        NULL,
    };
    g_D3D11Ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);

    ID3D11ShaderResourceView* srvs[] = {
        g_BufferIndex == 0 ? g_BackbufferTex2->srv : g_BackbufferTex->srv,
        g_TmpTex->srv,
        NULL,
        NULL,
        g_SRVParams,
        NULL,
        NULL,
    };
    g_D3D11Ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

    g_D3D11Ctx->CSSetShader(g_CSFinal, NULL, 0);
    g_D3D11Ctx->Dispatch(kBackbufferWidth / kCSGroupSizeX, kBackbufferHeight / kCSGroupSizeY, 1);
}

#endif // #if DO_COMPUTE_GPU


static void RenderFrame()
{
    static int s_FrameCount = 0;
    LARGE_INTEGER time1;

#if DO_COMPUTE_GPU
    QueryPerformanceCounter(&time1);
    float t = float(clock()) / CLOCKS_PER_SEC;


    // Update buffers containing scene & parameters
    UpdateTest(t, s_FrameCount);

    g_BufferIndex = 1 - g_BufferIndex;
    void* dataSpheres = alloca(g_SphereCount * g_ObjSize);
    void* dataMaterials = alloca(g_SphereCount * g_MatSize);
    void* dataEmissives = alloca(g_SphereCount * 4);
    ComputeParams dataParams;
    GetSceneDesc(dataSpheres, dataMaterials, &dataParams.cam, dataEmissives, &dataParams.emissiveCount);

    dataParams.sphereCount = g_SphereCount;
    dataParams.screenWidth = kBackbufferWidth;
    dataParams.screenHeight = kBackbufferHeight;
    dataParams.frames = s_FrameCount;
    dataParams.invWidth = 1.0f / kBackbufferWidth;
    dataParams.invHeight = 1.0f / kBackbufferHeight;
    float lerpFac = float(s_FrameCount) / float(s_FrameCount + 1);
#if DO_ANIMATE
    lerpFac *= DO_ANIMATE_SMOOTHING;
#endif
#if !DO_PROGRESSIVE
    lerpFac = 0;
#endif
    dataParams.lerpFac = lerpFac;

    g_D3D11Ctx->UpdateSubresource(g_DataSpheres, 0, NULL, dataSpheres, 0, 0);
    g_D3D11Ctx->UpdateSubresource(g_DataMaterials, 0, NULL, dataMaterials, 0, 0);
    g_D3D11Ctx->UpdateSubresource(g_DataParams, 0, NULL, &dataParams, 0, 0);
    g_D3D11Ctx->UpdateSubresource(g_DataEmissives, 0, NULL, dataEmissives, 0, 0);

    g_D3D11Ctx->Begin(g_QueryDisjoint);
    g_D3D11Ctx->End(g_QueryBegin);

    DoComputeCameraRays();
    for (int depth = 0; depth < kMaxDepth; ++depth)
        DoComputeRayBounce(depth);
    DoComputeSplats();
    DoComputeBlend();

    g_D3D11Ctx->End(g_QueryEnd);
    ID3D11UnorderedAccessView* uavs[] = { NULL };
    g_D3D11Ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);
    ++s_FrameCount;

#else
    QueryPerformanceCounter(&time1);
    float t = float(clock()) / CLOCKS_PER_SEC;
    static size_t s_RayCounter = 0;
    int rayCount;
    UpdateTest(t, s_FrameCount);
    DrawTest(t, s_FrameCount, g_Backbuffer, rayCount);
    s_FrameCount++;
    s_RayCounter += rayCount;
    LARGE_INTEGER time2;
    QueryPerformanceCounter(&time2);
    uint64_t dt = time2.QuadPart - time1.QuadPart;
    ++s_Count;
    s_Time += dt;
    if (s_Count > 10)
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);

        double s = double(s_Time) / double(frequency.QuadPart) / s_Count;
        sprintf_s(s_Buffer, sizeof(s_Buffer), "%.2fms (%.1f FPS) %.1fMrays/s %.2fMrays/frame frames %i\n", s * 1000.0f, 1.f / s, s_RayCounter / s_Count / s * 1.0e-6f, s_RayCounter / s_Count * 1.0e-6f, s_FrameCount);
        SetWindowTextA(g_Wnd, s_Buffer);
        OutputDebugStringA(s_Buffer);
        s_Count = 0;
        s_Time = 0;
        s_RayCounter = 0;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    g_D3D11Ctx->Map(g_BackbufferTex->tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    const uint8_t* src = (const uint8_t*)g_Backbuffer;
    uint8_t* dst = (uint8_t*)mapped.pData;
    for (int y = 0; y < kBackbufferHeight; ++y)
    {
        memcpy(dst, src, kBackbufferWidth * 16);
        src += kBackbufferWidth * 16;
        dst += mapped.RowPitch;
    }
    g_D3D11Ctx->Unmap(g_BackbufferTex->tex, 0);
#endif

    g_D3D11Ctx->VSSetShader(g_VertexShader, NULL, 0);
    g_D3D11Ctx->PSSetShader(g_PixelShader, NULL, 0);
    g_D3D11Ctx->PSSetShaderResources(0, 1, g_BufferIndex == 0 ? &g_BackbufferTex->srv : &g_BackbufferTex2->srv);
    g_D3D11Ctx->PSSetSamplers(0, 1, &g_SamplerLinear);
    g_D3D11Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_D3D11Ctx->RSSetState(g_RasterState);
    g_D3D11Ctx->OMSetBlendState(g_BlendStateOff, NULL, ~0);
    g_D3D11Ctx->Draw(3, 0);

    g_D3D11SwapChain->Present(0, 0);

#if DO_COMPUTE_GPU
    g_D3D11Ctx->End(g_QueryDisjoint);

    // get GPU times
    while (g_D3D11Ctx->GetData(g_QueryDisjoint, NULL, 0, 0) == S_FALSE) { Sleep(0); }
    D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
    g_D3D11Ctx->GetData(g_QueryDisjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
    if (!tsDisjoint.Disjoint)
    {
        UINT64 tsBegin, tsEnd;
        // Note: on some GPUs/drivers, even when the disjoint query above already said "yeah I have data",
        // might still not return "I have data" for timestamp queries before it.
        while (g_D3D11Ctx->GetData(g_QueryBegin, &tsBegin, sizeof(tsBegin), 0) == S_FALSE) { Sleep(0); }
        while (g_D3D11Ctx->GetData(g_QueryEnd, &tsEnd, sizeof(tsEnd), 0) == S_FALSE) { Sleep(0); }

        float s = float(tsEnd - tsBegin) / float(tsDisjoint.Frequency);

        static uint64_t s_RayCounter;
        D3D11_MAPPED_SUBRESOURCE mapped;
        g_D3D11Ctx->Map(g_DataCounter, 0, D3D11_MAP_READ, 0, &mapped);
        s_RayCounter += *(const int*)mapped.pData;
        g_D3D11Ctx->Unmap(g_DataCounter, 0);

        static float s_Time;
        ++s_Count;
        s_Time += s;
        if (s_Count > 50)
        {
            s = s_Time / s_Count;
            sprintf_s(s_Buffer, sizeof(s_Buffer), "%.2fms (%.1f FPS) %.1fMrays/s %.2fMrays/frame frames %i\n", s * 1000.0f, 1.f / s, s_RayCounter / s_Count / s * 1.0e-6f, s_RayCounter / s_Count * 1.0e-6f, s_FrameCount);
            SetWindowTextA(g_Wnd, s_Buffer);
            s_Count = 0;
            s_Time = 0;
            s_RayCounter = 0;
        }

    }
#endif // #if DO_COMPUTE_GPU
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


static HRESULT InitD3DDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_Wnd, &rc);
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
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &g_D3D11Device, &g_D3D11FeatureLevel, &g_D3D11Ctx);
    //hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &g_D3D11Device, &g_D3D11FeatureLevel, &g_D3D11Ctx);
    if (FAILED(hr))
        return hr;

    // Get DXGI factory
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_D3D11Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
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
    sd.OutputWindow = g_Wnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    hr = dxgiFactory->CreateSwapChain(g_D3D11Device, &sd, &g_D3D11SwapChain);

    // Prevent Alt-Enter
    dxgiFactory->MakeWindowAssociation(g_Wnd, DXGI_MWA_NO_ALT_ENTER);
    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    // RTV
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_D3D11SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr))
        return hr;
    hr = g_D3D11Device->CreateRenderTargetView(pBackBuffer, nullptr, &g_D3D11RenderTarget);
    pBackBuffer->Release();
    if (FAILED(hr))
        return hr;

    g_D3D11Ctx->OMSetRenderTargets(1, &g_D3D11RenderTarget, nullptr);

    // Viewport
    D3D11_VIEWPORT vp;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_D3D11Ctx->RSSetViewports(1, &vp);

    return S_OK;
}

static void ShutdownD3DDevice()
{
    if (g_D3D11Ctx) g_D3D11Ctx->ClearState();

    if (g_D3D11RenderTarget) g_D3D11RenderTarget->Release();
    if (g_D3D11SwapChain) g_D3D11SwapChain->Release();
    if (g_D3D11Ctx) g_D3D11Ctx->Release();
    if (g_D3D11Device) g_D3D11Device->Release();
}
