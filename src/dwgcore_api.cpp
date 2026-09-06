#include "dwgcore_api.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#include "dwg_view_session.h"
#include "qt_engine.h"

namespace {

std::mutex g_sessionsMutex;
std::unordered_map<DwgCoreHandle, DwgSession *> g_sessions;

// Looks up and returns the session for `handle` if it's still live, or
// null. A null result after this point in any function below means "handle
// already destroyed (or never valid) -- treat as a no-op", not an error to
// propagate to Qt.
DwgSession *lookup(DwgCoreHandle handle) {
    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    auto it = g_sessions.find(handle);
    return it == g_sessions.end() ? nullptr : it->second;
}

} // namespace

DwgCoreHandle DwgCore_CreateView() {
    DwgSession *session = nullptr;
    QtEngine::runBlocking([&session] { session = new DwgSession(); });
    if (!session) return nullptr;

    DwgCoreHandle handle = static_cast<DwgCoreHandle>(session);
    std::lock_guard<std::mutex> lock(g_sessionsMutex);
    g_sessions.emplace(handle, session);
    return handle;
}

void *DwgCore_GetNativeHandle(DwgCoreHandle handle) {
    DwgSession *session = lookup(handle);
    if (!session) return nullptr;

    void *result = nullptr;
    QtEngine::runBlocking([session, &result] { result = session->nativeHandle(); });
    return result;
}

bool DwgCore_LoadFile(DwgCoreHandle handle, const char *path_) {
    DwgSession *session = lookup(handle);
    if (!session || !path_) return false;

    const std::string path(path_);
    bool ok = false;
    QtEngine::runBlocking([session, &path, &ok] { ok = session->loadFile(path); });
    return ok;
}

void DwgCore_GetLastError(DwgCoreHandle handle, char *buffer, int32_t bufferLen) {
    if (!buffer || bufferLen <= 0) return;
    buffer[0] = '\0';

    DwgSession *session = lookup(handle);
    if (!session) return;

    std::string error;
    QtEngine::runBlocking([session, &error] { error = session->lastError(); });

    const size_t n = std::min(static_cast<size_t>(bufferLen - 1), error.size());
    std::memcpy(buffer, error.data(), n);
    buffer[n] = '\0';
}

void DwgCore_SetVisible(DwgCoreHandle handle, bool visible) {
    DwgSession *session = lookup(handle);
    if (!session) return;
    QtEngine::runQueued([session, visible] { session->setVisible(visible); });
}

void DwgCore_ResizeView(DwgCoreHandle handle, int32_t width, int32_t height) {
    DwgSession *session = lookup(handle);
    if (!session) return;
    QtEngine::runQueued([session, width, height] { session->resize(width, height); });
}

void DwgCore_ZoomFit(DwgCoreHandle handle) {
    DwgSession *session = lookup(handle);
    if (!session) return;
    QtEngine::runQueued([session] { session->zoomFit(); });
}

void DwgCore_DestroyView(DwgCoreHandle handle) {
    DwgSession *session = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_sessionsMutex);
        auto it = g_sessions.find(handle);
        if (it == g_sessions.end()) return;
        session = it->second;
        g_sessions.erase(it);
    }
    QtEngine::runBlocking([session] { delete session; });
}

void DwgCore_Shutdown() {
    std::unordered_map<DwgCoreHandle, DwgSession *> remaining;
    {
        std::lock_guard<std::mutex> lock(g_sessionsMutex);
        remaining.swap(g_sessions);
    }
    if (!remaining.empty()) {
        QtEngine::runBlocking([&remaining] {
            for (auto &kv : remaining) delete kv.second;
        });
    }
    QtEngine::shutdown();
}
