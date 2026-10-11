/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * gl21: the smallest Windows OpenGL program that shows whether WGL and GL 2.1
 * work (docs/plans/2026-10-11-opengl-zink.md, PLA-162). It makes a window the
 * size of the screen, sets a double-buffered RGBA pixel format with
 * ChoosePixelFormat/SetPixelFormat, makes a wglCreateContext context current,
 * logs GL_VERSION, GL_RENDERER, GL_SHADING_LANGUAGE_VERSION and
 * GL_EXTENSIONS through OutputDebugString, and then for N seconds (default 20)
 * clears to dark teal and draws two triangles, SwapBuffers each frame:
 *
 *   left, fixed function: glBegin/glEnd with red, green and blue corners;
 *   right, a GLSL 1.20 program: a solid yellow triangle.
 *
 * Before the first frame it checks the paths whose pointers a WoW64 (i386)
 * caller's thunks convert, reading the result back (logged as
 * "gl21: CHECK <name>: ok|FAIL ..."): vertex arrays from client memory and
 * from a buffer object at an offset, indices from client memory and from an
 * element buffer, a generic attribute from a buffer, glReadPixels into client
 * memory and into a pixel-pack buffer read through glMapBuffer, a texture from
 * client memory and from a pixel-unpack buffer, and glGetPointerv.
 *
 * It logs the frame count and the rate at the end and exits 0; 1 when a step
 * or check fails (the log says which).
 *
 *    gl21.exe [SECONDS] [stress]
 *
 * stress: instead of the triangles, three phases of SECONDS/3 each (default
 * 30 s in all), each frame drawing 20,000 vertices: "immediate" with 20,000
 * glVertex3f calls between one glBegin/glEnd, "client-array" as one
 * glDrawArrays from client memory, "vbo" as one glDrawArrays from a buffer
 * object; each phase logs its mean frame time and the time its draw calls took
 * ("gl21: STRESS <phase>: ... ms per frame ..."), the measurement of the OpenGL
 * plan's chunk 3 (decision 0089).
 *
 * Built with llvm-mingw for x86-64 (gl21.exe) and i686 (gl21-i386.exe) by
 * tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <GL/gl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GL 2.0 entry points, from wglGetProcAddress. */
typedef char GLchar;
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
typedef GLuint (APIENTRY *create_shader_fn)(GLenum);
typedef void (APIENTRY *shader_source_fn)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void (APIENTRY *compile_shader_fn)(GLuint);
typedef void (APIENTRY *get_shaderiv_fn)(GLuint, GLenum, GLint *);
typedef void (APIENTRY *get_info_log_fn)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint (APIENTRY *create_program_fn)(void);
typedef void (APIENTRY *attach_shader_fn)(GLuint, GLuint);
typedef void (APIENTRY *link_program_fn)(GLuint);
typedef void (APIENTRY *use_program_fn)(GLuint);

/* GL 1.5/2.0/2.1 buffer objects and generic attributes. */
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_READ_ONLY 0x88B8
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STATIC_DRAW 0x88E4
typedef void (APIENTRY *gen_buffers_fn)(GLsizei, GLuint *);
typedef void (APIENTRY *bind_buffer_fn)(GLenum, GLuint);
typedef void (APIENTRY *buffer_data_fn)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void *(APIENTRY *map_buffer_fn)(GLenum, GLenum);
typedef GLboolean (APIENTRY *unmap_buffer_fn)(GLenum);
typedef void (APIENTRY *vertex_attrib_pointer_fn)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void (APIENTRY *attrib_array_fn)(GLuint);

static gen_buffers_fn p_glGenBuffers;
static bind_buffer_fn p_glBindBuffer;
static buffer_data_fn p_glBufferData;
static map_buffer_fn p_glMapBuffer;
static unmap_buffer_fn p_glUnmapBuffer;
static vertex_attrib_pointer_fn p_glVertexAttribPointer;
static attrib_array_fn p_glEnableVertexAttribArray;
static attrib_array_fn p_glDisableVertexAttribArray;

static void probe_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    int n = snprintf(line, sizeof(line), "gl21: ");
    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
}

/* GL_EXTENSIONS can be thousands of characters: log it in pieces. */
static void log_long(const char *name, const char *text)
{
    size_t len = text ? strlen(text) : 0;
    probe_log("%s (%u characters):\n", name, (unsigned)len);
    for (size_t at = 0; at < len; at += 900)
        probe_log("  %.900s\n", text + at);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static GLuint compile(create_shader_fn create_shader, shader_source_fn shader_source,
                      compile_shader_fn compile_shader, get_shaderiv_fn get_shaderiv,
                      get_info_log_fn get_info_log, GLenum type, const char *src)
{
    GLuint shader = create_shader(type);
    GLint ok = 0;
    char log[1024] = "";
    shader_source(shader, 1, &src, NULL);
    compile_shader(shader);
    get_shaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        get_info_log(shader, sizeof(log), NULL, log);
        probe_log("GLSL compile failed: %s\n", log);
        return 0;
    }
    return shader;
}

static const char vertex_src[] =
    "#version 120\n"
    "void main() {\n"
    "    gl_Position = gl_Vertex;\n"
    "}\n";
static const char fragment_src[] =
    "#version 120\n"
    "void main() {\n"
    "    gl_FragColor = vec4(1.0, 0.85, 0.1, 1.0);\n"
    "}\n";

static double now_ms(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER count;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return count.QuadPart * 1000.0 / freq.QuadPart;
}

static int check_failures;

static void check(const char *name, int ok, const char *fmt, ...)
{
    char detail[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);
    probe_log("CHECK %s: %s %s\n", name, ok ? "ok" : "FAIL", detail);
    if (!ok) check_failures++;
}

/* A quad (two triangles) over [x0,x1]x[y0,y1] in normalised coordinates. */
static void quad(GLfloat *v, float x0, float y0, float x1, float y1)
{
    const GLfloat q[18] = { x0, y0, 0, x1, y0, 0, x1, y1, 0, x0, y0, 0, x1, y1, 0, x0, y1, 0 };
    memcpy(v, q, sizeof(q));
}

/* The four corners of a quad, for indexed draws. */
static void corners(GLfloat *v, float x0, float y0, float x1, float y1)
{
    const GLfloat q[12] = { x0, y0, 0, x1, y0, 0, x1, y1, 0, x0, y1, 0 };
    memcpy(v, q, sizeof(q));
}

/* The window pixel at the centre of a cell of a 4x2 grid over the screen. */
static void cell_box(int cell, float *x0, float *y0, float *x1, float *y1)
{
    float cx = -1.0f + 0.5f * (cell % 4), cy = cell < 4 ? 0.0f : -1.0f;
    *x0 = cx + 0.1f; *x1 = cx + 0.4f; *y0 = cy + 0.2f; *y1 = cy + 0.8f;
}

static void cell_pixel(int cell, int width, int height, int *x, int *y)
{
    float x0, y0, x1, y1;
    cell_box(cell, &x0, &y0, &x1, &y1);
    *x = (int)(((x0 + x1) / 2 + 1) / 2 * width);
    *y = (int)(((y0 + y1) / 2 + 1) / 2 * height);
}

static int same_rgb(const GLubyte *px, int r, int g, int b)
{
    return abs(px[0] - r) < 8 && abs(px[1] - g) < 8 && abs(px[2] - b) < 8;
}

/* The pointer paths a WoW64 caller's thunks convert, each read back from the
 * back buffer: what the thunks get wrong shows as a wrong colour, a GL error
 * or a crash. Offsets into buffers are not 0, so an offset taken for a client
 * pointer (or the reverse) cannot pass by being NULL. */
static void run_checks(int width, int height, use_program_fn use_program, GLuint program)
{
    enum { CLIENT_ARRAY, VBO_ARRAY, CLIENT_INDICES, VBO_INDICES, ATTRIB_VBO, TEX_CLIENT, TEX_PBO, CELLS };
    static const GLubyte expect[CELLS][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {255, 217, 25}, {255, 0, 255}, {0, 255, 255} };
    static const char *names[CELLS] = {
        "client-array", "vbo-array", "client-indices", "vbo-indices", "attrib-vbo", "tex-client", "tex-pbo" };
    static const GLushort indices[6] = { 0, 1, 2, 0, 2, 3 };
    static const GLubyte magenta[4 * 4] = { 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255 };
    static const GLubyte cyan[4 * 4] = { 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255 };
    const GLintptr vbo_offset = 256, index_offset = 64, pack_offset = 128, unpack_offset = 32;
    GLfloat client[CELLS][18], buffer_data[64 + 18 * 3];
    GLubyte pixels[CELLS][4], *mapped;
    GLuint vbo, ebo, pbo, upbo, tex;
    GLvoid *pointer = NULL;
    GLenum error;
    char *staging;
    int i, x, y;

    for (i = 0; i < CELLS; i++)
    {
        float x0, y0, x1, y1;
        cell_box(i, &x0, &y0, &x1, &y1);
        if (i == CLIENT_INDICES || i == VBO_INDICES) corners(client[i], x0, y0, x1, y1);
        else quad(client[i], x0, y0, x1, y1);
    }

    glViewport(0, 0, width, height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);

    /* vertices from client memory */
    glColor3ub(255, 0, 0);
    glVertexPointer(3, GL_FLOAT, 0, client[CLIENT_ARRAY]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glVertexPointer(3, GL_FLOAT, 0, client[CLIENT_ARRAY]);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
    check("get-pointer-client", pointer == (GLvoid *)client[CLIENT_ARRAY], "%p for %p", pointer, (void *)client[CLIENT_ARRAY]);

    /* vertices from a buffer at an offset: VBO_ARRAY's quad, then the attribute's, then the indexed corners */
    staging = calloc(1, vbo_offset + sizeof(client[0]) * 3);
    memcpy(staging + vbo_offset, client[VBO_ARRAY], sizeof(client[0]));
    memcpy(staging + vbo_offset + sizeof(client[0]), client[ATTRIB_VBO], sizeof(client[0]));
    memcpy(staging + vbo_offset + 2 * sizeof(client[0]), client[VBO_INDICES], 12 * sizeof(GLfloat));
    p_glGenBuffers(1, &vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    p_glBufferData(GL_ARRAY_BUFFER, vbo_offset + sizeof(client[0]) * 3, staging, GL_STATIC_DRAW);
    free(staging);
    glColor3ub(0, 255, 0);
    glVertexPointer(3, GL_FLOAT, 0, (const void *)vbo_offset);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
    check("get-pointer-offset", pointer == (GLvoid *)vbo_offset, "%p for offset %#x", pointer, (unsigned)vbo_offset);
    p_glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* indices from client memory, vertices from client memory */
    glColor3ub(0, 0, 255);
    glVertexPointer(3, GL_FLOAT, 0, client[CLIENT_INDICES]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    /* indices from an element buffer at an offset, vertices from the vbo */
    memset(buffer_data, 0, sizeof(buffer_data));
    memcpy((char *)buffer_data + index_offset, indices, sizeof(indices));
    p_glGenBuffers(1, &ebo);
    p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    p_glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_offset + sizeof(indices), buffer_data, GL_STATIC_DRAW);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glColor3ub(255, 255, 255);
    glVertexPointer(3, GL_FLOAT, 0, (const void *)(vbo_offset + 2 * sizeof(client[0])));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const void *)index_offset);
    p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    /* a generic attribute from the vbo at an offset, through the GLSL program (solid yellow):
     * attribute 0 is gl_Vertex in a compatibility context */
    use_program(program);
    p_glEnableVertexAttribArray(0);
    p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)(vbo_offset + sizeof(client[0])));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    p_glDisableVertexAttribArray(0);
    use_program(0);
    p_glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* textures from client memory and from an unpack buffer at an offset */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta);
    glEnable(GL_TEXTURE_2D);
    glColor3ub(255, 255, 255);
    glBegin(GL_QUADS);
    {
        float x0, y0, x1, y1;
        cell_box(TEX_CLIENT, &x0, &y0, &x1, &y1);
        glTexCoord2f(0, 0); glVertex2f(x0, y0);
        glTexCoord2f(1, 0); glVertex2f(x1, y0);
        glTexCoord2f(1, 1); glVertex2f(x1, y1);
        glTexCoord2f(0, 1); glVertex2f(x0, y1);
    }
    glEnd();
    memset(buffer_data, 0, sizeof(buffer_data));
    memcpy((char *)buffer_data + unpack_offset, cyan, sizeof(cyan));
    p_glGenBuffers(1, &upbo);
    p_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, upbo);
    p_glBufferData(GL_PIXEL_UNPACK_BUFFER, unpack_offset + sizeof(cyan), buffer_data, GL_STREAM_DRAW);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, (const void *)unpack_offset);
    p_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBegin(GL_QUADS);
    {
        float x0, y0, x1, y1;
        cell_box(TEX_PBO, &x0, &y0, &x1, &y1);
        glTexCoord2f(0, 0); glVertex2f(x0, y0);
        glTexCoord2f(1, 0); glVertex2f(x1, y0);
        glTexCoord2f(1, 1); glVertex2f(x1, y1);
        glTexCoord2f(0, 1); glVertex2f(x0, y1);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tex);
    glFinish();
    error = glGetError();
    check("draws", error == GL_NO_ERROR, "glGetError %#x", error);

    /* read back into client memory */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    for (i = 0; i < CELLS; i++)
    {
        cell_pixel(i, width, height, &x, &y);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[i]);
        check(names[i], same_rgb(pixels[i], expect[i][0], expect[i][1], expect[i][2]),
              "reads %u,%u,%u (want %u,%u,%u)", pixels[i][0], pixels[i][1], pixels[i][2],
              expect[i][0], expect[i][1], expect[i][2]);
    }

    /* read back into a pack buffer at an offset, through glMapBuffer */
    p_glGenBuffers(1, &pbo);
    p_glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    p_glBufferData(GL_PIXEL_PACK_BUFFER, pack_offset + 4, NULL, GL_STREAM_READ);
    cell_pixel(VBO_ARRAY, width, height, &x, &y);
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)pack_offset);
    mapped = p_glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    check("read-pbo-map", mapped && same_rgb(mapped + pack_offset, 0, 255, 0), "map %p reads %u,%u,%u",
          (void *)mapped, mapped ? mapped[pack_offset] : 0, mapped ? mapped[pack_offset + 1] : 0,
          mapped ? mapped[pack_offset + 2] : 0);
    if (mapped) p_glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    p_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    error = glGetError();
    check("read-backs", error == GL_NO_ERROR, "glGetError %#x", error);
}

/* 20,000 vertices of small triangles over the left part of the screen */
#define STRESS_VERTICES 20000

static void stress_vertices(GLfloat *v)
{
    for (int i = 0; i < STRESS_VERTICES / 3 + 1; i++)
    {
        float x = -0.95f + 0.015f * (i % 60), y = -0.9f + 0.016f * (i / 60);
        const GLfloat t[9] = { x, y, 0, x + 0.012f, y, 0, x, y + 0.012f, 0 };
        for (int k = 0; k < 9 && i * 9 + k < STRESS_VERTICES * 3; k++) v[i * 9 + k] = t[k];
    }
}

/* One phase of the stress: frames for SECONDS, each a clear, the 20,000
 * vertices drawn the phase's way, SwapBuffers. */
static int stress_phase(HDC hdc, const char *phase, int seconds, const GLfloat *v, GLuint vbo)
{
    double start = now_ms(), end = start + seconds * 1000.0, submit = 0, t0, t;
    unsigned frames = 0;
    MSG msg;

    do
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        t0 = now_ms();
        if (!strcmp(phase, "immediate"))
        {
            glColor3f(1.0f, 0.6f, 0.1f);
            glBegin(GL_TRIANGLES);
            for (int i = 0; i < STRESS_VERTICES; i++) glVertex3f(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]);
            glEnd();
        }
        else
        {
            glColor3f(strcmp(phase, "vbo") ? 0.2f : 0.3f, 0.8f, strcmp(phase, "vbo") ? 0.3f : 1.0f);
            glEnableClientState(GL_VERTEX_ARRAY);
            if (vbo) p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexPointer(3, GL_FLOAT, 0, vbo ? NULL : v);
            glDrawArrays(GL_TRIANGLES, 0, STRESS_VERTICES);
            if (vbo) p_glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDisableClientState(GL_VERTEX_ARRAY);
        }
        submit += now_ms() - t0;
        SwapBuffers(hdc);
        frames++;
        t = now_ms();
    } while (t < end);
    probe_log("STRESS %s: %u frames in %.1f s, %.2f ms per frame, draw calls %.2f ms per frame, glGetError %#x\n",
              phase, frames, (t - start) / 1000.0, (t - start) / frames, submit / frames, glGetError());
    return frames;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show)
{
    int seconds = cmdline ? atoi(cmdline + strspn(cmdline, "\" ")) : 0;
    int stress = cmdline && strstr(cmdline, "stress") != NULL;
    int width = GetSystemMetrics(SM_CXSCREEN), height = GetSystemMetrics(SM_CYSCREEN);
    PIXELFORMATDESCRIPTOR pfd = {0};
    WNDCLASSA wc = {0};
    HWND hwnd;
    HDC hdc;
    HGLRC ctx;
    int format;
    GLuint program = 0, vs, fs;
    GLint linked = 0;
    DWORD start, now;
    unsigned frames = 0;
    MSG msg;

    if (seconds <= 0) seconds = stress ? 30 : 20;
    probe_log("start: %d s%s, screen %dx%d, %d-bit\n", seconds, stress ? " stress" : "", width, height,
              (int)sizeof(void *) * 8);

    wc.style = CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.lpszClassName = "gl21";
    RegisterClassA(&wc);
    hwnd = CreateWindowExA(0, "gl21", "gl21", WS_POPUP | WS_VISIBLE, 0, 0, width, height,
                           NULL, NULL, instance, NULL);
    if (!hwnd) { probe_log("CreateWindow failed: %lu\n", GetLastError()); return 1; }
    hdc = GetDC(hwnd);

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    if (!(format = ChoosePixelFormat(hdc, &pfd))) { probe_log("ChoosePixelFormat failed: %lu\n", GetLastError()); return 1; }
    DescribePixelFormat(hdc, format, sizeof(pfd), &pfd);
    probe_log("pixel format %d: flags %#lx, colour %u, depth %u, stencil %u\n", format, pfd.dwFlags,
         pfd.cColorBits, pfd.cDepthBits, pfd.cStencilBits);
    if (!SetPixelFormat(hdc, format, &pfd)) { probe_log("SetPixelFormat failed: %lu\n", GetLastError()); return 1; }
    if (!(ctx = wglCreateContext(hdc))) { probe_log("wglCreateContext failed: %lu\n", GetLastError()); return 1; }
    if (!wglMakeCurrent(hdc, ctx)) { probe_log("wglMakeCurrent failed: %lu\n", GetLastError()); return 1; }

    probe_log("GL_VERSION %s\n", (const char *)glGetString(GL_VERSION));
    probe_log("GL_RENDERER %s\n", (const char *)glGetString(GL_RENDERER));
    probe_log("GL_VENDOR %s\n", (const char *)glGetString(GL_VENDOR));
    probe_log("GL_SHADING_LANGUAGE_VERSION %s\n", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
    log_long("GL_EXTENSIONS", (const char *)glGetString(GL_EXTENSIONS));

    {
        create_shader_fn create_shader = (create_shader_fn)wglGetProcAddress("glCreateShader");
        shader_source_fn shader_source = (shader_source_fn)wglGetProcAddress("glShaderSource");
        compile_shader_fn compile_shader = (compile_shader_fn)wglGetProcAddress("glCompileShader");
        get_shaderiv_fn get_shaderiv = (get_shaderiv_fn)wglGetProcAddress("glGetShaderiv");
        get_info_log_fn get_shader_log = (get_info_log_fn)wglGetProcAddress("glGetShaderInfoLog");
        create_program_fn create_program = (create_program_fn)wglGetProcAddress("glCreateProgram");
        attach_shader_fn attach_shader = (attach_shader_fn)wglGetProcAddress("glAttachShader");
        link_program_fn link_program = (link_program_fn)wglGetProcAddress("glLinkProgram");
        get_shaderiv_fn get_programiv = (get_shaderiv_fn)wglGetProcAddress("glGetProgramiv");
        get_info_log_fn get_program_log = (get_info_log_fn)wglGetProcAddress("glGetProgramInfoLog");
        use_program_fn use_program = (use_program_fn)wglGetProcAddress("glUseProgram");

        p_glGenBuffers = (gen_buffers_fn)wglGetProcAddress("glGenBuffers");
        p_glBindBuffer = (bind_buffer_fn)wglGetProcAddress("glBindBuffer");
        p_glBufferData = (buffer_data_fn)wglGetProcAddress("glBufferData");
        p_glMapBuffer = (map_buffer_fn)wglGetProcAddress("glMapBuffer");
        p_glUnmapBuffer = (unmap_buffer_fn)wglGetProcAddress("glUnmapBuffer");
        p_glVertexAttribPointer = (vertex_attrib_pointer_fn)wglGetProcAddress("glVertexAttribPointer");
        p_glEnableVertexAttribArray = (attrib_array_fn)wglGetProcAddress("glEnableVertexAttribArray");
        p_glDisableVertexAttribArray = (attrib_array_fn)wglGetProcAddress("glDisableVertexAttribArray");
        if (!p_glGenBuffers || !p_glBindBuffer || !p_glBufferData || !p_glMapBuffer || !p_glUnmapBuffer ||
            !p_glVertexAttribPointer || !p_glEnableVertexAttribArray || !p_glDisableVertexAttribArray)
        {
            probe_log("no GL 1.5/2.0 buffer or attribute entry points\n");
            return 1;
        }

        if (!create_shader || !shader_source || !compile_shader || !get_shaderiv || !get_shader_log ||
            !create_program || !attach_shader || !link_program || !get_programiv || !get_program_log || !use_program)
        {
            probe_log("no GL 2.0 shader entry points\n");
            return 1;
        }
        vs = compile(create_shader, shader_source, compile_shader, get_shaderiv, get_shader_log, GL_VERTEX_SHADER, vertex_src);
        fs = compile(create_shader, shader_source, compile_shader, get_shaderiv, get_shader_log, GL_FRAGMENT_SHADER, fragment_src);
        if (!vs || !fs) return 1;
        program = create_program();
        attach_shader(program, vs);
        attach_shader(program, fs);
        link_program(program);
        get_programiv(program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            char log[1024] = "";
            get_program_log(program, sizeof(log), NULL, log);
            probe_log("GLSL link failed: %s\n", log);
            return 1;
        }
        probe_log("GLSL 1.20 program linked\n");

        run_checks(width, height, use_program, program);
        probe_log("checks: %d failed\n", check_failures);

        if (stress)
        {
            GLfloat *v = malloc(STRESS_VERTICES * 3 * sizeof(*v));
            GLuint vbo;
            stress_vertices(v);
            p_glGenBuffers(1, &vbo);
            p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
            p_glBufferData(GL_ARRAY_BUFFER, STRESS_VERTICES * 3 * sizeof(*v), v, GL_STATIC_DRAW);
            p_glBindBuffer(GL_ARRAY_BUFFER, 0);
            glViewport(0, 0, width, height);
            stress_phase(hdc, "immediate", seconds / 3, v, 0);
            stress_phase(hdc, "client-array", seconds / 3, v, 0);
            stress_phase(hdc, "vbo", seconds / 3, v, vbo);
            free(v);
            probe_log("done: stress, %d checks failed\n", check_failures);
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(ctx);
            ReleaseDC(hwnd, hdc);
            DestroyWindow(hwnd);
            return check_failures ? 1 : 0;
        }

        start = GetTickCount();
        do
        {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            glViewport(0, 0, width, height);
            glClearColor(0.0f, 0.35f, 0.4f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            use_program(0);
            glBegin(GL_TRIANGLES);
            glColor3f(1.0f, 0.0f, 0.0f); glVertex2f(-0.9f, -0.7f);
            glColor3f(0.0f, 1.0f, 0.0f); glVertex2f(-0.1f, -0.7f);
            glColor3f(0.0f, 0.0f, 1.0f); glVertex2f(-0.5f, 0.7f);
            glEnd();

            use_program(program);
            glBegin(GL_TRIANGLES);
            glVertex2f(0.1f, -0.7f);
            glVertex2f(0.9f, -0.7f);
            glVertex2f(0.5f, 0.7f);
            glEnd();
            use_program(0);

            if (!frames)
            {
                GLenum error = glGetError();
                probe_log("first frame drawn (glGetError %#x)\n", error);
            }
            SwapBuffers(hdc);
            frames++;
            now = GetTickCount();
        } while (now - start < (DWORD)seconds * 1000);
    }

    probe_log("done: %u frames in %.1f s, %.1f fps, %d checks failed\n", frames, (now - start) / 1000.0,
         frames * 1000.0 / (now - start ? now - start : 1), check_failures);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(ctx);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return check_failures ? 1 : 0;
}
