// File: FingerprintUtils.cpp
// Version: 1.0 (Phase 3.1: Fingerprint Utilities extracted)
// -------------------------------------------------------------------------
// This file implements the composite fingerprinting functions.
// It encapsulates logic to capture and compare window fingerprints.
// -------------------------------------------------------------------------

#include "FingerprintUtils.h"
#include <cwchar>

// Implementation of GetFingerprint()
TrackedWindow GetFingerprint(HWND hwnd) {
    TrackedWindow tw;
    tw.processId = GetProcessId(hwnd);
    tw.hwnd = hwnd;
    GetWindowRect(hwnd, &tw.rect);
    wchar_t classBuf[256] = {0};
    GetClassName(hwnd, classBuf, 256);
    tw.className = classBuf;
    int titleLen = GetWindowTextLength(hwnd);
    if (titleLen > 0) {
        std::wstring title(titleLen + 1, L'\0');
        GetWindowText(hwnd, &title[0], titleLen + 1);
        title.resize(wcslen(title.c_str()));
        tw.windowTitle = title;
    } else {
        tw.windowTitle = L"";
    }
    tw.failCount = 0;
    tw.launchTime = GetTickCount64();
    return tw;
}

// Implementation of CompareFingerprints()
bool CompareFingerprints(const TrackedWindow &twStored, const TrackedWindow &twCurrent) {
    if (twStored.processId != twCurrent.processId)
        return false;
    if (twStored.className != twCurrent.className)
        return false;
    if (twStored.rect.left != twCurrent.rect.left ||
        twStored.rect.top != twCurrent.rect.top ||
        twStored.rect.right != twCurrent.rect.right ||
        twStored.rect.bottom != twCurrent.rect.bottom)
        return false;
    std::wstring storedTitle = twStored.windowTitle;
    std::wstring currentTitle = twCurrent.windowTitle;
    for (auto &c : storedTitle) c = towlower(c);
    for (auto &c : currentTitle) c = towlower(c);
    return (storedTitle == currentTitle);
}

// Implementation of CompareStableAttributes()
bool CompareStableAttributes(const TrackedWindow &stored, const TrackedWindow &current) {
    return (stored.processId == current.processId &&
            stored.className == current.className);
}

// End of file: FingerprintUtils.cpp (Version: 1.0)
