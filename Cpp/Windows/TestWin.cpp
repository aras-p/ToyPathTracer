#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>

#include "../Source/Test.h"

static HINSTANCE g_HInstance;
static HWND g_Wnd;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static const int g_BackbufferWidth = 1280;
static const int g_BackbufferHeight = 720;
static float* g_Backbuffer;
static uint32_t* g_BackbufferBytes;
static HBITMAP g_BackbufferBitmap;

static void InitBackbufferBitmap()
{
    BITMAPINFO bmi;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_BackbufferWidth;
    bmi.bmiHeader.biHeight = g_BackbufferHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = g_BackbufferWidth * g_BackbufferHeight * 4;
    HDC hdc = CreateCompatibleDC(GetDC(0));
    g_BackbufferBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&g_BackbufferBytes, NULL, 0x0);
    g_Backbuffer = new float[g_BackbufferWidth * g_BackbufferHeight * 4];
    memset(g_Backbuffer, 0, g_BackbufferWidth * g_BackbufferHeight * 4 * sizeof(g_Backbuffer[0]));
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
    InitBackbufferBitmap();

    InitializeTest();

    MyRegisterClass(hInstance);
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Main message loop
    MSG msg;
    msg.message = WM_NULL;
    while (msg.message != WM_QUIT)
    {
        bool gotMsg = (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) != 0);
        if (gotMsg)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            InvalidateRect(g_Wnd, NULL, FALSE);
            UpdateWindow(g_Wnd);
        }
    }

    ShutdownTest();

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
    HWND hWnd = CreateWindowW(L"TestClass", L"Test", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
        return FALSE;
    g_Wnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

// http://chilliant.blogspot.com.au/2012/08/srgb-approximations-for-hlsl.html
static uint32_t LinearToSRGB (float x)
{
    x = std::max(x, 0.0f);
    x = std::max(1.055f * powf(x, 0.416666667f) - 0.055f, 0.0f);
    uint32_t u = std::min((uint32_t)(x * 255.9f), 255u);
    return u;
}

static void DrawBitmap(HDC dc, int width, int height)
{
    uint32_t* dst = g_BackbufferBytes;
    const float* src = g_Backbuffer;
    for (int i = 0; i < g_BackbufferWidth * g_BackbufferHeight; ++i)
    {
        uint32_t r = LinearToSRGB(src[0]);
        uint32_t g = LinearToSRGB(src[1]);
        uint32_t b = LinearToSRGB(src[2]);
        *dst = b | (g << 8) | (r << 16);
        src += 4;
        dst += 1;
    }
    HDC srcDC = CreateCompatibleDC(dc);
    SetStretchBltMode(dc, COLORONCOLOR);
    SelectObject(srcDC, g_BackbufferBitmap);
    StretchBlt(dc, 0, 0, width, height, srcDC, 0, 0, g_BackbufferWidth, g_BackbufferHeight, SRCCOPY);
    DeleteObject(srcDC);
}

static uint64_t s_Time;
static int s_Count;
static char s_Buffer[200];

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            LARGE_INTEGER time1;
            QueryPerformanceCounter(&time1);
            float t = float(clock()) / CLOCKS_PER_SEC;
            static int s_FrameCount = 0;
            static size_t s_RayCounter = 0;
            int rayCount;
            DrawTest(t, s_FrameCount++, g_BackbufferWidth, g_BackbufferHeight, g_Backbuffer, rayCount);
            s_RayCounter += rayCount;
            LARGE_INTEGER time2;
            QueryPerformanceCounter(&time2);

            PAINTSTRUCT ps;
            RECT rect;
            HDC hdc = BeginPaint(hWnd, &ps);
            GetClientRect(hWnd, &rect);
            DrawBitmap(hdc, rect.right, rect.bottom);

            uint64_t dt = time2.QuadPart - time1.QuadPart;
            ++s_Count;
            s_Time += dt;
            if (s_Count > 10)
            {
                LARGE_INTEGER frequency;
                QueryPerformanceFrequency(&frequency);

                double s = double(s_Time) / double(frequency.QuadPart) / s_Count;
                sprintf_s(s_Buffer, sizeof(s_Buffer), "%.2fms (%.1f FPS) %.1fMrays/s %.2fMrays/frame frames %i\n", s * 1000.0f, 1.f / s, s_RayCounter / s_Count / s * 1.0e-6f, s_RayCounter / s_Count * 1.0e-6f, s_FrameCount);
                OutputDebugStringA(s_Buffer);
                s_Count = 0;
                s_Time = 0;
                s_RayCounter = 0;
            }
            RECT textRect;
            textRect.left = 5;
            textRect.top = 5;
            textRect.right = 500;
            textRect.bottom = 30;
            SetTextColor(hdc, 0x00000080);
            SetBkMode(hdc, TRANSPARENT);
            DrawTextA(hdc, s_Buffer, (int)strlen(s_Buffer), &textRect, DT_NOCLIP | DT_LEFT | DT_TOP);
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
