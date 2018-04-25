#include <stdint.h>
#include "D3D11Utils.h"

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

static void RenderFrame();

static float* g_Backbuffer;

static ID3D11VertexShader* g_VertexShader;
static ID3D11PixelShader* g_PixelShader;


static d3d11::Texture *g_BackbufferTex, *g_BackbufferTex2, *g_TmpTex;
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

static d3d11::Buffer *g_DataSpheres, *g_DataMaterials, *g_DataParams, *g_DataEmissives;
static d3d11::Buffer *g_DataRays, *g_DataRays2, *g_DataSplats;
static d3d11::Buffer *g_DataCounter, *g_DataIndirectCounter;
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

    if (FAILED(d3d11::Init (g_Wnd)))
    {
        d3d11::Shutdown();
        return 0;
    }

    d3d11::device->CreateVertexShader(g_VSBytecode, ARRAYSIZE(g_VSBytecode), NULL, &g_VertexShader);
    d3d11::device->CreatePixelShader(g_PSBytecode, ARRAYSIZE(g_PSBytecode), NULL, &g_PixelShader);
#if DO_COMPUTE_GPU
    d3d11::device->CreateComputeShader(g_CSByteCodeCameraRays, ARRAYSIZE(g_CSByteCodeCameraRays), NULL, &g_CSCameraRays);
    d3d11::device->CreateComputeShader(g_CSByteCodeBounce, ARRAYSIZE(g_CSByteCodeBounce), NULL, &g_CSBounce);
    d3d11::device->CreateComputeShader(g_CSByteCodeFinal, ARRAYSIZE(g_CSByteCodeFinal), NULL, &g_CSFinal);
    d3d11::device->CreateComputeShader(g_CSByteCodeCopyCount, ARRAYSIZE(g_CSByteCodeCopyCount), NULL, &g_CSCopyCount);
    d3d11::device->CreateVertexShader(g_VSBytecodeSplat, ARRAYSIZE(g_VSBytecodeSplat), NULL, &g_VSSplat);
    d3d11::device->CreatePixelShader(g_PSBytecodeSplat, ARRAYSIZE(g_PSBytecodeSplat), NULL, &g_PSSplat);
#endif

    int textureFlags = d3d11::Texture::kFlagDynamic;
#if DO_COMPUTE_GPU
    textureFlags = d3d11::Texture::kFlagRenderTarget | d3d11::Texture::kFlagUAV;
#endif
    g_BackbufferTex = new d3d11::Texture(kBackbufferWidth, kBackbufferHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, textureFlags);
    g_BackbufferTex2 = new d3d11::Texture(kBackbufferWidth, kBackbufferHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, textureFlags);
    g_TmpTex = new d3d11::Texture(kBackbufferWidth, kBackbufferHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, textureFlags);

#if DO_COMPUTE_GPU
    int camSize;
    GetObjectCount(g_SphereCount, g_ObjSize, g_MatSize, camSize);
    assert(g_ObjSize == 20);
    assert(g_MatSize == 36);
    assert(camSize == 88);

    g_DataSpheres   = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, g_SphereCount, g_ObjSize, 0);
    g_DataMaterials = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, g_SphereCount, g_MatSize, 0);
    g_DataParams    = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, 1, sizeof(ComputeParams), 0);
    g_DataEmissives = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, g_SphereCount, sizeof(int), 0);

    const int kMaxShadowRays = 3;
    const size_t kRayDataSize = 28; // 28 for FP16, 40 without
    const size_t kMaxRays = kBackbufferWidth * kBackbufferHeight * DO_SAMPLES_PER_PIXEL * (1 + kMaxShadowRays);

    const int kMaxSplats = kBackbufferWidth * kBackbufferHeight * DO_SAMPLES_PER_PIXEL * 3; //@TODO?
    const int kSplatDataSize = 16;

    g_DataRays  = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, kMaxRays, kRayDataSize, d3d11::Buffer::kFlagUAV);
    g_DataRays2 = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, kMaxRays, kRayDataSize, d3d11::Buffer::kFlagUAV);
    g_DataSplats = new d3d11::Buffer(d3d11::Buffer::kTypeStructured, kMaxSplats, kSplatDataSize, d3d11::Buffer::kFlagUAV);
    g_DataCounter = new d3d11::Buffer(d3d11::Buffer::kTypeRaw, 3, 4, d3d11::Buffer::kFlagUAV | d3d11::Buffer::kFlagCPUReadable);
    g_DataIndirectCounter = new d3d11::Buffer(d3d11::Buffer::kTypeRaw, 7, 4, d3d11::Buffer::kFlagUAV | d3d11::Buffer::kFlagIndirectArgs);

    int drawArgs[] = { 1, 1, 1, 1, 1, 0, 0 };
    d3d11::ctx->UpdateSubresource(g_DataIndirectCounter->buffer, 0, NULL, &drawArgs, 0, 0);

    D3D11_QUERY_DESC qDesc = {};
    qDesc.Query = D3D11_QUERY_TIMESTAMP;
    d3d11::device->CreateQuery(&qDesc, &g_QueryBegin);
    d3d11::device->CreateQuery(&qDesc, &g_QueryEnd);
    qDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    d3d11::device->CreateQuery(&qDesc, &g_QueryDisjoint);
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

    delete g_TmpTex; delete g_BackbufferTex; delete g_BackbufferTex2;
#if DO_COMPUTE_GPU
    delete g_DataSpheres; delete g_DataMaterials; delete g_DataParams; delete g_DataEmissives;
    delete g_DataRays; delete g_DataRays2; delete g_DataSplats;
    delete g_DataCounter; delete g_DataIndirectCounter;
#endif
    ShutdownTest();
    d3d11::Shutdown();

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
    d3d11::ctx->UpdateSubresource(g_DataCounter->buffer, 0, NULL, &zeroCounts, 0, 0);

    ID3D11ShaderResourceView* srvs[] = {
        NULL,
        NULL,
        g_DataSpheres->srv,
        g_DataMaterials->srv,
        g_DataParams->srv,
        g_DataEmissives->srv,
        NULL
    };
    d3d11::ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
    ID3D11UnorderedAccessView* uavs[] = {
        g_TmpTex->uav,
        g_DataCounter->uav,
        g_DataRays->uav,
        NULL
    };
    d3d11::ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);
    d3d11::ctx->CSSetShader(g_CSCameraRays, NULL, 0);
    d3d11::ctx->Dispatch(kBackbufferWidth / kCSGroupSizeX, kBackbufferHeight / kCSGroupSizeY, 1);
}

static void DoComputeRayBounce(int depth)
{
    // copy indirect invocation count
    {
        ID3D11UnorderedAccessView* uavs[] = {
            g_DataIndirectCounter->uav,
            NULL,
            NULL,
            NULL,
        };
        d3d11::ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);

        ID3D11ShaderResourceView* srvs[] = {
            g_DataCounter->srv,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
        };
        d3d11::ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

        d3d11::ctx->CSSetShader(g_CSCopyCount, NULL, 0);
        d3d11::ctx->Dispatch(1, 1, 1);
        srvs[0] = NULL;
        d3d11::ctx->CSSetShaderResources(0, 1, srvs); // unbind counter
    }

    // set new rays count to zero before processing the next bounce
    int zeroCount[] = { 0 };
    D3D11_BOX updateBox = {4, 0, 0, 8, 1, 1};
    d3d11::ctx->UpdateSubresource(g_DataCounter->buffer, 0, &updateBox, &zeroCount, 0, 0);

    // process ray bounce
    {
        ID3D11UnorderedAccessView* uavs[] = {
            g_TmpTex->uav,
            g_DataCounter->uav,
            (depth & 1) ? g_DataRays->uav : g_DataRays2->uav,
            g_DataSplats->uav,
        };
        d3d11::ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);

        ID3D11ShaderResourceView* srvs[] = {
            NULL,
            NULL,
            g_DataSpheres->srv,
            g_DataMaterials->srv,
            g_DataParams->srv,
            g_DataEmissives->srv,
            (depth & 1) ? g_DataRays2->srv : g_DataRays->srv,
        };
        d3d11::ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

        d3d11::ctx->CSSetShader(g_CSBounce, NULL, 0);
        d3d11::ctx->DispatchIndirect(g_DataIndirectCounter->buffer, 0);
    }
}

static void DoComputeSplats()
{
    ID3D11UnorderedAccessView* uavs[] = {
        NULL,
        NULL,
        NULL,
        NULL
    };
    d3d11::ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);
    d3d11::ctx->OMSetRenderTargets(1, &g_TmpTex->rtv, nullptr);

    d3d11::ctx->VSSetShader(g_VSSplat, NULL, 0);
    d3d11::ctx->PSSetShader(g_PSSplat, NULL, 0);
    d3d11::ctx->VSSetShaderResources(0, 1, &g_DataSplats->srv);
    d3d11::ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    d3d11::ctx->RSSetState(d3d11::rasterSolidNoCull);
    d3d11::ctx->OMSetBlendState(d3d11::blendAdditive, NULL, ~0);

    D3D11_BOX srcBox = { 8, 0, 0, 12, 1, 1 };
    d3d11::ctx->CopySubresourceRegion(g_DataIndirectCounter->buffer, 0, 12, 0, 0, g_DataCounter->buffer, 0, &srcBox);
    d3d11::ctx->DrawInstancedIndirect(g_DataIndirectCounter->buffer, 12);
    ID3D11ShaderResourceView* nullSRV = NULL;
    d3d11::ctx->VSSetShaderResources(0, 1, &nullSRV);

    d3d11::ctx->OMSetRenderTargets(1, &d3d11::renderTarget, nullptr);
}


static void DoComputeBlend()
{
    ID3D11UnorderedAccessView* uavs[] = {
        g_BufferIndex == 0 ? g_BackbufferTex->uav : g_BackbufferTex2->uav,
        g_DataCounter->uav,
        NULL,
        NULL,
    };
    d3d11::ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);

    ID3D11ShaderResourceView* srvs[] = {
        g_BufferIndex == 0 ? g_BackbufferTex2->srv : g_BackbufferTex->srv,
        g_TmpTex->srv,
        NULL,
        NULL,
        g_DataParams->srv,
        NULL,
        NULL,
    };
    d3d11::ctx->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);

    d3d11::ctx->CSSetShader(g_CSFinal, NULL, 0);
    d3d11::ctx->Dispatch(kBackbufferWidth / kCSGroupSizeX, kBackbufferHeight / kCSGroupSizeY, 1);
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

    d3d11::ctx->UpdateSubresource(g_DataSpheres->buffer, 0, NULL, dataSpheres, 0, 0);
    d3d11::ctx->UpdateSubresource(g_DataMaterials->buffer, 0, NULL, dataMaterials, 0, 0);
    d3d11::ctx->UpdateSubresource(g_DataParams->buffer, 0, NULL, &dataParams, 0, 0);
    d3d11::ctx->UpdateSubresource(g_DataEmissives->buffer, 0, NULL, dataEmissives, 0, 0);

    d3d11::ctx->Begin(g_QueryDisjoint);
    d3d11::ctx->End(g_QueryBegin);

    DoComputeCameraRays();
    for (int depth = 0; depth < kMaxDepth; ++depth)
        DoComputeRayBounce(depth);
    DoComputeSplats();
    DoComputeBlend();

    d3d11::ctx->End(g_QueryEnd);
    ID3D11UnorderedAccessView* uavs[] = { NULL };
    d3d11::ctx->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, NULL);
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
    d3d11::ctx->Map(g_BackbufferTex->tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    const uint8_t* src = (const uint8_t*)g_Backbuffer;
    uint8_t* dst = (uint8_t*)mapped.pData;
    for (int y = 0; y < kBackbufferHeight; ++y)
    {
        memcpy(dst, src, kBackbufferWidth * 16);
        src += kBackbufferWidth * 16;
        dst += mapped.RowPitch;
    }
    d3d11::ctx->Unmap(g_BackbufferTex->tex, 0);
#endif

    d3d11::ctx->VSSetShader(g_VertexShader, NULL, 0);
    d3d11::ctx->PSSetShader(g_PixelShader, NULL, 0);
    d3d11::ctx->PSSetShaderResources(0, 1, g_BufferIndex == 0 ? &g_BackbufferTex->srv : &g_BackbufferTex2->srv);
    d3d11::ctx->PSSetSamplers(0, 1, &d3d11::samplerLinear);
    d3d11::ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d11::ctx->RSSetState(d3d11::rasterSolidNoCull);
    d3d11::ctx->OMSetBlendState(d3d11::blendOff, NULL, ~0);
    d3d11::ctx->Draw(3, 0);

    d3d11::swapChain->Present(0, 0);

#if DO_COMPUTE_GPU
    d3d11::ctx->End(g_QueryDisjoint);

    // get GPU times
    while (d3d11::ctx->GetData(g_QueryDisjoint, NULL, 0, 0) == S_FALSE) { Sleep(0); }
    D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
    d3d11::ctx->GetData(g_QueryDisjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
    if (!tsDisjoint.Disjoint)
    {
        UINT64 tsBegin, tsEnd;
        // Note: on some GPUs/drivers, even when the disjoint query above already said "yeah I have data",
        // might still not return "I have data" for timestamp queries before it.
        while (d3d11::ctx->GetData(g_QueryBegin, &tsBegin, sizeof(tsBegin), 0) == S_FALSE) { Sleep(0); }
        while (d3d11::ctx->GetData(g_QueryEnd, &tsEnd, sizeof(tsEnd), 0) == S_FALSE) { Sleep(0); }

        float s = float(tsEnd - tsBegin) / float(tsDisjoint.Frequency);

        static uint64_t s_RayCounter;
        D3D11_MAPPED_SUBRESOURCE mapped;
        d3d11::ctx->Map(g_DataCounter->buffer, 0, D3D11_MAP_READ, 0, &mapped);
        s_RayCounter += *(const int*)mapped.pData;
        d3d11::ctx->Unmap(g_DataCounter->buffer, 0);

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

