// Pidufi: lector de PDF simple (Win32 + MuPDF).
#include "main_window.h"
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    MainWindow window;
    if (!window.create(inst, show)) return 1;

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) window.openFile(argv[1]);
    LocalFree(argv);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
