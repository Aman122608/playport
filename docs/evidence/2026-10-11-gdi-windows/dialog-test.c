/* SPDX-License-Identifier: GPL-3.0-or-later
 * dialog-test.exe: the reproducer of the 2026-10-11 GDI windows evidence
 * (decision 0076), imported on the phone as a Local title. A full-screen
 * D3D11 window presents (the first frame), then the game's thread opens a
 * message box owned by it and waits for OK, as a game's error or prompt does.
 * After OK the window presents another colour; then another thread opens an
 * error box and the game exits 4 s later with that box still open.
 *
 *   blue, 8 s   -> "Dialog test" box (OK)   -> green, 6 s
 *   -> "Dialog test error" box from a second thread, red 4 s -> exit code 3
 *
 * x86_64-w64-mingw32-clang -O2 -mwindows dialog-test.c -o dialog-test.exe -ld3d11 -ldxguid
 */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>

static IDXGISwapChain *chain;
static ID3D11DeviceContext *ctx;
static ID3D11RenderTargetView *target;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CLOSE) ExitProcess(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void present_for(DWORD ms, float r, float g, float b) {
    const float colour[4] = { r, g, b, 1 };
    DWORD start = GetTickCount();
    while (GetTickCount() - start < ms) {
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        ID3D11DeviceContext_ClearRenderTargetView(ctx, target, colour);
        IDXGISwapChain_Present(chain, 1, 0);
        Sleep(16);
    }
}

static DWORD WINAPI error_box(void *unused) {
    MessageBoxW(NULL, L"The game ends 4 seconds after this box opens, with the box still open.",
                L"Dialog test error", MB_OK | MB_ICONERROR);
    return 0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"DialogTest";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, L"DialogTest", L"Dialog test", WS_POPUP | WS_VISIBLE,
                                0, 0, w, h, NULL, NULL, inst, NULL);

    DXGI_SWAP_CHAIN_DESC desc = { 0 };
    desc.BufferDesc.Width = w;
    desc.BufferDesc.Height = h;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = hwnd;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ID3D11Device *dev;
    if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                                             D3D11_SDK_VERSION, &desc, &chain, &dev, NULL, &ctx))) {
        MessageBoxW(hwnd, L"No D3D11 device.", L"Dialog test", MB_OK | MB_ICONERROR);
        return 1;
    }
    ID3D11Texture2D *back;
    IDXGISwapChain_GetBuffer(chain, 0, &IID_ID3D11Texture2D, (void **)&back);
    ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)back, NULL, &target);
    ID3D11Texture2D_Release(back);

    present_for(8000, 0.1f, 0.2f, 0.8f);
    MessageBoxW(hwnd, L"This box opened after the first frame. Press OK and the game goes on.",
                L"Dialog test", MB_OK | MB_ICONINFORMATION);
    present_for(6000, 0.1f, 0.7f, 0.2f);
    CloseHandle(CreateThread(NULL, 0, error_box, NULL, 0, NULL));
    present_for(4000, 0.8f, 0.1f, 0.1f);
    ExitProcess(3);
}
