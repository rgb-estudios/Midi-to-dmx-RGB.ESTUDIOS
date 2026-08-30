# AEYLA R08 — Windows NanoVG GL2 capability preflight.
#
# The pinned iPlug2 NanoVG backend calls nvgCreateContext() after gladLoadGL().
# glad loading can succeed on a Windows software/headless OpenGL 1.1 context
# even though the shader/buffer entry points NanoVG GL2 immediately calls are
# unavailable. NanoVG then performs an indirect call through address 0 before it
# can return a null NVGcontext. Reject that graphics context first instead.
#
# This patch is intentionally applied AFTER AeylaIPlugWindowsGraphicsHardening
# has rewritten IGraphicsWin.cpp. It changes only the Windows APP/VST3 editor
# path; DSP/runtime, Art-Net and macOS builds are untouched.

if(NOT WIN32)
  return()
endif()

set(_aeyla_igraphics_win "${IPLUG2_DIR}/IGraphics/Platforms/IGraphicsWin.cpp")
if(NOT EXISTS "${_aeyla_igraphics_win}")
  message(FATAL_ERROR "Pinned iPlug2 IGraphicsWin.cpp not found: ${_aeyla_igraphics_win}")
endif()

file(READ "${_aeyla_igraphics_win}" _aeyla_igraphics_win_source)

set(_aeyla_nanovg_preflight_marker
    "IGraphicsWin:CreateGLContext-NanoVG-entrypoints-ok")
string(FIND "${_aeyla_igraphics_win_source}"
       "${_aeyla_nanovg_preflight_marker}"
       _aeyla_nanovg_preflight_present)

if(_aeyla_nanovg_preflight_present EQUAL -1)
  set(_aeyla_nanovg_preflight_old [=[  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-glad-ok");
  glGetError();
  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-exit");

  ReleaseDC(mPlugWnd, dc);]=])

  set(_aeyla_nanovg_preflight_new [=[  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-glad-ok");

  // A legacy WGL context may satisfy gladLoadGL() while exposing only OpenGL
  // 1.1. NanoVG GL2 creates GLSL shaders immediately in nvgCreateContext(), so
  // validate both the advertised version and every non-1.1 entry point needed
  // by that initialization path before allowing OnViewInitialized() to run.
  const GLubyte* aeylaVersionBytes = glGetString(GL_VERSION);
  const char* aeylaVersion = reinterpret_cast<const char*>(aeylaVersionBytes);
  if (!aeylaVersion || aeylaVersion[0] < '2')
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-OpenGL2-unavailable");
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(mHGLRC);
    mHGLRC = nullptr;
    ReleaseDC(mPlugWnd, dc);
    return;
  }

  static const char* const kAeylaNanoVGRequiredWglProcs[] = {
    "glCreateProgram",
    "glCreateShader",
    "glShaderSource",
    "glCompileShader",
    "glGetShaderiv",
    "glGetShaderInfoLog",
    "glAttachShader",
    "glBindAttribLocation",
    "glLinkProgram",
    "glGetProgramiv",
    "glGetProgramInfoLog",
    "glDeleteProgram",
    "glDeleteShader",
    "glGetUniformLocation",
    "glGenBuffers",
    "glBindBuffer",
    "glBufferData",
    "glBufferSubData",
    "glUseProgram",
    "glUniform1i",
    "glUniform2fv",
    "glUniform4fv",
    "glEnableVertexAttribArray",
    "glDisableVertexAttribArray",
    "glVertexAttribPointer",
    "glActiveTexture",
    "glBlendFuncSeparate"
  };

  for (const char* procName : kAeylaNanoVGRequiredWglProcs)
  {
    const PROC proc = wglGetProcAddress(procName);
    const auto procValue = reinterpret_cast<INT_PTR>(proc);
    if (!proc || procValue == 1 || procValue == 2 || procValue == 3 ||
        procValue == -1)
    {
      AeylaGraphicsStartupTrace(
          "IGraphicsWin:CreateGLContext-NanoVG-entrypoint-missing");
      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(mHGLRC);
      mHGLRC = nullptr;
      ReleaseDC(mPlugWnd, dc);
      return;
    }
  }

  AeylaGraphicsStartupTrace(
      "IGraphicsWin:CreateGLContext-NanoVG-entrypoints-ok");
  glGetError();
  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-exit");

  ReleaseDC(mPlugWnd, dc);]=])

  string(FIND "${_aeyla_igraphics_win_source}"
         "${_aeyla_nanovg_preflight_old}"
         _aeyla_nanovg_preflight_pos)
  if(_aeyla_nanovg_preflight_pos EQUAL -1)
    message(FATAL_ERROR
      "R08 Windows GL hardening block changed; review NanoVG capability preflight before building.")
  endif()

  string(REPLACE
    "${_aeyla_nanovg_preflight_old}"
    "${_aeyla_nanovg_preflight_new}"
    _aeyla_igraphics_win_source
    "${_aeyla_igraphics_win_source}")
  file(WRITE "${_aeyla_igraphics_win}" "${_aeyla_igraphics_win_source}")
  message(STATUS "Applied AEYLA NanoVG GL2 capability preflight to pinned iPlug2")
endif()
