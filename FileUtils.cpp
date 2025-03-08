// File: FileUtils.cpp
// Version: 1.1
// -------------------------------------------------------------------------
// This file implements functions for launching files and managing the
// tracking mapping persistence. Logging functionality has been removed.
// -------------------------------------------------------------------------

#include "FileUtils.h"
#include "WindowUtils.h"   // For GetMainWindowHandle() and GetWindowHandleByFileName()
#include "FingerprintUtils.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <shellapi.h>

void LaunchFile(const std::wstring &filePath, HWND hwnd, std::map<std::wstring, TrackedWindow>& fileWindowMap) {
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = hwnd;
    sei.lpVerb = L"open";
    sei.lpFile = filePath.c_str();
    sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) {
        const int pollInterval = 250;
        const int maxPollTime = 1000;
        int elapsed = 0;
        HWND hwndLaunched = nullptr;
        while (elapsed < maxPollTime) {
            hwndLaunched = GetMainWindowHandle(GetProcessId(sei.hProcess));
            if (hwndLaunched)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(pollInterval));
            elapsed += pollInterval;
        }
        if (hwndLaunched)
            fileWindowMap[filePath] = GetFingerprint(hwndLaunched);
        CloseHandle(sei.hProcess);
    } else {
        // Removed logging call.
        MessageBox(hwnd, L"Failed to launch file.", L"Error", MB_OK | MB_ICONERROR);
    }
}

void SaveTrackingMapping(const std::wstring &trackingFile, const std::map<std::wstring, TrackedWindow>& fileWindowMap) {
    std::wofstream ofs(trackingFile.c_str());
    if (!ofs) {
        // Removed logging call.
        return;
    }
    for (const auto& pair : fileWindowMap) {
        ofs << pair.first << L"\t" << pair.second.processId << std::endl;
    }
    ofs.close();
}

void LoadTrackingMapping(const std::wstring &trackingFile, std::map<std::wstring, TrackedWindow>& fileWindowMap, HWND hwnd) {
    std::wifstream ifs(trackingFile.c_str());
    if (!ifs) {
        // Removed logging call.
        return;
    }
    std::wstring line;
    while (std::getline(ifs, line)) {
        std::wistringstream iss(line);
        std::wstring filePath;
        DWORD processId;
        if (iss >> filePath >> processId) {
            HWND hwndTracked = GetMainWindowHandle(processId);
            if (!hwndTracked) {
                size_t pos = filePath.rfind(L'\\');
                if (pos != std::wstring::npos) {
                    std::wstring fileName = filePath.substr(pos + 1);
                    hwndTracked = GetWindowHandleByFileName(fileName);
                }
            }
            if (hwndTracked)
                fileWindowMap[filePath] = GetFingerprint(hwndTracked);
        }
    }
    ifs.close();
}

// End of file: FileUtils.cpp (Version: 1.1)
