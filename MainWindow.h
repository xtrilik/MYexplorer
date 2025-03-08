// ============================ 
// File: MainWindow.h
// Version: 1.59.7 (Added integrated browser feature with BrowserPanel integration)
// -------------------------------------------------------------------------
// This file declares the MainWindow class for MYexplorer.
// Changes in this version:
// - Added a new member variable, m_browserPanel, to manage an integrated browser.
// - Added methods ShowIntegratedBrowser() and HideIntegratedBrowser() to display and hide the browser panel.
// - Updated layout handling to support a split view when the browser is active.
// -------------------------------------------------------------------------

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>
#include <string>
#include <map>
#include <vector>
#include "WindowMonitor.h"  // For WindowInfo definition
#include "BrowserPanel.h"   // For integrated browser feature

/**
 * @struct TrackedWindow
 * @brief Extended fingerprint for a tracked window.
 *
 * Contains process ID, window handle, geometry, class name, window title,
 * a failure counter, and the launch time.
 */
struct TrackedWindow {
    DWORD processId;
    HWND hwnd;
    RECT rect;
    std::wstring className;
    std::wstring windowTitle;
    int failCount;         ///< Initialized to 0 when a new entry is added.
    ULONGLONG launchTime;  ///< Timestamp (in ms) when the window was first captured.
};

/**
 * @class MainWindow
 * @brief Main application window for MYexplorer.
 *
 * Manages UI creation, tab-based layout (File Tracking, Window Monitoring, CLI),
 * file tracking, dynamic launcher buttons, window monitoring, and an integrated browser.
 */
class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    BOOL Create(LPCWSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle,
                int x, int y, int nWidth, int nHeight, HWND hWndParent = nullptr);
    HWND GetHwnd() const { return m_hwnd; }

private:
    // Main window and container panel handles.
    HWND m_hwnd;                   ///< Main window handle.
    HWND m_hTabControl;            ///< Handle to the tab control.
    HWND m_hPanelFileTracking;     ///< Container panel for File Tracking.
    HWND m_hPanelWindowMonitoring; ///< Container panel for Window Monitoring.
    HWND m_hPanelCLI;              ///< Container panel for the CLI feature.
    
    // Embedded child controls.
    HWND m_hListViewFileTracking;      ///< ListView for file tracking.
    HWND m_hListViewWindowMonitoring;  ///< ListView for window monitoring.
    HWND m_hCLIEdit;                   ///< EDIT control for CLI input.
    HWND m_hCLIListView;               ///< ListView for file list in CLI.

    std::map<std::wstring, TrackedWindow> m_fileWindowMap;  ///< Mapping from file path to window fingerprint.
    std::vector<WindowInfo> m_prevWindowList;               ///< Previous window list for bottom panel.

    std::map<std::wstring, bool> m_launcherMap;             ///< Indicates which files are marked as launchers.
    std::map<std::wstring, HWND> m_fileButtons;             ///< Mapping of file paths to dynamic launcher button HWNDs.
    std::map<UINT, std::wstring> m_launcherButtonMap;       ///< Mapping of launcher button control IDs to file paths.

    // Integrated browser panel for URL files.
    BrowserPanel* m_browserPanel;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void PopulateListView();       // Populates the File Tracking ListView.
    void PopulateCLIListView();    // Populates the CLI ListView.
    void RefreshLauncherButtons();
    void InitListViewControls();   // Initialize columns for ListView controls.

    // New: Filter the CLI ListView based on the current input.
    void FilterCLIListView(const std::wstring &filter);

    // Methods for handling tab changes.
    void CreateTabControl();
    void SwitchPanel(int tabIndex);

    // --- New: CLI edit control subclass procedure as a private static member ---
    static LRESULT CALLBACK CLIEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    // --- New: Helper function to handle launcher button clicks ---
    // This function encapsulates the logic for activating or launching a file.
    void HandleLauncherButtonClick(const std::wstring &filePath);

    // --- New: Integrated browser feature methods ---
    // Show the integrated browser with the given URL in a split view.
    void ShowIntegratedBrowser(const std::wstring &url);
    // Hide the integrated browser and revert to the previous layout.
    void HideIntegratedBrowser();
};

#endif // MAINWINDOW_H

// ---------------------------- End of file: MainWindow.h (Version: 1.59.7) ----------------------------
