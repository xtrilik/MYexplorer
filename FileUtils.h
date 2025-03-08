// File: FileUtils.h
// Version: 1.0 (Phase 3.3: File Launching & Persistence modularized)
// -------------------------------------------------------------------------
// This header declares functions for launching files and managing the
// tracking persistence mapping.
// Functions include:
//   - LaunchFile()
//   - SaveTrackingMapping()
//   - LoadTrackingMapping()
// The functions use FingerprintUtils to capture window fingerprints and
// WindowUtils to locate windows.
// -------------------------------------------------------------------------

#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <windows.h>
#include <string>
#include <map>
#include "FingerprintUtils.h" // For TrackedWindow

// Launches the file specified by filePath using ShellExecuteExW.
// The parent window is specified by hwnd, and upon success, updates the
// fileWindowMap with the tracked window fingerprint.
void LaunchFile(const std::wstring &filePath, HWND hwnd, std::map<std::wstring, TrackedWindow>& fileWindowMap);

// Saves the tracking mapping (file path and process ID) to the given trackingFile.
void SaveTrackingMapping(const std::wstring &trackingFile, const std::map<std::wstring, TrackedWindow>& fileWindowMap);

// Loads the tracking mapping from the given trackingFile and updates fileWindowMap.
// Uses window utility functions to locate the corresponding window handles.
void LoadTrackingMapping(const std::wstring &trackingFile, std::map<std::wstring, TrackedWindow>& fileWindowMap, HWND hwnd);

#endif // FILEUTILS_H

// End of file: FileUtils.h (Version: 1.0)
