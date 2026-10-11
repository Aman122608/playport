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
 * It logs the frame count and the rate at the end and exits 0; 1 when a step
 * fails (the log says which).
 *
 *    gl21.exe [SECONDS]
 *
 * Built with llvm-mingw by tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>

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

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show)
{
    int seconds = cmdline && *cmdline ? atoi(cmdline) : 20;
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

    if (seconds <= 0) seconds = 20;
    probe_log("start: %d s, screen %dx%d\n", seconds, width, height);

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

    probe_log("done: %u frames in %.1f s, %.1f fps\n", frames, (now - start) / 1000.0,
         frames * 1000.0 / (now - start ? now - start : 1));
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(ctx);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return 0;
}
