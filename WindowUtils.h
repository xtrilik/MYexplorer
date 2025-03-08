// File: WindowUtils.h
// Version: 1.0 (Phase 3.2: Window Utility Functions extracted)
// -------------------------------------------------------------------------
// This header declares window utility functions used to locate and match
// system windows. Functions include:
//   - GetMainWindowHandle()
//   - GetWindowHandleByFileName()
//   - IsFileWindowOpen()
//   - GetPowerPointWindow()
//   - GetWordWindow()
// Additionally, an inline helper StripLnkExtension() is provided.
// -------------------------------------------------------------------------

#ifndef WINDOWUTILS_H
#define WINDOWUTILS_H

#include <windows.h>
#include <string>

// Returns the main window handle for a given process ID.
HWND GetMainWindowHandle(DWORD processID);

// Returns a window handle by matching a file name substring in the window's title.
HWND GetWindowHandleByFileName(const std::wstring &fileName);

// Checks if any window with a title containing fileName is open.
bool IsFileWindowOpen(const std::wstring &fileName);

// Returns the handle of the PowerPoint window (if found).
HWND GetPowerPointWindow();

// Returns the handle of the Word window (if found).
HWND GetWordWindow();

// Inline function to strip the ".lnk" extension from a file name.
inline std::wstring StripLnkExtension(const std::wstring &fileLower) {
    static const std::wstring lnkExt = L".lnk";
    if (fileLower.size() >= lnkExt.size()) {
        if (0 == fileLower.compare(fileLower.size() - lnkExt.size(), lnkExt.size(), lnkExt))
            return fileLower.substr(0, fileLower.size() - lnkExt.size());
    }
    return fileLower;
}

#endif // WINDOWUTILS_H

// End of file: WindowUtils.h (Version: 1.0)
