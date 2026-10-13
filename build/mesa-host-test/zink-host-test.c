/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Drive Zink on KosmicKrisp, both built for Linux with KosmicKrisp on the
 * mock Metal bridge (make-mock-bridge.py), through EGL and OpenGL the way a
 * Windows game reaches them through Wine: a compatibility context, a clear,
 * a fixed-function triangle, a GLSL 1.20 triangle, a glBlitFramebuffer and a
 * glReadPixels. Nothing is rendered; the mock completes every commit at once.
 * What this checks is that Zink accepts KosmicKrisp's device and features,
 * that the shaders Zink makes for these draws compile (NIR validation runs in
 * a debug build) and translate to MSL fully (the mock exits 3 otherwise), and
 * that nothing on the way crashes, asserts or raises a GL error.
 *
 *    zink-host-test
 *
 * The EGL display is the surfaceless platform's, as win32u makes it, and
 * eglInitialize reaches Zink the way it does on the phone: no DRM device has
 * a driver in this build (only Zink is built), so Zink runs without one
 * (patches/mesa) on the Vulkan device the loader finds, which run.sh limits
 * to the mock with VK_DRIVER_FILES. The surfaces are pbuffers: kopper's Metal
 * window surfaces need a CAMetalLayer, which only the phone has.
 */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAIL(...)                                                              \
   do {                                                                        \
      fprintf(stderr, "zink-host-test: " __VA_ARGS__);                         \
      fputc('\n', stderr);                                                     \
      exit(1);                                                                 \
   } while (0)

/* The GL entry points, from eglGetProcAddress as win32u takes them
 * (EGL_KHR_client_get_all_proc_addresses); the prototypes give the types. */
#define GL_FUNCS(X)                                                            \
   X(glGetString)                                          \
   X(glGetError)                                            \
   X(glViewport)                                            \
   X(glClearColor)                                        \
   X(glClear)                                                  \
   X(glBegin)                                                  \
   X(glEnd)                                                      \
   X(glColor3f)                                              \
   X(glVertex2f)                                            \
   X(glCreateShader)                                    \
   X(glShaderSource)                                    \
   X(glCompileShader)                                  \
   X(glGetShaderiv)                                      \
   X(glGetShaderInfoLog)                            \
   X(glCreateProgram)                                  \
   X(glAttachShader)                                    \
   X(glLinkProgram)                                      \
   X(glGetProgramiv)                                    \
   X(glGetProgramInfoLog)                          \
   X(glUseProgram)                                        \
   X(glGenFramebuffers)                              \
   X(glBindFramebuffer)                              \
   X(glGenRenderbuffers)                            \
   X(glBindRenderbuffer)                            \
   X(glRenderbufferStorage)                      \
   X(glFramebufferRenderbuffer)              \
   X(glCheckFramebufferStatus)                \
   X(glBlitFramebuffer)                              \
   X(glReadPixels)                                        \
   X(glFinish)

#define DECLARE(name) static __typeof__(&name) p_##name;
GL_FUNCS(DECLARE)

static void
check_gl(const char *what)
{
   GLenum e = p_glGetError();
   if (e != GL_NO_ERROR)
      FAIL("%s: GL error 0x%x", what, e);
}

static GLuint
compile(GLenum type, const char *src)
{
   GLuint s = p_glCreateShader(type);
   GLint ok = 0;
   p_glShaderSource(s, 1, &src, NULL);
   p_glCompileShader(s);
   p_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok) {
      char log[4096] = "";
      p_glGetShaderInfoLog(s, sizeof(log), NULL, log);
      FAIL("GLSL compile failed: %s", log);
   }
   return s;
}

static const char vs120[] =
   "#version 120\n"
   "attribute vec2 unused;\n"
   "varying vec3 colour;\n"
   "void main() {\n"
   "   colour = gl_Color.rgb;\n"
   "   gl_Position = gl_Vertex * vec4(0.5, 0.5, 1.0, 1.0);\n"
   "}\n";
static const char fs120[] =
   "#version 120\n"
   "varying vec3 colour;\n"
   "uniform float unused_uniform;\n"
   "void main() {\n"
   "   gl_FragColor = vec4(colour.bgr, 1.0);\n"
   "}\n";

int
main(void)
{
   PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
   if (!get_platform_display)
      FAIL("no EGL_EXT_platform_base");
   /* The display win32u makes (dlls/win32u/opengl.c egldrv_init_egl_platform). */
   EGLDisplay dpy = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
   EGLint major, minor;
   if (!eglInitialize(dpy, &major, &minor))
      FAIL("eglInitialize: 0x%x", eglGetError());
   printf("EGL %d.%d %s\n", major, minor, eglQueryString(dpy, EGL_VENDOR));
   if (!eglBindAPI(EGL_OPENGL_API))
      FAIL("eglBindAPI");

   const EGLint config_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE,
   };
   EGLConfig config;
   EGLint nconf = 0;
   if (!eglChooseConfig(dpy, config_attribs, &config, 1, &nconf) || !nconf)
      FAIL("no pbuffer config");
   const EGLint pbuffer_attribs[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
   EGLSurface surf = eglCreatePbufferSurface(dpy, config, pbuffer_attribs);
   if (surf == EGL_NO_SURFACE)
      FAIL("eglCreatePbufferSurface: 0x%x", eglGetError());
   /* A compatibility context, what WGL's wglCreateContext gives. */
   const EGLint ctx_attribs[] = {
      EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
      EGL_NONE,
   };
   EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctx_attribs);
   if (ctx == EGL_NO_CONTEXT)
      FAIL("eglCreateContext: 0x%x", eglGetError());
   if (!eglMakeCurrent(dpy, surf, surf, ctx))
      FAIL("eglMakeCurrent: 0x%x", eglGetError());

#define LOAD(name)                                                             \
   if (!(p_##name = (__typeof__(&name))eglGetProcAddress(#name)))              \
      FAIL("no %s", #name);
   GL_FUNCS(LOAD)

   const char *renderer = (const char *)p_glGetString(GL_RENDERER);
   const char *version = (const char *)p_glGetString(GL_VERSION);
   const char *glsl = (const char *)p_glGetString(GL_SHADING_LANGUAGE_VERSION);
   printf("GL_RENDERER %s\nGL_VERSION %s\nGLSL %s\n", renderer, version, glsl);
   if (!renderer || !strstr(renderer, "zink"))
      FAIL("the renderer is not Zink");
   check_gl("glGetString");

   p_glViewport(0, 0, 64, 64);
   p_glClearColor(0.0f, 0.25f, 0.5f, 1.0f);
   p_glClear(GL_COLOR_BUFFER_BIT);
   check_gl("glClear");

   /* Fixed function: Mesa's own shaders for the fixed-function state. */
   p_glBegin(GL_TRIANGLES);
   p_glColor3f(1, 0, 0);
   p_glVertex2f(-1, -1);
   p_glColor3f(0, 1, 0);
   p_glVertex2f(0, -1);
   p_glColor3f(0, 0, 1);
   p_glVertex2f(-0.5f, 0);
   p_glEnd();
   check_gl("fixed-function triangle");

   GLuint prog = p_glCreateProgram();
   p_glAttachShader(prog, compile(GL_VERTEX_SHADER, vs120));
   p_glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fs120));
   p_glLinkProgram(prog);
   GLint linked = 0;
   p_glGetProgramiv(prog, GL_LINK_STATUS, &linked);
   if (!linked) {
      char log[4096] = "";
      p_glGetProgramInfoLog(prog, sizeof(log), NULL, log);
      FAIL("GLSL link failed: %s", log);
   }
   p_glUseProgram(prog);
   p_glBegin(GL_TRIANGLES);
   p_glColor3f(1, 1, 0);
   p_glVertex2f(0, 0);
   p_glVertex2f(1, 0);
   p_glVertex2f(0.5f, 1);
   p_glEnd();
   p_glUseProgram(0);
   check_gl("GLSL 1.20 triangle");

   /* What win32u does on every SwapBuffers: a blit from the game's FBO. */
   GLuint fbo, rb;
   p_glGenFramebuffers(1, &fbo);
   p_glGenRenderbuffers(1, &rb);
   p_glBindRenderbuffer(GL_RENDERBUFFER, rb);
   p_glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 32, 32);
   p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
   if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      FAIL("framebuffer incomplete");
   p_glClear(GL_COLOR_BUFFER_BIT);
   p_glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
   p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
   p_glBlitFramebuffer(0, 0, 32, 32, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
   check_gl("glBlitFramebuffer");

   p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
   static unsigned char pixels[64 * 64 * 4];
   p_glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
   p_glFinish();
   check_gl("glReadPixels");

   eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(dpy, ctx);
   eglDestroySurface(dpy, surf);
   eglTerminate(dpy);
   printf("zink-host-test: ok\n");
   return 0;
}
