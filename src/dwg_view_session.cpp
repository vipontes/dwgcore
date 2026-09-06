#include "dwg_view_session.h"

#include <Qt>

#include <cstdint>

#include "dwg_document.h"
#include "viewer_widget.h"

DwgSession::DwgSession() {
    widget_ = new ViewerWidget();
    widget_->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    // Force native (HWND-backed) window creation now, while still hidden --
    // winId() is meaningless before this and showing the widget first would
    // flash a real top-level window before the caller gets a chance to
    // reparent it.
    widget_->setAttribute(Qt::WA_NativeWindow, true);
    widget_->winId();
}

DwgSession::~DwgSession() {
    delete widget_;
}

void *DwgSession::nativeHandle() const {
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(widget_->winId()));
}

bool DwgSession::loadFile(const std::string &path) {
    DwgDocument doc;
    if (!doc.loadFile(path)) {
        lastError_ = doc.errorMessage();
        return false;
    }
    lastError_.clear();
    widget_->setDocument(std::move(doc));
    return true;
}

void DwgSession::setVisible(bool visible) {
    widget_->setVisible(visible);
}

void DwgSession::resize(int width, int height) {
    widget_->resize(width, height);
}

void DwgSession::zoomFit() {
    widget_->zoomFit();
}
