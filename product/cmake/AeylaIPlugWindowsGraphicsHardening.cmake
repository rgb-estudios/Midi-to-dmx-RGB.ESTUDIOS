# AEYLA R08 — pinned iPlug2 Windows graphics hardening.
#
# NanoVG on Windows uses GL function pointers loaded by glad. The pinned iPlug2
# revision assumes every Win32/OpenGL setup step succeeds. On a hosted or
# otherwise graphics-constrained Windows session that can become an indirect
# call through address 0 (0xC0000005). A production host must reject an
# unavailable editor without terminating the whole plugin/standalone process.
#
# The startup trace in this file is TEMPORARY R08 diagnostic instrumentation.
# It is intentionally confined to the pinned Windows graphics translation unit
# and must be removed before the final PRETEST SHA is frozen.

if(NOT WIN32)
  return()
endif()

set(_aeyla_igraphics_win "${IPLUG2_DIR}/IGraphics/Platforms/IGraphicsWin.cpp")
if(NOT EXISTS "${_aeyla_igraphics_win}")
  message(FATAL_ERROR "Pinned iPlug2 IGraphicsWin.cpp not found: ${_aeyla_igraphics_win}")
endif()

file(READ "${_aeyla_igraphics_win}" _aeyla_igraphics_win_source)

# Local trace helper: no dependency on the APP wrapper, so this translation
# unit remains linkable for VST3 as well. It writes only coarse startup stages.
set(_aeyla_trace_anchor [=[static double sFPS = 0.0;
]=])
set(_aeyla_trace_impl [=[static double sFPS = 0.0;

static void AeylaGraphicsStartupTrace(const char* stage)
{
  if(stage == nullptr)
    return;
  char tempPath[MAX_PATH] = {};
  const DWORD tempLength = GetTempPathA(MAX_PATH, tempPath);
  if(tempLength == 0 || tempLength >= MAX_PATH)
    return;
  static const char kFileName[] = "AEYLA_GRAPHICAL_STARTUP_TRACE.txt";
  if(tempLength + sizeof(kFileName) >= MAX_PATH)
    return;
  char path[MAX_PATH] = {};
  lstrcpyA(path, tempPath);
  lstrcatA(path, kFileName);
  HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(file == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  WriteFile(file, stage, static_cast<DWORD>(lstrlenA(stage)), &written, nullptr);
  static const char newline[] = "\r\n";
  WriteFile(file, newline, 2U, &written, nullptr);
  FlushFileBuffers(file);
  CloseHandle(file);
}
]=])
string(FIND "${_aeyla_igraphics_win_source}" "AeylaGraphicsStartupTrace" _aeyla_graphics_trace_present)
if(_aeyla_graphics_trace_present EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_trace_anchor}" _aeyla_trace_anchor_pos)
  if(_aeyla_trace_anchor_pos EQUAL -1)
    message(FATAL_ERROR "Pinned iPlug2 graphics trace anchor changed; review R08 instrumentation.")
  endif()
  string(REPLACE "${_aeyla_trace_anchor}" "${_aeyla_trace_impl}"
    _aeyla_igraphics_win_source "${_aeyla_igraphics_win_source}")
endif()

# Trace the synchronous WM_CREATE path. CreateWindowW does not return until this
# callback completes, so these markers distinguish a child-window/WndProc crash
# from the later GL/layout stages.
set(_aeyla_wm_create_old [=[  if (msg == WM_CREATE)
  {
    CREATESTRUCTW* lpcs = (CREATESTRUCTW *) lParam;
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LPARAM) lpcs->lpCreateParams);
    IGraphicsWin* pGraphics = (IGraphicsWin*) GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (pGraphics->mVSYNCEnabled) // use VBLANK thread
    {
      assert((pGraphics->FPS() == 60) && "If you want to run at frame rates other than 60FPS");
      pGraphics->StartVBlankThread(hWnd);
    }
    else // use WM_TIMER -- its best to get below 16ms because the windows time quanta is slightly above 15ms.
    {
      int mSec = static_cast<int>(std::floorf(1000.0f / (pGraphics->FPS())));
      if (mSec < 20) mSec = 15;
      SetTimer(hWnd, IPLUG_TIMER_ID, mSec, NULL);
    }

    SetFocus(hWnd); // gets scroll wheel working straight away
    DragAcceptFiles(hWnd, true);
    return 0;
  }]=])
set(_aeyla_wm_create_new [=[  if (msg == WM_CREATE)
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-enter");
    CREATESTRUCTW* lpcs = (CREATESTRUCTW *) lParam;
    if (!lpcs || !lpcs->lpCreateParams)
    {
      AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-invalid-createparams");
      return -1;
    }
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LPARAM) lpcs->lpCreateParams);
    IGraphicsWin* pGraphics = (IGraphicsWin*) GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!pGraphics)
    {
      AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-null-graphics");
      return -1;
    }
    AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-graphics-ok");

    if (pGraphics->mVSYNCEnabled) // use VBLANK thread
    {
      AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-before-vblank-thread");
      assert((pGraphics->FPS() == 60) && "If you want to run at frame rates other than 60FPS");
      pGraphics->StartVBlankThread(hWnd);
      AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-after-vblank-thread");
    }
    else // use WM_TIMER -- its best to get below 16ms because the windows time quanta is slightly above 15ms.
    {
      AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-before-timer");
      const float fps = pGraphics->FPS();
      if (!(fps > 0.0f) || !std::isfinite(fps))
      {
        AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-invalid-fps");
        return -1;
      }
      int mSec = static_cast<int>(std::floorf(1000.0f / fps));
      if (mSec < 20) mSec = 15;
      SetTimer(hWnd, IPLUG_TIMER_ID, mSec, NULL);
      AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-after-timer");
    }

    SetFocus(hWnd); // gets scroll wheel working straight away
    DragAcceptFiles(hWnd, true);
    AeylaGraphicsStartupTrace("IGraphicsWin:WM_CREATE-exit");
    return 0;
  }]=])
string(FIND "${_aeyla_igraphics_win_source}" "IGraphicsWin:WM_CREATE-enter" _aeyla_wm_create_patched)
if(_aeyla_wm_create_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_wm_create_old}" _aeyla_wm_create_pos)
  if(_aeyla_wm_create_pos EQUAL -1)
    message(FATAL_ERROR "Pinned iPlug2 WM_CREATE block changed; review Windows startup hardening.")
  endif()
  string(REPLACE "${_aeyla_wm_create_old}" "${_aeyla_wm_create_new}"
    _aeyla_igraphics_win_source "${_aeyla_igraphics_win_source}")
endif()

# Harden the complete legacy GL2 context setup. Never enter glad or any GL call
# unless every Win32/WGL prerequisite succeeded.
set(_aeyla_gl_setup_old [=[  HDC dc = GetDC(mPlugWnd);
  int fmt = ChoosePixelFormat(dc, &pfd);
  SetPixelFormat(dc, fmt, &pfd);
  mHGLRC = wglCreateContext(dc);
  wglMakeCurrent(dc, mHGLRC);
]=])
set(_aeyla_gl_setup_new [=[  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-enter");
  HDC dc = GetDC(mPlugWnd);
  if (!dc)
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-GetDC-failed");
    mHGLRC = nullptr;
    return;
  }
  const int fmt = ChoosePixelFormat(dc, &pfd);
  if (fmt == 0 || !SetPixelFormat(dc, fmt, &pfd))
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-pixelformat-failed");
    ReleaseDC(mPlugWnd, dc);
    mHGLRC = nullptr;
    return;
  }
  mHGLRC = wglCreateContext(dc);
  if (!mHGLRC || !wglMakeCurrent(dc, mHGLRC))
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-wgl-failed");
    if (mHGLRC)
      wglDeleteContext(mHGLRC);
    mHGLRC = nullptr;
    ReleaseDC(mPlugWnd, dc);
    return;
  }
  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-wgl-current");
]=])
string(FIND "${_aeyla_igraphics_win_source}" "IGraphicsWin:CreateGLContext-enter" _aeyla_gl_setup_patched)
if(_aeyla_gl_setup_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_gl_setup_old}" _aeyla_gl_setup_pos)
  if(_aeyla_gl_setup_pos EQUAL -1)
    message(FATAL_ERROR "Pinned iPlug2 GL setup block changed; review Windows graphics hardening.")
  endif()
  string(REPLACE "${_aeyla_gl_setup_old}" "${_aeyla_gl_setup_new}"
    _aeyla_igraphics_win_source "${_aeyla_igraphics_win_source}")
endif()

set(_aeyla_gl_loader_old [=[  //TODO: return false if GL init fails?
  if (!gladLoadGL())
    DBGMSG("Error initializing glad");

  glGetError();

  ReleaseDC(mPlugWnd, dc);]=])
set(_aeyla_gl_loader_new [=[  // Fail the editor cleanly when this Windows session cannot provide a usable
  // OpenGL function table. Never call through an unloaded glad function pointer.
  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-before-glad");
  if (!mHGLRC || !gladLoadGL())
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-glad-failed");
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

  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-glad-ok");
  glGetError();
  AeylaGraphicsStartupTrace("IGraphicsWin:CreateGLContext-exit");

  ReleaseDC(mPlugWnd, dc);]=])
string(FIND "${_aeyla_igraphics_win_source}" "IGraphicsWin:CreateGLContext-before-glad" _aeyla_gl_loader_patched)
if(_aeyla_gl_loader_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_gl_loader_old}" _aeyla_gl_loader_pos)
  if(_aeyla_gl_loader_pos EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 GL loader block changed; review AEYLA Windows graphics hardening.")
  endif()
  string(REPLACE "${_aeyla_gl_loader_old}" "${_aeyla_gl_loader_new}"
    _aeyla_igraphics_win_source "${_aeyla_igraphics_win_source}")
endif()

set(_aeyla_create_window_old [=[  mPlugWnd = CreateWindowW(wndClassName, L"IPlug", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, mParentWnd, 0, mHInstance, this);

  HDC dc = GetDC(mPlugWnd);]=])
set(_aeyla_create_window_new [=[  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-before-CreateWindow");
  mPlugWnd = CreateWindowW(wndClassName, L"IPlug", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, x, y, w, h, mParentWnd, 0, mHInstance, this);

  if (!mPlugWnd)
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-CreateWindow-failed");
    if (--nWndClassReg == 0)
      UnregisterClassW(wndClassName, mHInstance);
    return nullptr;
  }
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-CreateWindow");

  HDC dc = GetDC(mPlugWnd);
  if (!dc)
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-GetDC-failed");
    DestroyWindow(mPlugWnd);
    mPlugWnd = nullptr;
    if (--nWndClassReg == 0)
      UnregisterClassW(wndClassName, mHInstance);
    return nullptr;
  }
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-GetDC");]=])
string(FIND "${_aeyla_igraphics_win_source}" "IGraphicsWin:OpenWindow-before-CreateWindow" _aeyla_create_window_patched)
if(_aeyla_create_window_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_create_window_old}" _aeyla_create_window_pos)
  if(_aeyla_create_window_pos EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 CreateWindow block changed; review AEYLA Windows graphics hardening.")
  endif()
  string(REPLACE "${_aeyla_create_window_old}" "${_aeyla_create_window_new}"
    _aeyla_igraphics_win_source "${_aeyla_igraphics_win_source}")
endif()

set(_aeyla_context_old [=[  SetPlatformContext(dc);
  ReleaseDC(mPlugWnd, dc);

#ifdef IGRAPHICS_GL
  CreateGLContext();
#endif

  OnViewInitialized((void*) dc);

  SetScreenScale(screenScale); // resizes draw context

  GetDelegate()->LayoutUI(this);]=])
set(_aeyla_context_new [=[  SetPlatformContext(dc);
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-SetPlatformContext");
  ReleaseDC(mPlugWnd, dc);

#ifdef IGRAPHICS_GL
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-before-CreateGLContext");
  CreateGLContext();
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-CreateGLContext");
  if (!mHGLRC)
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-no-GL-context");
    DestroyWindow(mPlugWnd);
    mPlugWnd = nullptr;
    SetPlatformContext(nullptr);
    if (--nWndClassReg == 0)
      UnregisterClassW(wndClassName, mHInstance);
    return nullptr;
  }
#endif

  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-before-OnViewInitialized");
  OnViewInitialized((void*) dc);
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-OnViewInitialized");

  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-before-SetScreenScale");
  SetScreenScale(screenScale); // resizes draw context
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-SetScreenScale");

  auto* delegate = GetDelegate();
  if (!delegate)
  {
    AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-null-delegate");
    CloseWindow();
    return nullptr;
  }
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-before-LayoutUI");
  delegate->LayoutUI(this);
  AeylaGraphicsStartupTrace("IGraphicsWin:OpenWindow-after-LayoutUI");]=])
string(FIND "${_aeyla_igraphics_win_source}" "IGraphicsWin:OpenWindow-before-OnViewInitialized" _aeyla_context_patched)
if(_aeyla_context_patched EQUAL -1)
  string(FIND "${_aeyla_igraphics_win_source}" "${_aeyla_context_old}" _aeyla_context_pos)
  if(_aeyla_context_pos EQUAL -1)
    message(FATAL_ERROR
      "Pinned iPlug2 OpenWindow context block changed; review AEYLA Windows graphics hardening.")
  endif()
  string(REPLACE "${_aeyla_context_old}" "${_aeyla_context_new}"
    _aeyla_igraphics_win_source "${_aeyla_igraphics_win_source}")
endif()

file(WRITE "${_aeyla_igraphics_win}" "${_aeyla_igraphics_win_source}")
message(STATUS "Applied AEYLA Windows OpenGL fail-graceful hardening to pinned iPlug2")
