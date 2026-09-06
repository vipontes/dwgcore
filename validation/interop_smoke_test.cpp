// Standalone smoke test for dwgcore.dll's public C API, with no WPF/HwndHost
// involved -- isolates whether a hang/crash is in the native DLL's
// threading (QtEngine/DwgSession) or in the C# interop layer around it.
// Prints a line after every call so a hang shows exactly where it stuck.

#include <cstdio>
#include <windows.h>

#include "../src/dwgcore_api.h"

int main(int argc, char *argv[]) {
    std::fprintf(stderr, "[1] calling DwgCore_CreateView...\n");
    std::fflush(stderr);
    DwgCoreHandle handle = DwgCore_CreateView();
    std::fprintf(stderr, "[1] returned handle=%p\n", handle);
    if (!handle) {
        std::fprintf(stderr, "FAILED: null handle\n");
        return 1;
    }

    std::fprintf(stderr, "[2] calling DwgCore_GetNativeHandle...\n");
    std::fflush(stderr);
    void *hwnd = DwgCore_GetNativeHandle(handle);
    std::fprintf(stderr, "[2] returned hwnd=%p\n", hwnd);

    if (argc >= 2) {
        std::fprintf(stderr, "[3] calling DwgCore_LoadFile(%s)...\n", argv[1]);
        std::fflush(stderr);
        bool ok = DwgCore_LoadFile(handle, argv[1]);
        std::fprintf(stderr, "[3] returned ok=%d\n", ok ? 1 : 0);
        if (!ok) {
            char err[512];
            DwgCore_GetLastError(handle, err, sizeof(err));
            std::fprintf(stderr, "    error: %s\n", err);
        }
    }

    std::fprintf(stderr, "[4] calling DwgCore_SetVisible(true)...\n");
    std::fflush(stderr);
    DwgCore_SetVisible(handle, true);
    std::fprintf(stderr, "[4] returned\n");

    Sleep(500);

    std::fprintf(stderr, "[5] calling DwgCore_DestroyView...\n");
    std::fflush(stderr);
    DwgCore_DestroyView(handle);
    std::fprintf(stderr, "[5] returned\n");

    std::fprintf(stderr, "[6] calling DwgCore_Shutdown...\n");
    std::fflush(stderr);
    DwgCore_Shutdown();
    std::fprintf(stderr, "[6] returned -- SUCCESS\n");
    return 0;
}
