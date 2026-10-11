/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * wgl: the WGL behaviours OpenGL games rely on, checked one after another on
 * x86-64 (docs/plans/2026-10-11-opengl-zink.md chunk 2, PLA-162). Each check
 * logs "wgl: CHECK <name>: ok|FAIL <detail>" through OutputDebugString; most
 * read pixels back, so a check proves what was drawn, not only that a call
 * returned. In order:
 *
 *   dummy         a hidden dummy window with its own context that swaps once and
 *                 stays alive, as games' pixel-format queries do; WGL extensions
 *   first-frame   the game window (wglChoosePixelFormatARB, RGBA8, depth 24,
 *                 stencil 8; a GL 3.3 compatibility context) draws before its
 *                 first swap, with no GL error, and reads its colour back
 *   depth-stencil a stencil mask and a depth test, read back
 *   interval      wglSwapIntervalEXT 1 and 0: the frame rate of each
 *   pbuffer       a pbuffer drawn and read back, with a context sharing the
 *                 window context's texture; wglShareLists
 *   srgb, msaa    with the mode argument srgb, the game window's format is
 *                 sRGB-capable and GL_FRAMEBUFFER_SRGB encodes a linear 0.5; with
 *                 msaa, it has 4x multisampling and an edge pixel is resolved
 *   second-window a second visible window presents (its first swap takes the
 *                 game layer), reads back, and goes; then hand-back: the first
 *                 window presents again
 *   border        GL_CLAMP_TO_BORDER's colour (custom border colours)
 *   resize        the window at half size: the frame is the new size
 *   modeset       ChangeDisplaySettings to a smaller mode, the window that size
 *                 (its frame is scaled to the screen), and back
 *
 * then for SECONDS (default 20) clears to dark teal and turns a fixed-function
 * triangle, SwapBuffers each frame at swap interval 1, logging the rate every
 * 5 s. Exit code: the number of failed checks.
 *
 *    wgl.exe [SECONDS [srgb|msaa]]
 *
 * Built with llvm-mingw by tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static PFNWGLGETEXTENSIONSSTRINGARBPROC get_extensions;
static PFNWGLCHOOSEPIXELFORMATARBPROC choose_format;
static PFNWGLCREATECONTEXTATTRIBSARBPROC create_context;
static PFNWGLSWAPINTERVALEXTPROC swap_interval;
static PFNWGLGETSWAPINTERVALEXTPROC get_swap_interval;
static PFNWGLCREATEPBUFFERARBPROC create_pbuffer;
static PFNWGLGETPBUFFERDCARBPROC get_pbuffer_dc;
static PFNWGLRELEASEPBUFFERDCARBPROC release_pbuffer_dc;
static PFNWGLDESTROYPBUFFERARBPROC destroy_pbuffer;
static int failures;

static void probe_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    int n = snprintf(line, sizeof(line), "wgl: ");
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
}

static void check(const char *name, BOOL ok, const char *fmt, ...)
{
    char detail[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);
    probe_log("CHECK %s: %s %s\n", name, ok ? "ok" : "FAIL", detail);
    if (!ok) failures++;
}

static double now_s(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / freq.QuadPart;
}

static void pump(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static HWND make_window(const char *name, BOOL visible, int width, int height)
{
    return CreateWindowExA(0, "wgl", name, WS_POPUP | (visible ? WS_VISIBLE : 0), 0, 0, width, height,
                           NULL, NULL, GetModuleHandleA(NULL), NULL);
}

static void read_pixel(int x, int y, unsigned char rgba[4])
{
    glReadBuffer(GL_BACK);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

static BOOL is_colour(const unsigned char rgba[4], int r, int g, int b)
{
    return abs(rgba[0] - r) <= 3 && abs(rgba[1] - g) <= 3 && abs(rgba[2] - b) <= 3;
}

/* A pixel format from wglChoosePixelFormatARB: RGB8, with alpha 8, depth 24 and
 * stencil 8 when full, and these extra attributes. */
static int choose(HDC hdc, const int *extra, BOOL pbuffer, BOOL full)
{
    int attribs[40], n = 0, format = 0;
    UINT count = 0;
    attribs[n++] = pbuffer ? WGL_DRAW_TO_PBUFFER_ARB : WGL_DRAW_TO_WINDOW_ARB; attribs[n++] = TRUE;
    attribs[n++] = WGL_SUPPORT_OPENGL_ARB; attribs[n++] = TRUE;
    if (!pbuffer) { attribs[n++] = WGL_DOUBLE_BUFFER_ARB; attribs[n++] = TRUE; }
    attribs[n++] = WGL_PIXEL_TYPE_ARB; attribs[n++] = WGL_TYPE_RGBA_ARB;
    attribs[n++] = WGL_RED_BITS_ARB; attribs[n++] = 8;
    attribs[n++] = WGL_GREEN_BITS_ARB; attribs[n++] = 8;
    attribs[n++] = WGL_BLUE_BITS_ARB; attribs[n++] = 8;
    if (full)
    {
        attribs[n++] = WGL_ALPHA_BITS_ARB; attribs[n++] = 8;
        attribs[n++] = WGL_DEPTH_BITS_ARB; attribs[n++] = 24;
        attribs[n++] = WGL_STENCIL_BITS_ARB; attribs[n++] = 8;
    }
    while (extra && *extra) attribs[n++] = *extra++;
    attribs[n] = 0;
    if (!choose_format(hdc, attribs, NULL, 1, &format, &count) || !count) return 0;
    return format;
}

static HGLRC make_context(HDC hdc, HGLRC share)
{
    static const int attribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB, 0,
    };
    return create_context(hdc, share, attribs);
}

static BOOL set_format(HDC hdc, int format)
{
    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(hdc, format, sizeof(pfd), &pfd);
    return SetPixelFormat(hdc, format, &pfd);
}

static void triangle(float angle, float r, float g, float b)
{
    glBegin(GL_TRIANGLES);
    glColor3f(r, g, b);
    for (int i = 0; i < 3; i++)
    {
        float a = angle + i * 2.0943951f;
        glVertex2f(0.6f * cosf(a), 0.6f * sinf(a));
    }
    glEnd();
}

static void quad(float x0, float y0, float x1, float y1, float z)
{
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z); glVertex3f(x1, y0, z); glVertex3f(x1, y1, z); glVertex3f(x0, y1, z);
    glEnd();
}

/* Frames per second over SECONDS of clears and swaps. */
static double rate(HDC hdc, double seconds)
{
    double start = now_s(), t;
    unsigned frames = 0;
    do
    {
        pump();
        glClearColor(0.1f * (frames % 8), 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SwapBuffers(hdc);
        frames++;
    } while ((t = now_s() - start) < seconds);
    return frames / t;
}

static void check_depth_stencil(HDC hdc, int width, int height)
{
    unsigned char left[4], right[4], centre[4];
    GLint depth = 0, stencil = 0;

    glGetIntegerv(GL_DEPTH_BITS, &depth);
    glGetIntegerv(GL_STENCIL_BITS, &stencil);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    /* Stencil 1 on the left half only; then red where stencil is 1. */
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    quad(-1.0f, -1.0f, 0.0f, 1.0f, 0.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glColor3f(1.0f, 0.0f, 0.0f);
    quad(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
    glDisable(GL_STENCIL_TEST);
    read_pixel(width / 4, height / 2, left);
    read_pixel(3 * width / 4, height / 2, right);

    /* A near blue quad, then a far yellow one over it: GL_LESS keeps blue. */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glColor3f(0.0f, 0.0f, 1.0f);
    quad(-0.5f, -0.5f, 0.5f, 0.5f, -0.5f);
    glColor3f(1.0f, 1.0f, 0.0f);
    quad(-0.8f, -0.8f, 0.8f, 0.8f, 0.5f);
    glDisable(GL_DEPTH_TEST);
    read_pixel(width / 2, height / 2, centre);
    SwapBuffers(hdc);

    check("depth-stencil", depth >= 24 && stencil >= 8 && is_colour(left, 255, 0, 0) && is_colour(right, 0, 0, 0) &&
          is_colour(centre, 0, 0, 255), "depth %d stencil %d, stencil left %u,%u,%u right %u,%u,%u, depth centre %u,%u,%u",
          depth, stencil, left[0], left[1], left[2], right[0], right[1], right[2], centre[0], centre[1], centre[2]);
}

static void check_pbuffer(HDC window_dc, HGLRC window_ctx)
{
    static const int size_attribs[] = {0};
    int format = choose(window_dc, NULL, TRUE, TRUE);
    HPBUFFERARB pbuffer;
    HDC dc;
    HGLRC ctx, plain_a, plain_b;
    GLuint texture;
    unsigned char rgba[4] = {0};
    GLboolean shared, shared_lists;

    if (!format) { check("pbuffer", FALSE, "no pbuffer pixel format"); return; }
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glFinish();
    if (!(pbuffer = create_pbuffer(window_dc, format, 256, 256, size_attribs)))
    {
        check("pbuffer", FALSE, "wglCreatePbufferARB failed: %lu", GetLastError());
        return;
    }
    dc = get_pbuffer_dc(pbuffer);
    if (!(ctx = make_context(dc, window_ctx)) || !wglMakeCurrent(dc, ctx))
    {
        check("pbuffer", FALSE, "no pbuffer context: %lu", GetLastError());
        return;
    }
    glViewport(0, 0, 256, 256);
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    glReadBuffer(GL_FRONT);
    glReadPixels(128, 128, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    shared = glIsTexture(texture);
    check("pbuffer", is_colour(rgba, 255, 0, 255) && glGetError() == GL_NO_ERROR,
          "format %d 256x256, read %u,%u,%u", format, rgba[0], rgba[1], rgba[2]);
    check("share-context", shared, "the window context's texture %u %s in the pbuffer's shared context",
          texture, shared ? "is" : "is NOT");
    wglMakeCurrent(window_dc, window_ctx);
    wglDeleteContext(ctx);
    release_pbuffer_dc(pbuffer, dc);
    destroy_pbuffer(pbuffer);

    /* wglShareLists between two plain contexts, before either has objects. */
    plain_a = wglCreateContext(window_dc);
    plain_b = wglCreateContext(window_dc);
    shared_lists = plain_a && plain_b && wglShareLists(plain_a, plain_b);
    if (shared_lists && wglMakeCurrent(window_dc, plain_a))
    {
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glFinish();
        wglMakeCurrent(window_dc, plain_b);
        shared_lists = glIsTexture(t);
    }
    check("share-lists", shared_lists, "wglShareLists %s", shared_lists ? "shares textures" : "failed");
    wglMakeCurrent(window_dc, window_ctx);
    if (plain_a) wglDeleteContext(plain_a);
    if (plain_b) wglDeleteContext(plain_b);
}

/* sRGB or 4x MSAA on the current (game) window. */
static void check_mode(const char *mode, HDC hdc, int format, int width, int height)
{
    unsigned char rgba[4] = {0};
    GLint samples = 0, srgb_capable = 0;

    glGetIntegerv(GL_SAMPLES, &samples);
    glViewport(0, 0, width, height);
    if (!strcmp(mode, "srgb"))
    {
        glGetIntegerv(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &srgb_capable);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        read_pixel(width / 2, height / 2, rgba);
        glDisable(GL_FRAMEBUFFER_SRGB);
        /* linear 0.5 is sRGB 188 */
        check("srgb", abs(rgba[0] - 188) <= 4, "format %d, capable %d, linear 0.5 reads %u (sRGB 188, linear 128)",
              format, srgb_capable, rgba[0]);
    }
    else
    {
        /* A white triangle edge across black: a resolved edge pixel is grey. */
        unsigned grey = 0, white = 0;
        glEnable(GL_MULTISAMPLE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_TRIANGLES);
        glVertex2f(-1.0f, -1.0f); glVertex2f(1.0f, -1.0f); glVertex2f(-1.0f, 0.93f);
        glEnd();
        glReadBuffer(GL_BACK);
        for (int x = 0; x < width && !grey; x += 7)
        {
            unsigned char column[4 * 256];
            int rows = height < 256 ? height : 256;
            glReadPixels(x, height / 2 - rows / 2, 1, rows, GL_RGBA, GL_UNSIGNED_BYTE, column);
            for (int y = 0; y < rows; y++)
            {
                if (column[4 * y] >= 235) white++;
                else if (column[4 * y] > 20 && !grey) grey = column[4 * y];
            }
        }
        check("msaa", samples >= 4 && grey && white, "format %d, GL_SAMPLES %d, an edge pixel %u (resolved: neither "
              "0 nor 255), %u white pixels read", format, samples, grey, white);
    }
    SwapBuffers(hdc);
}

/* A second visible window with its own context: its first swap takes the
 * game layer from the first window, which still presents; it reads back,
 * presents for a second and goes. */
static void check_second_window(int width, int height, HDC main_dc, HGLRC main_ctx)
{
    HWND hwnd = make_window("wgl second", TRUE, width, height);
    HDC dc = GetDC(hwnd);
    int format = choose(dc, NULL, FALSE, FALSE);
    HGLRC ctx;
    unsigned char rgba[4] = {0};
    double start;

    if (!format || !set_format(dc, format) || !(ctx = make_context(dc, NULL)) || !wglMakeCurrent(dc, ctx))
    {
        check("second-window", FALSE, "format %d, error %lu", format, GetLastError());
        if (hwnd) DestroyWindow(hwnd);
        wglMakeCurrent(main_dc, main_ctx);
        return;
    }
    glClear(GL_COLOR_BUFFER_BIT);
    SwapBuffers(dc);
    glViewport(0, 0, width, height);
    glClearColor(0.4f, 0.1f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    read_pixel(width / 2, height / 2, rgba);
    check("second-window", is_colour(rgba, 102, 26, 102) && glGetError() == GL_NO_ERROR,
          "format %d, after its first swap reads %u,%u,%u", format, rgba[0], rgba[1], rgba[2]);
    start = now_s();
    while (now_s() - start < 1.0)
    {
        pump();
        glClearColor(0.4f, 0.1f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        triangle((float)(now_s() - start), 1.0f, 1.0f, 1.0f);
        SwapBuffers(dc);
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(ctx);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);
    wglMakeCurrent(main_dc, main_ctx);
}

static void check_border(int width, int height)
{
    static const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    static const unsigned char texel[4] = {255, 255, 255, 255};
    unsigned char rgba[4] = {0};
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, blue);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);   /* texture coordinates 2..3: all border */
    glTexCoord2f(2.0f, 2.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(3.0f, 2.0f); glVertex2f(1.0f, -1.0f);
    glTexCoord2f(3.0f, 3.0f); glVertex2f(1.0f, 1.0f);
    glTexCoord2f(2.0f, 3.0f); glVertex2f(-1.0f, 1.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    read_pixel(width / 2, height / 2, rgba);
    glDeleteTextures(1, &texture);
    check("border", is_colour(rgba, 0, 0, 255), "GL_CLAMP_TO_BORDER blue reads %u,%u,%u,%u", rgba[0], rgba[1], rgba[2], rgba[3]);
}

static void draw_frames(HDC hdc, int width, int height, double seconds, float r, float g, float b)
{
    double start = now_s();
    while (now_s() - start < seconds)
    {
        pump();
        glViewport(0, 0, width, height);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        triangle((float)(now_s() - start), 1.0f, 0.85f, 0.1f);
        SwapBuffers(hdc);
    }
}

static void check_resize(HWND hwnd, HDC hdc, int width, int height)
{
    RECT rect;
    unsigned char inside[4] = {0};
    int w = width / 2, h = height / 2;

    SetWindowPos(hwnd, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    pump();
    GetClientRect(hwnd, &rect);
    draw_frames(hdc, w, h, 1.0, 0.3f, 0.0f, 0.0f);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    read_pixel(w - 2, h - 2, inside);
    SwapBuffers(hdc);
    check("resize", rect.right == w && rect.bottom == h && is_colour(inside, 0, 255, 0) && glGetError() == GL_NO_ERROR,
          "client %ldx%ld (asked %dx%d), corner reads %u,%u,%u", rect.right, rect.bottom, w, h, inside[0], inside[1], inside[2]);
    SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    pump();
    draw_frames(hdc, width, height, 0.5, 0.0f, 0.35f, 0.4f);
}

/* A game that sets a smaller display mode and fills it: the window, and so its
 * swap chain, is the mode's size, whatever the screen reports afterwards. */
static void check_modeset(HWND hwnd, HDC hdc, int width, int height)
{
    DEVMODEA mode = {.dmSize = sizeof(mode)}, pick = {0};
    RECT rect;
    LONG ret;
    unsigned char rgba[4] = {0};
    int w, h;

    for (DWORD i = 0; EnumDisplaySettingsA(NULL, i, &mode); i++)
        if (mode.dmPelsWidth < (DWORD)width && mode.dmPelsHeight < (DWORD)height && mode.dmPelsWidth >= 640 &&
            mode.dmPelsWidth * mode.dmPelsHeight > pick.dmPelsWidth * pick.dmPelsHeight)
            pick = mode;   /* the largest mode smaller than the screen */
    if (!pick.dmSize) { probe_log("CHECK modeset: skipped, no smaller display mode listed\n"); return; }
    pick.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    ret = ChangeDisplaySettingsA(&pick, CDS_FULLSCREEN);
    w = pick.dmPelsWidth;
    h = pick.dmPelsHeight;
    SetWindowPos(hwnd, NULL, 0, 0, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    pump();
    GetClientRect(hwnd, &rect);
    draw_frames(hdc, w, h, 2.0, 0.0f, 0.0f, 0.3f);
    glViewport(0, 0, w, h);
    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    read_pixel(w - 2, h - 2, rgba);
    SwapBuffers(hdc);
    check("modeset", ret == DISP_CHANGE_SUCCESSFUL && rect.right == w && rect.bottom == h && is_colour(rgba, 255, 255, 0),
          "ChangeDisplaySettings %dx%d -> %ld, window %ldx%ld, screen then %dx%d, corner reads %u,%u,%u", w, h, ret,
          rect.right, rect.bottom, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), rgba[0], rgba[1], rgba[2]);
    ChangeDisplaySettingsA(NULL, 0);
    SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    pump();
    probe_log("modeset: restored, screen %dx%d\n", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
}

#define LOAD(var, type, name) (var = (type)wglGetProcAddress(name))

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show)
{
    int seconds = cmdline && *cmdline ? atoi(cmdline) : 20;
    const char *mode = cmdline ? strchr(cmdline, ' ') : NULL;
    static const int srgb_attribs[] = {WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, TRUE, 0};
    static const int msaa_attribs[] = {WGL_SAMPLE_BUFFERS_ARB, 1, WGL_SAMPLES_ARB, 4, 0};
    const int *extra = NULL;
    int width = GetSystemMetrics(SM_CXSCREEN), height = GetSystemMetrics(SM_CYSCREEN);
    PIXELFORMATDESCRIPTOR pfd = {0};
    WNDCLASSA wc = {0};
    HWND dummy, hwnd;
    HDC dummy_dc, hdc;
    HGLRC dummy_ctx, ctx;
    int format;
    const char *ext;
    unsigned char rgba[4] = {0};
    GLenum error;
    double start, last, t;
    unsigned frames = 0, logged = 0;

    if (seconds <= 0) seconds = 20;
    while (mode && *mode == ' ') mode++;
    if (mode && !strcmp(mode, "srgb")) extra = srgb_attribs;
    else if (mode && !strcmp(mode, "msaa")) extra = msaa_attribs;
    else mode = NULL;
    probe_log("start: %d s, screen %dx%d, mode %s\n", seconds, width, height, mode ? mode : "plain");
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.lpszClassName = "wgl";
    RegisterClassA(&wc);

    /* The dummy: ChoosePixelFormat, a legacy context, the extension list. */
    dummy = make_window("wgl dummy", FALSE, 64, 64);
    dummy_dc = GetDC(dummy);
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;
    format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!format || !SetPixelFormat(dummy_dc, format, &pfd) || !(dummy_ctx = wglCreateContext(dummy_dc)) ||
        !wglMakeCurrent(dummy_dc, dummy_ctx))
    {
        probe_log("no legacy context on the dummy window: format %d, error %lu\n", format, GetLastError());
        return 100;
    }
    LOAD(get_extensions, PFNWGLGETEXTENSIONSSTRINGARBPROC, "wglGetExtensionsStringARB");
    LOAD(choose_format, PFNWGLCHOOSEPIXELFORMATARBPROC, "wglChoosePixelFormatARB");
    LOAD(create_context, PFNWGLCREATECONTEXTATTRIBSARBPROC, "wglCreateContextAttribsARB");
    LOAD(swap_interval, PFNWGLSWAPINTERVALEXTPROC, "wglSwapIntervalEXT");
    LOAD(get_swap_interval, PFNWGLGETSWAPINTERVALEXTPROC, "wglGetSwapIntervalEXT");
    LOAD(create_pbuffer, PFNWGLCREATEPBUFFERARBPROC, "wglCreatePbufferARB");
    LOAD(get_pbuffer_dc, PFNWGLGETPBUFFERDCARBPROC, "wglGetPbufferDCARB");
    LOAD(release_pbuffer_dc, PFNWGLRELEASEPBUFFERDCARBPROC, "wglReleasePbufferDCARB");
    LOAD(destroy_pbuffer, PFNWGLDESTROYPBUFFERARBPROC, "wglDestroyPbufferARB");
    ext = get_extensions ? get_extensions(dummy_dc) : NULL;
    probe_log("WGL extensions: %s\n", ext ? ext : "(none)");
    if (!choose_format || !create_context || !swap_interval || !get_swap_interval || !create_pbuffer ||
        !get_pbuffer_dc || !release_pbuffer_dc || !destroy_pbuffer)
    {
        check("dummy", FALSE, "a WGL extension entry point is missing");
        return 100;
    }
    /* A dummy that swaps (some engines present a frame from it). */
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    check("dummy", SwapBuffers(dummy_dc), "format %d, swapped once, kept alive", format);
    wglMakeCurrent(NULL, NULL);

    /* The game window. */
    hwnd = make_window("wgl", TRUE, width, height);
    hdc = GetDC(hwnd);
    if (!(format = choose(hdc, extra, FALSE, TRUE)) || !set_format(hdc, format) || !(ctx = make_context(hdc, NULL)) ||
        !wglMakeCurrent(hdc, ctx))
    {
        probe_log("no GL 3.3 compatibility context on the window: format %d, error %lu\n", format, GetLastError());
        return 100;
    }
    probe_log("window: format %d, GL_VERSION %s, GL_RENDERER %s\n", format, (const char *)glGetString(GL_VERSION),
              (const char *)glGetString(GL_RENDERER));
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    read_pixel(width / 2, height / 2, rgba);
    error = glGetError();
    check("first-frame", error == GL_NO_ERROR && is_colour(rgba, 0, 255, 0),
          "before the first swap: glGetError %#x, centre reads %u,%u,%u", error, rgba[0], rgba[1], rgba[2]);
    SwapBuffers(hdc);
    wglDeleteContext(dummy_ctx);
    ReleaseDC(dummy, dummy_dc);
    DestroyWindow(dummy);

    check_depth_stencil(hdc, width, height);
    if (mode) check_mode(mode, hdc, format, width, height);

    {
        double one, zero;
        swap_interval(1);
        one = rate(hdc, 3.0);
        probe_log("interval 1: %d, %.1f fps\n", get_swap_interval(), one);
        swap_interval(0);
        zero = rate(hdc, 3.0);
        probe_log("interval 0: %d, %.1f fps\n", get_swap_interval(), zero);
        swap_interval(1);
        check("interval", one > 20.0 && zero > 20.0, "interval 1 %.1f fps, interval 0 %.1f fps", one, zero);
    }

    check_pbuffer(hdc, ctx);
    check_second_window(width, height, hdc, ctx);
    draw_frames(hdc, width, height, 1.0, 0.0f, 0.35f, 0.4f);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    read_pixel(width / 2, height / 2, rgba);
    SwapBuffers(hdc);
    check("hand-back", is_colour(rgba, 0, 0, 255) && glGetError() == GL_NO_ERROR,
          "the first window draws again after the second: %u,%u,%u", rgba[0], rgba[1], rgba[2]);
    check_border(width, height);
    SwapBuffers(hdc);
    check_resize(hwnd, hdc, width, height);
    check_modeset(hwnd, hdc, width, height);
    probe_log("checks done: %d failed\n", failures);

    swap_interval(1);
    start = last = now_s();
    do
    {
        pump();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.35f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        triangle((float)(now_s() - start), 1.0f, 0.85f, 0.1f);
        SwapBuffers(hdc);
        frames++;
        t = now_s();
        if (t - last >= 5.0)
        {
            probe_log("scene: %.1f fps over %.1f s\n", (frames - logged) / (t - last), t - last);
            logged = frames;
            last = t;
        }
    } while (t - start < seconds);
    probe_log("done: %u frames in %.1f s, %.1f fps, %d failed\n", frames, t - start, frames / (t - start), failures);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(ctx);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return failures;
}
