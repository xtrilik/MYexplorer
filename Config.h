// File: Config.h
// Version: 1.1
// -------------------------------------------------------------------------
// This file centralizes configuration constants for MYexplorer.
// Externalized constants include the project folder, timer configuration,
// persistence file name, and UI layout values.
//
// Changes in Version 1.1:
//  - Moved hard-coded constants from various modules into this file.
//  - Added #include <windows.h> to ensure Windows-specific types (e.g., UINT_PTR, UINT)
//    are defined, resolving IntelliSense errors.
// -------------------------------------------------------------------------

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>  // Required for UINT_PTR, UINT, etc.
#include <string>

// Folder where the application monitors files.
const std::wstring PROJECT_FOLDER = L"C:\\tmp";

// Timer configuration.
const UINT_PTR TIMER_ID = 1;          // Timer identifier.
const UINT TIMER_INTERVAL = 500;      // Timer interval for updating UI (in milliseconds).

// File used to persist window tracking mapping.
const std::wstring TRACKING_FILE = L"tracking.dat";

// UI and launcher button settings.
const int ID_CLOSE_WINDOW = 2001;
const int MAX_LAUNCHER_BUTTONS = 5;
const int ID_LAUNCHER_BUTTON_BASE = 3000;

// Layout constants for the dynamic launcher buttons.
const int BUTTON_ROW_Y = 850;
const int BUTTON_X_START = 10;
const int BUTTON_WIDTH = 120;
const int BUTTON_HEIGHT = 30;
const int BUTTON_X_GAP = 10;

#endif // CONFIG_H

// End of file: Config.h (Version: 1.1)
