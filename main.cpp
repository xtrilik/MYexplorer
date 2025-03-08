// File: main.cpp
// Version: 1.4

#include <windows.h>
#include "MainWindow.h"
#include <commctrl.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Initialize common controls (for the ListView control)
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    // Create and show the main window with increased dimensions.
    MainWindow mainWindow;
    if (!mainWindow.Create(L"MYexplorer - Monitoring App", WS_OVERLAPPEDWINDOW, 0, 
                             CW_USEDEFAULT, CW_USEDEFAULT, 1600, 900))
    {
        MessageBox(NULL, L"Window creation failed!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }
    
    // Ensure the window is shown normally and updated immediately.
    ShowWindow(mainWindow.GetHwnd(), SW_SHOWNORMAL);
    UpdateWindow(mainWindow.GetHwnd());

    // Standard message loop.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!IsDialogMessage(mainWindow.GetHwnd(), &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return static_cast<int>(msg.wParam);
}
