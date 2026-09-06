#pragma once

#include <cstdint>

#if defined(_WIN32)
#if defined(DWGCORE_EXPORTS)
#define DWGCORE_API extern "C" __declspec(dllexport)
#else
#define DWGCORE_API extern "C" __declspec(dllimport)
#endif
#else
#define DWGCORE_API extern "C"
#endif

typedef void *DwgCoreHandle;

// Plain-C, P/Invoke-friendly surface for embedding a DWG/DXF viewport
// (backed by a real native HWND on Windows) into a host application such as
// a C# WPF window via HwndHost.
//
// THREADING RULE: every function below marshals its actual work onto a
// single dedicated Qt worker thread (see qt_engine.h) -- none of them touch
// a Qt object directly on the calling thread, because Qt's own event loop
// can't share a thread with a host UI framework's (e.g. WPF's dispatcher)
// event loop. DwgCore_CreateView/GetNativeHandle/LoadFile/DestroyView block
// the calling thread until the work completes; DwgCore_SetVisible/
// ResizeView/ZoomFit are fire-and-forget. Keep this rule when extending the
// API: never add a function that touches a Qt/widget object directly.

// Creates one embedded viewport. Starts the shared Qt engine (QApplication
// + its worker thread) on first call; later calls reuse it, so multiple
// simultaneous viewports in one process are supported. Returns null on
// failure.
DWGCORE_API DwgCoreHandle DwgCore_CreateView();

// Returns the viewport's native handle (an HWND on Windows) as void*, or
// null if `handle` is invalid. The widget starts hidden -- call this only
// after reparenting the handle into the host window (SetParent + swapping
// WS_POPUP for WS_CHILD), then call DwgCore_SetVisible(handle, true), to
// avoid a flash of a real top-level/taskbar window.
DWGCORE_API void *DwgCore_GetNativeHandle(DwgCoreHandle handle);

// Loads a .dxf/.dwg file into the viewport, replacing whatever was loaded
// before, and fits the view to it. `path` is a plain narrow-char path in
// the OS's active code page (CP_ACP), NOT UTF-8 -- libdxfrw (via
// DwgDocument::loadFile) opens it as a plain `const char*`, the same
// convention dwgviewer/src/main.cpp already relies on
// (`QString::toLocal8Bit()`/`fromLocal8Bit()`). From C#, marshal the string
// with `CharSet.Ansi` to match. Blocks the calling thread until parsing
// finishes -- for large files, call this from a background thread/Task on
// the host side rather than a UI thread. Returns false on failure; see
// DwgCore_GetLastError for why. Returns false immediately if `handle` is
// invalid.
DWGCORE_API bool DwgCore_LoadFile(DwgCoreHandle handle, const char *path);

// Copies the last load error (UTF-8, null-terminated, truncated to fit
// bufferLen) into `buffer`. Writes an empty string if there was no error,
// `handle` is invalid, or `buffer`/`bufferLen` are unusable.
DWGCORE_API void DwgCore_GetLastError(DwgCoreHandle handle, char *buffer, int32_t bufferLen);

// Shows or hides the viewport widget. No-op if `handle` is invalid.
DWGCORE_API void DwgCore_SetVisible(DwgCoreHandle handle, bool visible);

// Manual-override resize; usually unnecessary once the handle has been
// reparented as a WS_CHILD, since the host's own native window resizing
// already delivers real WM_SIZE messages straight to this widget's window
// proc. No-op if `handle` is invalid.
DWGCORE_API void DwgCore_ResizeView(DwgCoreHandle handle, int32_t width, int32_t height);

// Re-fits the view to the currently loaded document. No-op if `handle` is
// invalid.
DWGCORE_API void DwgCore_ZoomFit(DwgCoreHandle handle);

// Destroys one viewport. Safe to call with an already-destroyed or invalid
// handle (no-op). Blocks the calling thread until the underlying widget has
// actually been deleted.
DWGCORE_API void DwgCore_DestroyView(DwgCoreHandle handle);

// Destroys any remaining viewports, quits the Qt engine, and joins its
// worker thread. Call this explicitly before the host process exits (e.g.
// from C#'s App.OnExit) -- there is deliberately no DllMain-based cleanup,
// since joining a thread during DLL_PROCESS_DETACH is unsafe. Safe to call
// even if no viewport was ever created.
DWGCORE_API void DwgCore_Shutdown();
