#pragma once

#include <string>

class ViewerWidget;

// One embedded DWG/DXF viewport: a top-level, frameless ViewerWidget whose
// native HWND is meant to be reparented into a host window (e.g. via WPF's
// HwndHost) by the caller.
//
// Every method here must only be called from the Qt worker thread -- this
// class does no thread marshaling itself. dwgcore_api.cpp is the only
// caller, and it's responsible for routing every call through
// QtEngine::runBlocking()/runQueued().
class DwgSession {
public:
    DwgSession();
    ~DwgSession();

    DwgSession(const DwgSession &) = delete;
    DwgSession &operator=(const DwgSession &) = delete;

    // The widget's native handle (an HWND on Windows), as an opaque pointer.
    // Forced into existence at construction time, before the widget is ever
    // shown, so it's always valid to call.
    void *nativeHandle() const;

    // Loads a .dxf/.dwg file (UTF-8 path), replacing whatever was loaded
    // before. Returns false and records lastError() on failure.
    bool loadFile(const std::string &path);
    const std::string &lastError() const { return lastError_; }

    // Call `true` only after the caller has finished reparenting the native
    // handle into a host window -- the widget starts hidden specifically to
    // avoid a flash of a real top-level/taskbar window before that happens.
    void setVisible(bool visible);

    // Manual-override resize. Usually unnecessary: once reparented as a
    // WS_CHILD, the host's own native resizing already delivers WM_SIZE to
    // this widget's window proc directly.
    void resize(int width, int height);

    void zoomFit();

private:
    ViewerWidget *widget_ = nullptr;
    std::string lastError_;
};
