#include <windows.h>
#include <d2d1.h>
#include <cmath>
#include <iostream>
#pragma comment(lib, "d2d1.lib")

// --- Findet den WorkerW, der hinter den Icons liegt ---
// (siehe Originalcode von CodeProject)
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    // Suche nach SHELLDLL_DefView als Child
    HWND p = FindWindowEx(hwnd, NULL, L"SHELLDLL_DefView", NULL);
    HWND* ret = (HWND*)lParam;

    if (p)
    {
        // Nimm das nächste WorkerW nach diesem Fenster!
        *ret = FindWindowEx(NULL, hwnd, L"WorkerW", NULL);
    }
    return TRUE;
}

// Gibt das Handle des richtigen WorkerW-Fensters zurück
HWND get_wallpaper_window()
{
    HWND progman = FindWindow(L"ProgMan", NULL);
    // Nachricht 0x052C an Progman: Erzeuge WorkerW hinter den Icons
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

    // Suche nach dem richtigen WorkerW (siehe oben)
    HWND wallpaper_hwnd = nullptr;
    EnumWindows(EnumWindowsProc, (LPARAM)&wallpaper_hwnd);
    return wallpaper_hwnd;
}

// ----------------------- Direct2D-Animation ------------------------

HWND g_hwnd = nullptr;
ID2D1Factory* g_pFactory = nullptr;
ID2D1HwndRenderTarget* g_pRenderTarget = nullptr;
ID2D1SolidColorBrush* g_pBrush = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    if (msg == WM_PAINT && g_pRenderTarget) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);

        g_pRenderTarget->BeginDraw();

        // Einfach animierte Fläche
        float t = GetTickCount() * 0.001f;
        float r = 0.5f + 0.5f * sinf(t * 2.0f);
        float g = 0.5f + 0.5f * sinf(t * 0.8f + 1.0f);
        float b = 0.5f + 0.5f * sinf(t * 1.3f + 2.0f);

        g_pRenderTarget->Clear(D2D1::ColorF(r, g, b, 1.0f));
        g_pBrush->SetColor(D2D1::ColorF(1.0f - r, 1.0f - g, 1.0f - b, 1.0f));
        D2D1_RECT_F rect = D2D1::RectF(100 + 100 * sinf(t), 100, 300 + 100 * sinf(t), 300);
        g_pRenderTarget->FillRectangle(rect, g_pBrush);

        g_pRenderTarget->EndDraw();
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool InitD2D(HWND hwnd, int w, int h)
{
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pFactory);
    if (FAILED(hr)) return false;

    hr = g_pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(w, h)),
        &g_pRenderTarget
    );
    if (FAILED(hr)) return false;

    hr = g_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 1.0f, 1.0f), &g_pBrush);
    return SUCCEEDED(hr);
}

void CleanupD2D()
{
    if (g_pBrush) g_pBrush->Release();
    if (g_pRenderTarget) g_pRenderTarget->Release();
    if (g_pFactory) g_pFactory->Release();
}

int main()
{
    // 1. Richtigen WorkerW ermitteln (siehe oben)
    HWND wallpaperParent = get_wallpaper_window();
    if (!wallpaperParent) {
        MessageBox(0, L"Kein gültiger WorkerW gefunden!", L"Fehler", MB_ICONERROR);
        return 1;
    }
    std::wcout << L"WallpaperParent HWND: " << wallpaperParent << std::endl;

    // 2. Fensterklasse registrieren
    HINSTANCE hInst = GetModuleHandle(0);
    WNDCLASSEX wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"DXWallpaperWnd";
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    RegisterClassEx(&wc);

    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    // 3. Fenster als Child vom "richtigen" WorkerW erzeugen (WS_CHILD!)
    g_hwnd = CreateWindowEx(
        0,  // KEINE Layered/Transparent-Flags, sonst manchmal unsichtbar!
        L"DXWallpaperWnd", NULL, WS_CHILD | WS_VISIBLE,
        0, 0, w, h, wallpaperParent, NULL, hInst, NULL);

    ShowWindow(g_hwnd, SW_SHOW);

    if (!InitD2D(g_hwnd, w, h)) {
        MessageBox(0, L"Direct2D init failed", L"Fehler", MB_ICONERROR);
        return 1;
    }

    // Animation Loop (klassisch)
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        InvalidateRect(g_hwnd, 0, FALSE);
        Sleep(3);
    }

    CleanupD2D();
    return 0;
}
