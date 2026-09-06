#pragma once

#include <functional>

// Owns the single QApplication instance this DLL ever creates, and the one
// dedicated background thread it runs on. Every Qt object dwgcore creates
// (ViewerWidget instances included) lives on that thread and must only be
// touched from it.
//
// Why a dedicated thread at all: Qt wants to own the thread it runs its
// event loop on (QApplication::exec()), but a WPF host process's main
// thread already runs WPF's own dispatcher loop -- the two can't share a
// thread. Running Qt on its own worker thread inside this DLL, and
// marshaling every call in from the outside via runBlocking()/runQueued(),
// is what lets a P/Invoke call from WPF's UI thread reach Qt safely.
//
// Callers (dwgcore_api.cpp) never touch a QObject directly -- every
// exported function's body is just "pick blocking or queued, put the real
// work in the lambda". See dwgcore_api.cpp's top-of-file comment.
namespace QtEngine {

// Starts the worker thread and constructs QApplication on it if this is the
// first call; a no-op otherwise. Safe to call from any thread; blocks the
// caller until the engine is ready to accept work. runBlocking()/
// runQueued() both call this themselves, so callers don't need to.
void ensureStarted();

// Runs `fn` on the Qt thread and blocks the calling thread until it
// completes. Safe to call from a thread with no Qt event loop of its own
// (e.g. a P/Invoke call arriving on WPF's UI thread) -- Qt's
// BlockingQueuedConnection handles the cross-thread wait internally; it
// only requires the *receiving* thread (the Qt one) to be pumping events.
void runBlocking(const std::function<void()> &fn);

// Runs `fn` on the Qt thread without waiting for it to finish. Use for
// high-frequency or fire-and-forget calls (e.g. a live resize) so the
// caller's thread is never blocked on Qt work.
void runQueued(const std::function<void()> &fn);

// Quits the QApplication event loop and joins the worker thread. Must be
// called explicitly before the hosting process exits (e.g. from C#'s
// App.OnExit) -- there is deliberately no DllMain-based cleanup, since
// joining a thread during DLL_PROCESS_DETACH is unsafe and can deadlock.
// Safe to call even if ensureStarted() was never called. Not safe to call
// concurrently with itself or with ensureStarted().
void shutdown();

} // namespace QtEngine
