#include "qt_engine.h"

#include <QApplication>
#include <QMetaObject>
#include <QObject>

#include <clocale>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace QtEngine {
namespace {

std::mutex g_lifecycleMutex; // guards start/shutdown sequencing itself
bool g_started = false;
std::thread g_thread;

std::mutex g_readyMutex; // guards g_ready/g_dispatcher, signaled by g_readyCv
std::condition_variable g_readyCv;
bool g_ready = false;
QObject *g_dispatcher = nullptr; // lives on the Qt thread; used only as an
                                  // invokeMethod() context/thread anchor

void threadMain() {
    // QApplication keeps a reference to argc for its lifetime; give it
    // static storage rather than a stack frame that's about to be reused.
    static int argc = 1;
    static char appName[] = "dwgcore";
    static char *argv[] = {appName, nullptr};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt6 always scales per-monitor; this attribute is Qt5-only and must be
    // set before QApplication is constructed.
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // libdxfrw's DXF number parser uses locale-sensitive strtod() and DXF
    // files always use '.' as the decimal separator -- same fix as
    // dwgviewer/src/main.cpp, needed here too since QApplication adopts the
    // system locale on construction.
    std::setlocale(LC_NUMERIC, "C");

    QObject dispatcher;
    {
        std::lock_guard<std::mutex> lock(g_readyMutex);
        g_dispatcher = &dispatcher;
        g_ready = true;
    }
    g_readyCv.notify_all();

    app.exec();

    // Back on this thread after the event loop stops: tear down before
    // `app`/`dispatcher` go out of scope, so no other thread can reach them
    // mid-destruction.
    std::lock_guard<std::mutex> lock(g_readyMutex);
    g_dispatcher = nullptr;
    g_ready = false;
}

} // namespace

void ensureStarted() {
    std::lock_guard<std::mutex> lifecycleLock(g_lifecycleMutex);
    if (g_started) return;
    g_started = true;
    g_thread = std::thread(threadMain);

    std::unique_lock<std::mutex> lock(g_readyMutex);
    g_readyCv.wait(lock, [] { return g_ready; });
}

void runBlocking(const std::function<void()> &fn) {
    ensureStarted();
    QObject *dispatcher;
    {
        std::lock_guard<std::mutex> lock(g_readyMutex);
        dispatcher = g_dispatcher;
    }
    if (!dispatcher) return; // shutting down concurrently; nothing to do
    QMetaObject::invokeMethod(dispatcher, fn, Qt::BlockingQueuedConnection);
}

void runQueued(const std::function<void()> &fn) {
    ensureStarted();
    QObject *dispatcher;
    {
        std::lock_guard<std::mutex> lock(g_readyMutex);
        dispatcher = g_dispatcher;
    }
    if (!dispatcher) return;
    QMetaObject::invokeMethod(dispatcher, fn, Qt::QueuedConnection);
}

void shutdown() {
    std::lock_guard<std::mutex> lifecycleLock(g_lifecycleMutex);
    if (!g_started) return;

    QObject *dispatcher;
    {
        std::lock_guard<std::mutex> lock(g_readyMutex);
        dispatcher = g_dispatcher;
    }
    if (dispatcher) {
        QMetaObject::invokeMethod(dispatcher, [] { qApp->quit(); }, Qt::QueuedConnection);
    }
    if (g_thread.joinable()) g_thread.join();
    g_started = false;
}

} // namespace QtEngine
