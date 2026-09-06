# dwgcore

A native interop DLL that embeds the same DWG/DXF rendering pipeline as
`../dwgviewer` (`libdxfrw` -> `DwgDocument` -> `ViewerWidget`/`QPainter`) as
a real, live child window inside a host application -- concretely, a C# WPF
app via `HwndHost` (see `../csharptest`). See `README.md` for the full
design writeup (why a dedicated thread, the C API, build steps, DPI, the
WPF-airspace limitation). This file is conventions or "gotchas that will
bite you," not a repeat of that.

## Architecture (one line each; see README.md for why)

```
file.dxf/.dwg -> libdxfrw -> DwgDocument -> ViewerWidget (QWidget, real HWND)
                                                 |
                                    winId() -----+----- SetParent (host's HwndHost)
                                                 |
                              dwgcore.dll (Qt worker thread) <--P/Invoke--> host UI thread
```

- `src/dwg_document.h/.cpp`, `src/viewer_widget.h/.cpp` -- ported verbatim
  from `../dwgviewer/src/`. **Do not let these drift from the source of
  truth without a reason.** If you fix a rendering bug here, check whether
  it applies to `dwgviewer` too (and vice versa) -- see
  `../dwgviewer/CLAUDE.md` for the full list of entity-rendering
  conventions/traps (Y-flip angle conventions, dash-pattern drawing, dimension
  synthesis, hatch loops, etc.) that apply to this copy identically, since
  it's the same widget.
- `src/qt_engine.h/.cpp` -- the shared `QApplication` + dedicated worker
  thread + `runBlocking`/`runQueued` cross-thread marshaling. This is the
  answer to the project's core threading problem (see README.md). If you
  touch this file, re-verify with `validation/interop_smoke_test.cpp`
  (below) before trusting a change.
- `src/dwg_view_session.h/.cpp` -- one `DwgSession` per embedded viewport.
  **Every method here assumes it's already running on the Qt thread.** It
  does zero thread-safety of its own on purpose -- that's `dwgcore_api.cpp`'s
  job, not this class's.
- `src/dwgcore_api.h/.cpp` -- the exported `extern "C"` surface. Plain C
  types only (no STL across the ABI boundary, no C++ name mangling) so it
  P/Invokes cleanly.

## The one rule for `dwgcore_api.cpp`

Every exported function's body only ever does two things: pick
`QtEngine::runBlocking` (caller needs the result, e.g. create/load/destroy)
or `QtEngine::runQueued` (fire-and-forget, e.g. resize/visibility), and put
the actual Qt/widget work inside the lambda. **Never call into
`ViewerWidget`/`DwgDocument`/`DwgSession`/any `QObject` directly from the
calling thread.** This is the rule that keeps the whole design correct; it's
restated at the top of `dwgcore_api.h` too.

## `validation/interop_smoke_test.cpp` -- keep this working

A plain console program (no WPF, no HwndHost) that calls the full public API
in sequence and prints a line after every call, so a hang or crash shows
exactly where it got stuck. This is what isolated a threading question
during development: run it any time a change touches `qt_engine.cpp` or
`dwg_view_session.cpp`, *before* going anywhere near the WPF harness --
it's much faster to iterate on and rules out whether a problem is in this
DLL or in the C#/WPF layer around it. Build it with MSVC directly (it's not
part of the CMake target -- deliberately, so it never needs Qt's MOC step):

```
call "<VS install>\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /EHsc /std:c++17 /I src validation\interop_smoke_test.cpp ^
   /link /LIBPATH:build\Release dwgcore.lib /OUT:build\Release\interop_smoke_test.exe
build\Release\interop_smoke_test.exe path\to\file.dxf
```

If you see the app hang or fail to show its window, run this first. In
practice, an apparent hang on the very first WPF launch after a fresh
rebuild is far more likely to be Windows Defender's real-time scan of the
newly-built (so newly-hashed, so unseen) `dwgcore.dll` and its Qt
dependencies than an actual deadlock -- rerun the same launch a second time
before assuming a code bug; if the smoke test above completes cleanly and
quickly, the native layer is fine and the problem is upstream of it.

## `third_party/libdxfrw` -- vendored, treat as read-only

Same policy as `../dwgviewer`: don't modify files under here as part of a
feature change. If a real bug fix is needed, flag it explicitly rather than
patching silently, so re-vendoring later doesn't lose the fix.

## Building

See README.md for the full 32-bit/64-bit CMake invocations and the
`windeployqt` step needed to collect Qt's runtime DLLs next to
`dwgcore.dll`. Both bitness variants must be rebuilt (and re-deployed) after
any source change meant to reach the WPF harness -- `../csharptest`'s
project file copies whatever is currently sitting in
`build/Release`/`build-x86/Release`, not what CMake would produce if
rebuilt.
