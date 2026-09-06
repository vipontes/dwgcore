# dwgcore

A native interop DLL that embeds the same DWG/DXF rendering pipeline as
`dwgviewer` (`libdxfrw` -> `DwgDocument` -> `ViewerWidget`/`QPainter`) as a
real, live child window inside a host application — a C# WPF app via
`HwndHost`, in particular — rather than a screenshot/bitmap bridge.

## Why a dedicated thread (the threading issue this project exists to solve)

`QWidget::winId()` gives a real Win32 HWND on Windows, and WPF's `HwndHost`
can reparent that HWND into the WPF visual tree (`SetParent` + swapping
`WS_POPUP` for `WS_CHILD`). That part is straightforward.

The part that isn't: **Qt wants to run its own event loop**
(`QApplication::exec()`), but a WPF host's main thread already runs WPF's
own dispatcher loop — they cannot share a thread. This DLL's fix is to run
`QApplication` and every widget it owns on **one dedicated background
thread** for the whole process (`src/qt_engine.*`), and marshal every call
in from the host process's thread through Qt's own thread-safe invocation
mechanism (`QMetaObject::invokeMethod` with `Qt::BlockingQueuedConnection`/
`Qt::QueuedConnection`) rather than ever touching a Qt object directly from
outside that thread.

**The rule for anyone extending `dwgcore_api.cpp`:** every exported
function's body only ever does two things — pick `QtEngine::runBlocking`
(caller needs the result/guarantee, e.g. create/load/destroy) or
`QtEngine::runQueued` (fire-and-forget, e.g. resize/visibility) — and puts
the actual Qt/widget work inside the lambda. Never call into
`ViewerWidget`/`DwgDocument`/any `QObject` directly from the calling
thread.

## Architecture

```
file.dxf/.dwg -> libdxfrw -> DwgDocument -> ViewerWidget (QWidget, real HWND)
                                                 |
                                    winId() -----+----- SetParent (host's HwndHost)
                                                 |
                              dwgcore.dll (Qt worker thread) <--P/Invoke--> host UI thread
```

- `src/dwg_document.h/.cpp`, `src/viewer_widget.h/.cpp` — ported unchanged
  from `dwgviewer/src/`; same `DRW_Interface`/`Shape` model and
  paint/pan/zoom widget. See `dwgviewer/CLAUDE.md` for the entity-rendering
  conventions and traps (Y-flip angle conventions, dash patterns, etc.) —
  they apply here identically since this is the same widget.
- `src/qt_engine.h/.cpp` — the shared `QApplication` + worker thread +
  `runBlocking`/`runQueued` marshaling described above.
- `src/dwg_view_session.h/.cpp` — one `DwgSession` per embedded viewport: a
  top-level, frameless `ViewerWidget` whose native handle is forced into
  existence (but kept hidden) at construction. All of its methods assume
  they're already running on the Qt thread — only `dwgcore_api.cpp` is
  responsible for getting them there.
- `src/dwgcore_api.h/.cpp` — the exported `extern "C"` surface. Plain C
  signatures only (no STL types, no C++ name mangling) so it P/Invokes
  cleanly from C#.

## Building (32-bit and 64-bit)

Qt6's official installer ships no 32-bit Windows kit, so a 32-bit host
process needs this DLL built against Qt5 instead — mirroring
`dwgviewer`'s own Qt6-else-Qt5 CMake fallback.

```bash
# 64-bit (Qt6/msvc2022_64) -- for a 64-bit host process
cmake -S dwgcore -B dwgcore/build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/msvc2022_64
cmake --build dwgcore/build --config Release

# 32-bit (Qt5/msvc2019) -- for a 32-bit (legacy) host process
cmake -S dwgcore -B dwgcore/build-x86 -A Win32 -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019
cmake --build dwgcore/build-x86 --config Release
```

Each produces `dwgcore.dll` (plus `dwgcore.lib`) alongside the matching
Qt DLLs (`Qt6Widgets.dll`/`Qt6Core.dll`/`Qt6Gui.dll` or their Qt5
equivalents, plus `platforms/qwindows.dll`) that must ship next to it — use
Qt's `windeployqt.exe` from the matching kit against the built `dwgcore.dll`
to collect them.

**The host process's bitness must match the DLL's.** A 64-bit .NET process
can only load a 64-bit `dwgcore.dll`, and vice versa — there is no
`AnyCPU`-style flexibility once a native dependency is involved.

## Using it from C#

See `csharptest/` for a complete minimal WPF harness: P/Invoke
declarations, an `HwndHost` subclass doing the `SetParent`/style-bit
surgery, and a window that loads a file and displays it embedded. In
outline, per viewport:

1. `DwgCore_CreateView()` — do this in `HwndHost.BuildWindowCore`.
2. `DwgCore_GetNativeHandle(handle)` — get the HWND, `SetParent` it under
   the `HwndHost`'s own handle, strip `WS_POPUP`/`WS_CAPTION` and add
   `WS_CHILD` via `SetWindowLongPtr(GWL_STYLE, ...)`.
3. `DwgCore_SetVisible(handle, true)` — only now, after reparenting, to
   avoid a flash of a real top-level/taskbar window.
4. `DwgCore_LoadFile(handle, path)` — **blocks the calling thread** until
   parsing finishes (this DLL deliberately keeps this call synchronous,
   matching `dwgviewer`'s own behavior, rather than adding async-callback
   complexity across the native/managed boundary). Call it via `Task.Run`
   from C# for large files so the UI thread isn't frozen. `path` is a plain
   narrow-char, active-code-page string (not UTF-8) — marshal it with
   `CharSet.Ansi` from C#; see `dwgcore_api.h` for why.
5. `DwgCore_DestroyView(handle)` — in `HwndHost.DestroyWindowCore`.
6. `DwgCore_Shutdown()` — once, from `App.OnExit`, before the process
   exits. There's no `DllMain`-based cleanup (joining a thread during
   `DLL_PROCESS_DETACH` is unsafe), so this call is mandatory, not optional
   cleanup.

`DwgCore_ResizeView`/`DwgCore_ZoomFit` exist for manual control, but note
that once the handle is reparented as a `WS_CHILD`, the host's own native
window resizing already delivers real `WM_SIZE` messages straight to Qt's
window proc — `DwgCore_ResizeView` is a safety net, not something you need
to wire up to every host resize event.

### DPI

Set `Qt::HighDpiScaleFactorRoundingPolicy::PassThrough` is already handled
inside `qt_engine.cpp`, before `QApplication` is constructed. What this DLL
*cannot* do is change the host **process's** DPI-awareness mode after
launch — that's fixed by the host `.exe`'s own manifest. For crisp,
non-double-scaled rendering, the host WPF app's manifest must declare
Per-Monitor V2 DPI awareness
(`<dpiAwareness>PerMonitorV2</dpiAwareness>`).

## Known limitation carried over from the referenced design discussion

WPF cannot draw over a native embedded HWND ("airspace"): a WPF popup,
tooltip, adorner, or overlapping control that should appear on top of the
embedded viewport will get clipped/hidden behind it instead. Not an issue
for a host that never needs WPF content layered over the viewport; if that
changes later, the fix would be rendering into a shared texture and
presenting through `D3DImage` instead of HWND embedding — a materially
different (and larger) rework, not a tweak to this design.
