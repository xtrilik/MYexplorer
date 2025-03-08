// File: WindowMonitor.h
// Version: 1.2

#ifndef WINDOWMONITOR_H
#define WINDOWMONITOR_H

#include <windows.h>
#include <string>
#include <vector>

enum class WindowState {
    Normal,
    Minimized,
    Maximized,
    Unknown
};

struct WindowInfo {
    HWND hwnd;
    std::wstring title;
    RECT rect;
    WindowState state;
    DWORD processId;
    std::wstring processName;
    bool isFocused;         // Indicates whether the window is focused.
    std::wstring className; // The window's class name.
};

class WindowMonitor {
public:
    WindowMonitor();
    ~WindowMonitor();

    // Enumerate all top-level windows.
    std::vector<WindowInfo> EnumerateWindows();

    // (Optional) Real-time monitoring using WinEventHook.
    bool StartMonitoring();
    void StopMonitoring();

private:
    // Helper to determine a window's state.
    WindowState GetWindowState(HWND hwnd);

    // Helper to get the process name given a process ID.
    std::wstring GetProcessName(DWORD processId);

    // Callback for WinEventHook (if needed).
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
                                      LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

    HWINEVENTHOOK m_hook;
};

#endif // WINDOWMONITOR_H
