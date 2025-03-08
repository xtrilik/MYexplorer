// File: WindowMonitor.cpp
// Version: 1.2

#include "WindowMonitor.h"
#include <psapi.h>
#include <sstream>
#include <vector>
#include <cwchar>

// Helper callback used by EnumWindows.
static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    std::vector<WindowInfo>* pWindows = reinterpret_cast<std::vector<WindowInfo>*>(lParam);

    // Only consider visible windows with non-empty titles.
    if (!IsWindowVisible(hwnd))
        return TRUE;
    int length = GetWindowTextLength(hwnd);
    if (length == 0)
        return TRUE;

    std::wstring title(length + 1, L'\0');
    GetWindowText(hwnd, &title[0], length + 1);
    title.resize(wcslen(title.c_str()));
    if(title.empty())
        return TRUE;

    WindowInfo info;
    info.hwnd = hwnd;
    info.title = title;
    GetWindowRect(hwnd, &info.rect);

    // Determine window state using GetWindowPlacement.
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if(GetWindowPlacement(hwnd, &wp)) {
        if(wp.showCmd == SW_SHOWMINIMIZED)
            info.state = WindowState::Minimized;
        else if(wp.showCmd == SW_SHOWMAXIMIZED)
            info.state = WindowState::Maximized;
        else
            info.state = WindowState::Normal;
    } else {
        info.state = WindowState::Unknown;
    }

    // Get the process ID.
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    info.processId = pid;

    // Retrieve process name.
    info.processName = L"";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if(hProcess) {
        wchar_t processPath[MAX_PATH] = {0};
        if(GetModuleFileNameEx(hProcess, NULL, processPath, MAX_PATH))
            info.processName = processPath;
        CloseHandle(hProcess);
    }
    
    // Determine if the window is focused.
    info.isFocused = (hwnd == GetForegroundWindow());
    
    // Retrieve the window's class name.
    wchar_t classBuf[256] = {0};
    if(GetClassName(hwnd, classBuf, 256))
        info.className = classBuf;
    else
        info.className = L"";

    pWindows->push_back(info);
    return TRUE;
}

WindowMonitor::WindowMonitor() : m_hook(nullptr) {}

WindowMonitor::~WindowMonitor() {
    StopMonitoring();
}

std::vector<WindowInfo> WindowMonitor::EnumerateWindows() {
    std::vector<WindowInfo> windows;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

bool WindowMonitor::StartMonitoring() {
    if(m_hook)
        return true;
    m_hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, NULL,
                             WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    return (m_hook != nullptr);
}

void WindowMonitor::StopMonitoring() {
    if(m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
}

void CALLBACK WindowMonitor::WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
                                          LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
    std::wstringstream ss;
    ss << L"WinEvent: event=" << event << L", hwnd=0x" << std::hex << (uintptr_t)hwnd;
    OutputDebugString(ss.str().c_str());
}

WindowState WindowMonitor::GetWindowState(HWND hwnd) {
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    if(GetWindowPlacement(hwnd, &wp)) {
        if(wp.showCmd == SW_SHOWMINIMIZED)
            return WindowState::Minimized;
        else if(wp.showCmd == SW_SHOWMAXIMIZED)
            return WindowState::Maximized;
        else
            return WindowState::Normal;
    }
    return WindowState::Unknown;
}

std::wstring WindowMonitor::GetProcessName(DWORD processId) {
    std::wstring processName;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if(hProcess) {
        wchar_t name[MAX_PATH] = {0};
        if(GetModuleFileNameEx(hProcess, NULL, name, MAX_PATH))
            processName = name;
        CloseHandle(hProcess);
    }
    return processName;
}
