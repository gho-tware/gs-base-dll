#include "pch.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#ifndef DXGI_SWAP_EFFECT_FLIP_DISCARD
#define DXGI_SWAP_EFFECT_FLIP_DISCARD (DXGI_SWAP_EFFECT)4
#endif

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_hWndChild = nullptr;
std::atomic<bool> g_isMenuVisible(true);
bool g_dragging = false;
POINT g_dragStart;

std::vector<std::wstring> GetWindowTitles() {
    std::vector<std::wstring> windowTitles;

    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
        int length = GetWindowTextLength(hWnd);

        if (length == 0) return TRUE;

        std::wstring windowTitle(length + 1, L'\0');
        GetWindowText(hWnd, &windowTitle[0], length + 1);
        windowTitle.resize(length);

        auto titles = reinterpret_cast<std::vector<std::wstring>*>(lParam);
        titles->push_back(windowTitle);

        return TRUE;
        }, reinterpret_cast<LPARAM>(&windowTitles));

    return windowTitles;
}

HWND FindGameWindow() {
    auto windowTitles = GetWindowTitles();

    for (const auto& title : windowTitles) {
        if (title.find(L"FiveM") != std::wstring::npos) {
            return FindWindow(NULL, title.c_str());
        }
    }
    return NULL;
}

void RenderMenu() {
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };


    if (!g_isMenuVisible) {
        ShowWindow(g_hWndChild, SW_HIDE);
        return;
    }
    else {
        ShowWindow(g_hWndChild, SW_SHOW);
    }

    if (!g_pd3dDeviceContext || !g_mainRenderTargetView || !g_pSwapChain) {
        return;
    }

    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
    HRESULT hr = g_pSwapChain->Present(1, 0); // V-Sync habilitado

    if (FAILED(hr)) {
        return;
    }
}

LRESULT CALLBACK ChildWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        g_dragging = true;
        GetCursorPos(&g_dragStart);
        SetCapture(hWnd);
        return 0;

    case WM_MOUSEMOVE:
        if (g_dragging) {
            POINT cursorPos;

            GetCursorPos(&cursorPos);

            int dx = cursorPos.x - g_dragStart.x;
            int dy = cursorPos.y - g_dragStart.y;

            RECT rect;

            GetWindowRect(hWnd, &rect);
            MoveWindow(hWnd, rect.left + dx, rect.top + dy, rect.right - rect.left, rect.bottom - rect.top, TRUE);
            g_dragStart = cursorPos;

            // Forçar o redesenho completo da janela
            InvalidateRect(hWnd, NULL, TRUE);
            UpdateWindow(hWnd);
        }
        return 0;

    case WM_LBUTTONUP:
        g_dragging = false;
        ReleaseCapture();
        return 0;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

void InitializeMenu() {
    HWND hWnd = FindGameWindow();
    if (hWnd == NULL) {
        return;
    }

    RECT rect;
    GetClientRect(hWnd, &rect);

    int windowWidth = 800; // largura janela
    int windowHeight = 500; // altura janela

    g_hWndChild = CreateWindowEx(0, L"STATIC", NULL, WS_CHILD | WS_VISIBLE, 100, 100, windowWidth, windowHeight, hWnd, NULL, NULL, NULL);

    if (g_hWndChild == NULL) {
        return;
    }

    SetWindowLongPtr(g_hWndChild, GWLP_WNDPROC, (LONG_PTR)ChildWndProc);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));

    sd.BufferCount = 2;
    sd.BufferDesc.Width = windowWidth;
    sd.BufferDesc.Height = windowHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWndChild;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;

    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,
    };

    UINT createDeviceFlags = 0;

    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (FAILED(res)) {
        return;
    }

    ID3D11Texture2D* pBackBuffer;
    res = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(res)) {
        return;
    }

    res = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(res)) {
        return;
    }

    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)windowWidth;
    vp.Height = (FLOAT)windowHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pd3dDeviceContext->RSSetViewports(1, &vp);

    MSG msg;
    while (true) {
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        RenderMenu();

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Aproximadamente 60 FPS
    }
}

void MonitorKeyPress() {
    const int KEY_CHECK_INTERVAL_MS = 50;
    const int DEBOUNCE_INTERVAL_MS = 200;

    while (true) {
        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            g_isMenuVisible = !g_isMenuVisible;
            std::this_thread::sleep_for(std::chrono::milliseconds(DEBOUNCE_INTERVAL_MS));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(KEY_CHECK_INTERVAL_MS));
    }
}

void Cleanup() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_hWndChild) { DestroyWindow(g_hWndChild); g_hWndChild = nullptr; }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            Beep(500, 400);
            std::thread(InitializeMenu).detach();
            std::thread(MonitorKeyPress).detach();
            break;
        case DLL_PROCESS_DETACH:
            Cleanup();
            break;
    }

    return TRUE;
}