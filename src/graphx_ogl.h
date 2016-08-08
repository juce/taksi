#ifndef _JUCE_GRAPHX_OGL_
#define _JUCE_GRAPHX_OGL_

#include <GL/gl.h>
#include <wingdi.h>

#ifndef HEAP_SEARCH_INTERVAL
#define HEAP_SEARCH_INTERVAL 1000  // in msec
#define DLLMAP_PAUSE 500           // in msec
#endif

EXTERN_C _declspec(dllexport) void RestoreOpenGLFunctions();
//VOID HookerFuncOpenGL(LPVOID lpParam); // hooker thread function
//BOOL GraphicsLoopHookedOGL();
void InitHooksOGL();
void GraphicsCleanupOGL();
void VerifyOGL();

//BOOL FindEntryInIAT(DWORD procAddr, DWORD** pFunction);

/* typedefs for OpenGL functions that we use.
 * We cannot link implicitly with opengl32.lib, because we test
 * OpenGL usage in the applications by looking for OpenGL functions.
 * (Linking with opengl32.lib will make every app look like it is
 * using OpenGL).
 */
typedef WINGDIAPI void (APIENTRY *PFNGLBEGINPROC)(GLenum mode);
typedef WINGDIAPI void (APIENTRY *PFNGLDISABLEPROC)(GLenum cap);
typedef WINGDIAPI void (APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef WINGDIAPI void (APIENTRY *PFNGLCOLOR3FPROC)(GLfloat red, GLfloat green, GLfloat blue);
typedef WINGDIAPI void (APIENTRY *PFNGLENABLEPROC)(GLenum cap);
typedef WINGDIAPI void (APIENTRY *PFNGLENDPROC)(void);
typedef WINGDIAPI void (APIENTRY *PFNGLGETINTEGERVPROC)(GLenum pname, GLint *params);
typedef WINGDIAPI const GLubyte* (APIENTRY *PFNGLGETSTRINGPROC)(GLenum name);
typedef WINGDIAPI void (APIENTRY *PFNGLLOADIDENTITYPROC)(void);
typedef WINGDIAPI void (APIENTRY *PFNGLMATRIXMODEPROC)(GLenum mode);
typedef WINGDIAPI void (APIENTRY *PFNGLORTHOPROC)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
typedef WINGDIAPI void (APIENTRY *PFNGLPOPATTRIBPROC)(void);
typedef WINGDIAPI void (APIENTRY *PFNGLPOPMATRIXPROC)(void);
typedef WINGDIAPI void (APIENTRY *PFNGLPUSHATTRIBPROC)(GLbitfield mask);
typedef WINGDIAPI void (APIENTRY *PFNGLPUSHMATRIXPROC)(void);
typedef WINGDIAPI void (APIENTRY *PFNGLREADBUFFERPROC)(GLenum mode);
typedef WINGDIAPI void (APIENTRY *PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
typedef WINGDIAPI void (APIENTRY *PFNGLSHADEMODELPROC)(GLenum mode);
typedef WINGDIAPI void (APIENTRY *PFNGLVERTEX2IPROC)(GLint x, GLint y);
typedef WINGDIAPI void (APIENTRY *PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);

typedef WINGDIAPI HDC  (WINAPI *PFNWGLGETCURRENTDCPROC)(VOID);
typedef WINGDIAPI PROC (WINAPI *PFNWGLGETPROCADDRESSPROC)(LPCSTR);

#endif
