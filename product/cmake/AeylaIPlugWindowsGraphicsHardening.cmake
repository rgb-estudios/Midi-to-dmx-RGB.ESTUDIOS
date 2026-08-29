# AEYLA R08 — pinned iPlug2 Windows graphics hardening.
#
# NanoVG on Windows uses GL function pointers loaded by glad. The pinned iPlug2
# revision logs a failed gladLoadGL() but immediately calls glGetError() anyway.
# On a non-interactive/headless Windows desktop this can be a null indirect call
# at address 0 (0xC0000005). A production host must reject an unavailable editor
# without terminating the whole plugin/standalone process.

if(NOT WIN32)
  return()
endif()

set(_aeyla_igraphics_win "${IPLUG2_DIR}/IGraphics/Platforms/IGraphicsWin.cpp")
if(NOT EXISTS "${_aeyla_igraphics_win}")
  message(FATAL_ERROR "Pinned iPlug2 IGraphicsWin.cpp not found: ${_aeyla_igraphics_win}")
endif()

file(READ "${_aeyla_igraphics_win}" _aeyla_igraphics_win_source)

set(_aeyla_gl_loader_old [=[  //TODO: return false if GL init fails?
  if (!gladLoadGL())
    DBGMSG("Error initializing glad");

  glGetError();

  ReleaseDC(mPlugWnd, dc);]=])

set(_aeyla_gl_loader_new [=[  // Fail the editor cleanly when this Windows session cannot provide a usable
  // OpenGL function table. Never call through an unloaded glad function pointer.
  if (!mHGLRC || !gladLoadGL())
  {
    DBGMSG("Error initializing Windows OpenGL/glad; editor will remain unavailable");
    if (mHGLRC)
    {
      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(mHGLRC);
      mHGLRC = nullptr;
    }
    ReleaseDC(mPlugWnd, dc);
    return;
  }

  glGetError();

  ReleaseDC(mPlugWnd, dc);]=])

string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_gl_loader_new}" _aeyla_gl_loader_patched)
if(_aeyla_gl_loader_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_gl_loader_old}" _aeyla_gl_loader_pos)
  if(_aeyla_gl_loader_pos EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 GL loader block changed; review AEYLA Windows graphics hardening.")
  endif()
  string(REPLACE
    "${_aeyla_gl_loader_old}"
    "${_aeyla_gl_loader_new}"
    _aeyla_igraphics_win_source
    "${_aeyla_igraphics_win_source}")
endif()

set(_aeyla_create_window_old [=[  mPlugWnd = CreateWindowW(wndClassName, L"IPlug", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, mParentWnd, 0, mHInstance, this);

  HDC dc = GetDC(mPlugWnd);]=])

set(_aeyla_create_window_new [=[  mPlugWnd = CreateWindowW(wndClassName, L"IPlug", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, mParentWnd, 0, mHInstance, this);

  if (!mPlugWnd)
  {
    if (--nWndClassReg == 0)
      UnregisterClassW(wndClassName, mHInstance);
    return nullptr;
  }

  HDC dc = GetDC(mPlugWnd);]=])

string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_create_window_new}" _aeyla_create_window_patched)
if(_aeyla_create_window_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_create_window_old}" _aeyla_create_window_pos)
  if(_aeyla_create_window_pos EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 CreateWindow block changed; review AEYLA Windows graphics hardening.")
  endif()
  string(REPLACE
    "${_aeyla_create_window_old}"
    "${_aeyla_create_window_new}"
    _aeyla_igraphics_win_source
    "${_aeyla_igraphics_win_source}")
endif()

set(_aeyla_create_gl_old [=[#ifdef IGRAPHICS_GL
  CreateGLContext();
#endif

  OnViewInitialized((void*) dc);]=])

set(_aeyla_create_gl_new [=[#ifdef IGRAPHICS_GL
  CreateGLContext();
  if (!mHGLRC)
  {
    DestroyWindow(mPlugWnd);
    mPlugWnd = nullptr;
    SetPlatformContext(nullptr);
    if (--nWndClassReg == 0)
      UnregisterClassW(wndClassName, mHInstance);
    return nullptr;
  }
#endif

  OnViewInitialized((void*) dc);]=])

string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_create_gl_new}" _aeyla_create_gl_patched)
if(_aeyla_create_gl_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_create_gl_old}" _aeyla_create_gl_pos)
  if(_aeyla_create_gl_pos EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 CreateGLContext block changed; review AEYLA Windows graphics hardening.")
  endif()
  string(REPLACE
    "${_aeyla_create_gl_old}"
    "${_aeyla_create_gl_new}"
    _aeyla_igraphics_win_source
    "${_aeyla_igraphics_win_source}")
endif()

file(WRITE "${_aeyla_igraphics_win}" "${_aeyla_igraphics_win_source}")
message(STATUS "Applied AEYLA Windows OpenGL fail-graceful hardening to pinned iPlug2")
