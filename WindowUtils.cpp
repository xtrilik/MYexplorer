// File: WindowUtils.cpp
// Version: 1.0 (Phase 3.2: Window Utility Functions extracted)
// -------------------------------------------------------------------------
// This file implements window utility functions for locating and matching
// system windows. It includes implementations for:
//   - GetMainWindowHandle()
//   - GetWindowHandleByFileName()
//   - IsFileWindowOpen()
//   - GetPowerPointWindow()
//   - GetWordWindow()
// -------------------------------------------------------------------------

#include "WindowUtils.h"
#include <cwchar>

// -------------------------------------------------------------------------
// GetMainWindowHandle
// -------------------------------------------------------------------------
struct HandleData {
    DWORD processID;
    HWND bestHandle;
};

static BOOL CALLBACK EnumProcMain(HWND hwnd, LPARAM lParam) {
    HandleData* pData = reinterpret_cast<HandleData*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == pData->processID && IsWindowVisible(hwnd)) {
        pData->bestHandle = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND GetMainWindowHandle(DWORD processID) {
    HandleData data = { processID, 0 };
    EnumWindows(EnumProcMain, reinterpret_cast<LPARAM>(&data));
    return data.bestHandle;
}

// -------------------------------------------------------------------------
// GetWindowHandleByFileName
// -------------------------------------------------------------------------
struct CallbackData {
    std::wstring fileName;
    HWND hwndFound;
};

static BOOL CALLBACK EnumProcFile(HWND hwnd, LPARAM lParam) {
    CallbackData* pData = reinterpret_cast<CallbackData*>(lParam);
    int len = GetWindowTextLength(hwnd);
    if (len > 0) {
        std::wstring title(len + 1, L'\0');
        GetWindowText(hwnd, &title[0], len + 1);
        title.resize(wcslen(title.c_str()));
        std::wstring titleLower = title;
        for (auto &c : titleLower) c = towlower(c);
        
        std::wstring fileLower = pData->fileName;
        for (auto &c : fileLower) c = towlower(c);
        fileLower = StripLnkExtension(fileLower);
        
        if (!fileLower.empty() && titleLower.find(fileLower) != std::wstring::npos) {
            pData->hwndFound = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

HWND GetWindowHandleByFileName(const std::wstring &fileName) {
    CallbackData data = { fileName, nullptr };
    EnumWindows(EnumProcFile, reinterpret_cast<LPARAM>(&data));
    return data.hwndFound;
}

// -------------------------------------------------------------------------
// IsFileWindowOpen
// -------------------------------------------------------------------------
struct EnumData {
    std::wstring fileName;
    bool found;
};

static BOOL CALLBACK EnumProcIsFile(HWND hwnd, LPARAM lParam) {
    EnumData* data = reinterpret_cast<EnumData*>(lParam);
    int len = GetWindowTextLength(hwnd);
    if (len > 0) {
        std::wstring title(len + 1, L'\0');
        GetWindowText(hwnd, &title[0], len + 1);
        title.resize(wcslen(title.c_str()));
        if (title.find(data->fileName) != std::wstring::npos) {
            data->found = true;
            return FALSE;
        }
    }
    return TRUE;
}

bool IsFileWindowOpen(const std::wstring &fileName) {
    EnumData data = { fileName, false };
    EnumWindows(EnumProcIsFile, reinterpret_cast<LPARAM>(&data));
    return data.found;
}

// -------------------------------------------------------------------------
// GetPowerPointWindow
// -------------------------------------------------------------------------
HWND GetPowerPointWindow() {
    HWND pptHwnd = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
         wchar_t className[256] = {0};
         if (GetClassName(hwnd, className, 256)) {
              std::wstring cls(className);
              if (cls == L"PPTFrameClass") {
                   *((HWND*)lParam) = hwnd;
                   return FALSE;
              }
         }
         return TRUE;
    }, reinterpret_cast<LPARAM>(&pptHwnd));
    return pptHwnd;
}

// -------------------------------------------------------------------------
// GetWordWindow
// -------------------------------------------------------------------------
HWND GetWordWindow() {
    HWND wordHwnd = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
         wchar_t className[256] = {0};
         if (GetClassName(hwnd, className, 256)) {
              std::wstring cls(className);
              if (cls == L"OpusApp") {
                   *((HWND*)lParam) = hwnd;
                   return FALSE;
              }
         }
         return TRUE;
    }, reinterpret_cast<LPARAM>(&wordHwnd));
    return wordHwnd;
}

// End of file: WindowUtils.cpp (Version: 1.0)
