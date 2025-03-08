// ============================ 
// File: MainWindow.cpp
// Version: 1.61.0 (Added .url detection and ShowIntegratedBrowser call)
// -------------------------------------------------------------------------
// This file implements the MainWindow class for MYexplorer.
//
// Changes in this version (1.61.0):
// 1) Added a ShowIntegratedBrowser(const std::wstring &url) method, which creates a BrowserPanel
//    and navigates to the given URL in an embedded WebView2 browser.
// 2) In both the CLI and File Tracking double-click logic, if the file ends with ".url",
//    we read the file's "URL=" line and call ShowIntegratedBrowser(url). This ensures
//    that .url files open in the integrated browser, rather than in the default system browser.
//
// NOTE: This file is presented in full to comply with the directive. Other code is unchanged
//       from version 1.60.0 except for the new .url logic and ShowIntegratedBrowser.
//
// -------------------------------------------------------------------------

#include "MainWindow.h"
#include "WindowMonitor.h"        // For WindowInfo and window enumeration functions
#include "FingerprintUtils.h"     // For composite fingerprinting functions
#include "WindowUtils.h"          // For window utility functions
#include "FileUtils.h"            // For file launching & persistence functions
#include "Config.h"               // Externalized configuration constants
#include "BrowserPanel.h"         // For the integrated browser feature
#include <commctrl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <exception>
#include <shellapi.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <cwctype>
#include <windowsx.h>

namespace fs = std::filesystem;

#ifndef LVM_SETTOPINDEX
#define LVM_SETTOPINDEX (LVM_FIRST + 154)
#endif

#ifndef ListView_SetTopIndex
#define ListView_SetTopIndex(hwnd, i) ((void)SendMessage((hwnd), LVM_SETTOPINDEX, (WPARAM)(i), 0L))
#endif

#ifndef ListView_SetRedraw
#define ListView_SetRedraw(hwnd, fRedraw) ((void)SendMessage((hwnd), WM_SETREDRAW, (WPARAM)(fRedraw), 0))
#endif

// -------------------------------------------------------------------------
// Reintroduced definitions to fix missing references
// -------------------------------------------------------------------------
static const int TAB_CONTROL_HEIGHT = 30;

// Helper for returning a string describing the window's current state.
static std::wstring GetStateString(HWND hwnd) {
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(WINDOWPLACEMENT);
    std::wstring stateStr;
    if (GetWindowPlacement(hwnd, &wp)) {
        if (wp.showCmd == SW_SHOWMINIMIZED)
            stateStr = L"Minimized";
        else if (wp.showCmd == SW_SHOWMAXIMIZED)
            stateStr = L"Maximized";
        else
            stateStr = L"Normal";
    } else {
        stateStr = L"Unknown";
    }
    if (hwnd == GetForegroundWindow())
        stateStr += L" (Focused)";
    return stateStr;
}

// Compare two window lists for equality
static bool WindowListsEqual(const std::vector<WindowInfo>& a, const std::vector<WindowInfo>& b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].title       != b[i].title       ||
            a[i].processName != b[i].processName ||
            a[i].state       != b[i].state       ||
            a[i].className   != b[i].className   ||
            a[i].rect.left   != b[i].rect.left   ||
            a[i].rect.top    != b[i].rect.top    ||
            a[i].rect.right  != b[i].rect.right  ||
            a[i].rect.bottom != b[i].rect.bottom)
            return false;
    }
    return true;
}

// -------------------------------------------------------------------------
// Helper for Window Activation
// -------------------------------------------------------------------------
static void ActivateTrackedWindow(TrackedWindow &tw) {
    if (IsIconic(tw.hwnd))
        ShowWindow(tw.hwnd, SW_RESTORE);
    SetForegroundWindow(tw.hwnd);
}

// -------------------------------------------------------------------------
// PanelSubclassProc: Forward WM_NOTIFY messages from container panel to main window.
// -------------------------------------------------------------------------
static LRESULT CALLBACK PanelSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                            UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_NOTIFY) {
        HWND hMain = GetParent(hWnd);
        return SendMessage(hMain, uMsg, wParam, lParam);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// -------------------------------------------------------------------------
// MainWindow: ShowIntegratedBrowser
// Creates a new BrowserPanel (WebView2) on the right side, in a split view.
// -------------------------------------------------------------------------
void MainWindow::ShowIntegratedBrowser(const std::wstring &url) {
    if (!m_browserPanel) {
        m_browserPanel = new BrowserPanel();
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int leftPaneWidth = rc.right / 3; // left pane = 1/3 width
        RECT browserRect = { leftPaneWidth, TAB_CONTROL_HEIGHT, rc.right, rc.bottom };
        if (!m_browserPanel->Create(m_hwnd, browserRect)) {
            delete m_browserPanel;
            m_browserPanel = nullptr;
            MessageBox(m_hwnd, L"Failed to create integrated browser panel.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
        // Optionally hide other panels to emphasize the split view
        ShowWindow(m_hPanelFileTracking, SW_SHOW); // keep file tracking visible
        ShowWindow(m_hPanelWindowMonitoring, SW_HIDE);
        ShowWindow(m_hPanelCLI, SW_HIDE);
    }
    m_browserPanel->Navigate(url);
    // Force a resize event to ensure correct layout
    SendMessage(m_hwnd, WM_SIZE, 0, 0);
}

// -------------------------------------------------------------------------
// CLIEditSubclassProc: Handle key input in the CLI edit control.
// -------------------------------------------------------------------------
LRESULT CALLBACK MainWindow::CLIEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            wchar_t buffer[256] = {0};
            GetWindowText(hWnd, buffer, 256);
            std::wstring input(buffer);
            size_t start = input.find_first_not_of(L" \t");
            size_t end = input.find_last_not_of(L" \t");
            if (start != std::wstring::npos && end != std::wstring::npos)
                input = input.substr(start, end - start + 1);
            else
                input = L"";
            if (input.empty()) {
                MessageBox(pThis->GetHwnd(), L"Please enter a file name.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }
            // Check if file is .url
            std::wstring filePath = PROJECT_FOLDER + L"\\" + input;
            std::wstring fileLower = filePath;
            for (auto &c : fileLower) c = towlower(c);
            if (fileLower.size() >= 4 && fileLower.rfind(L".url") == fileLower.size() - 4) {
                // It's a .url file; read the URL= line and show integrated browser
                std::wifstream ifs(filePath.c_str());
                if (!ifs) {
                    MessageBox(pThis->GetHwnd(), L"Failed to open .url file.", L"Error", MB_OK | MB_ICONERROR);
                    return 0;
                }
                std::wstring line, url;
                while (std::getline(ifs, line)) {
                    if (line.find(L"URL=") == 0) {
                        url = line.substr(4);
                        break;
                    }
                }
                ifs.close();
                if (!url.empty()) {
                    pThis->ShowIntegratedBrowser(url);
                    // Clear the edit control and reset the CLI ListView.
                    SetWindowText(hWnd, L"");
                    pThis->PopulateCLIListView();
                    return 0;
                }
                // If we fail to parse URL=, fallback to normal logic
            }
            // Otherwise, do normal logic
            auto it = pThis->m_fileWindowMap.find(filePath);
            if (it != pThis->m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                TrackedWindow currentFp = GetFingerprint(it->second.hwnd);
                if (!CompareStableAttributes(it->second, currentFp))
                    it->second = currentFp;
                ActivateTrackedWindow(it->second);
            } else {
                HWND hwndFound = GetWindowHandleByFileName(input);
                if (hwndFound) {
                    ActivateTrackedWindow(pThis->m_fileWindowMap[filePath] = GetFingerprint(hwndFound));
                } else {
                    LaunchFile(filePath, pThis->GetHwnd(), pThis->m_fileWindowMap);
                }
            }
            // Clear the edit control and reset the CLI ListView.
            SetWindowText(hWnd, L"");
            pThis->PopulateCLIListView();
            return 0;
        }
        else if (wParam == VK_TAB) {
            // Basic auto-completion...
            wchar_t buffer[256] = {0};
            GetWindowText(hWnd, buffer, 256);
            std::wstring input(buffer);
            size_t start = input.find_first_not_of(L" \t");
            size_t end = input.find_last_not_of(L" \t");
            if (start != std::wstring::npos && end != std::wstring::npos)
                input = input.substr(start, end - start + 1);
            else
                input = L"";
            if (input.empty()) {
                return 0;
            }
            int count = ListView_GetItemCount(pThis->m_hCLIListView);
            if (count == 0)
                return 0;
            std::wstring commonPrefix;
            {
                wchar_t itemText[256] = {0};
                LVITEM lvi = {0};
                lvi.mask = LVIF_TEXT;
                lvi.iItem = 0;
                lvi.iSubItem = 0;
                lvi.pszText = itemText;
                lvi.cchTextMax = 256;
                ListView_GetItem(pThis->m_hCLIListView, &lvi);
                commonPrefix = itemText;
            }
            for (int i = 1; i < count; ++i) {
                wchar_t itemText[256] = {0};
                LVITEM lvi = {0};
                lvi.mask = LVIF_TEXT;
                lvi.iItem = i;
                lvi.iSubItem = 0;
                lvi.pszText = itemText;
                lvi.cchTextMax = 256;
                ListView_GetItem(pThis->m_hCLIListView, &lvi);
                std::wstring currentItem(itemText);
                size_t j = 0;
                while (j < commonPrefix.size() && j < currentItem.size() &&
                       towlower(commonPrefix[j]) == towlower(currentItem[j])) {
                    ++j;
                }
                commonPrefix = commonPrefix.substr(0, j);
            }
            if (commonPrefix.size() > input.size()) {
                SetWindowText(hWnd, commonPrefix.c_str());
                int len = GetWindowTextLength(hWnd);
                SendMessage(hWnd, EM_SETSEL, len, len);
            }
            return 0;
        }
    }
    else if (uMsg == WM_KEYUP) {
        if (wParam != VK_RETURN && wParam != VK_TAB) {
            wchar_t buffer[256] = {0};
            GetWindowText(hWnd, buffer, 256);
            std::wstring currentText(buffer);
            size_t start = currentText.find_first_not_of(L" \t");
            size_t end = currentText.find_last_not_of(L" \t");
            if (start != std::wstring::npos && end != std::wstring::npos)
                currentText = currentText.substr(start, end - start + 1);
            else
                currentText = L"";
            pThis->FilterCLIListView(currentText);
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// -------------------------------------------------------------------------
// FilterCLIListView: Rebuild the CLI ListView with files matching the filter.
// -------------------------------------------------------------------------
void MainWindow::FilterCLIListView(const std::wstring &filter) {
    ListView_DeleteAllItems(m_hCLIListView);
    int index = 0;
    try {
        for (const auto &entry : fs::directory_iterator(PROJECT_FOLDER)) {
            if (entry.is_regular_file()) {
                std::wstring fileName = entry.path().filename().wstring();
                std::wstring fileNameLower = fileName;
                std::wstring filterLower = filter;
                for (auto &c : fileNameLower)
                    c = towlower(c);
                for (auto &c : filterLower)
                    c = towlower(c);
                if (filterLower.empty() || fileNameLower.find(filterLower) == 0) {
                    LVITEM item = {0};
                    item.mask = LVIF_TEXT;
                    item.iItem = index;
                    item.iSubItem = 0;
                    item.pszText = const_cast<LPWSTR>(fileName.c_str());
                    ListView_InsertItem(m_hCLIListView, &item);
                    LVITEM statusItem = {0};
                    statusItem.mask = LVIF_TEXT;
                    statusItem.iItem = index;
                    statusItem.iSubItem = 1;
                    statusItem.pszText = const_cast<LPWSTR>(L"Not launched");
                    ListView_SetItem(m_hCLIListView, &statusItem);
                    ++index;
                }
            }
        }
    }
    catch (const std::exception &e) {
        MessageBox(m_hwnd, L"Failed to enumerate project folder for CLI. Please check the PROJECT_FOLDER path.",
                   L"Error", MB_OK | MB_ICONERROR);
    }
}

// -------------------------------------------------------------------------
// RefreshLauncherButtons (Dynamic launcher buttons)
// -------------------------------------------------------------------------
void MainWindow::RefreshLauncherButtons() {
    // Clear existing buttons
    for (const auto &pair : m_fileButtons) {
        if (IsWindow(pair.second))
            DestroyWindow(pair.second);
    }
    m_fileButtons.clear();
    m_launcherButtonMap.clear();
    int buttonIndex = 0;
    for (const auto &entry : m_launcherMap) {
        if (entry.second) {
            if (buttonIndex >= MAX_LAUNCHER_BUTTONS)
                break;
            int xPos = BUTTON_X_START + buttonIndex * (BUTTON_WIDTH + BUTTON_X_GAP);
            int yPos = BUTTON_ROW_Y;
            const std::wstring &filePath = entry.first;
            size_t pos = filePath.find_last_of(L"\\");
            std::wstring buttonLabel = (pos != std::wstring::npos) ? filePath.substr(pos + 1) : filePath;
            UINT ctrlId = ID_LAUNCHER_BUTTON_BASE + buttonIndex;

            HWND hButton = CreateWindowEx(
                0,
                L"BUTTON",
                buttonLabel.c_str(),
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                xPos,
                yPos,
                BUTTON_WIDTH,
                BUTTON_HEIGHT,
                m_hwnd,
                (HMENU)(UINT_PTR)(ctrlId),
                GetModuleHandle(nullptr),
                nullptr
            );

            if (hButton) {
                m_fileButtons[filePath] = hButton;
                m_launcherButtonMap[ctrlId] = filePath;
            }
            ++buttonIndex;
        }
    }
}

// -------------------------------------------------------------------------
// Tab Control and Panel Encapsulation
// -------------------------------------------------------------------------
static const int TAB_CONTROL_HEADER_HEIGHT = 30; // example if needed

void MainWindow::CreateTabControl() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    m_hTabControl = CreateWindowEx(0, WC_TABCONTROL, L"",
                                    WS_CHILD | WS_VISIBLE | TCS_TABS,
                                    0, 0, rc.right, TAB_CONTROL_HEIGHT,
                                    m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    if (m_hTabControl) {
         TCITEM tie = {0};
         tie.mask = TCIF_TEXT;
         tie.pszText = const_cast<LPWSTR>(L"File Tracking");
         TabCtrl_InsertItem(m_hTabControl, 0, &tie);
         tie.pszText = const_cast<LPWSTR>(L"Window Monitoring");
         TabCtrl_InsertItem(m_hTabControl, 1, &tie);
         tie.pszText = const_cast<LPWSTR>(L"CLI");
         TabCtrl_InsertItem(m_hTabControl, 2, &tie);
    }
}

void MainWindow::SwitchPanel(int tabIndex) {
    if (tabIndex == 0) {
         ShowWindow(m_hPanelFileTracking, SW_SHOW);
         ShowWindow(m_hPanelWindowMonitoring, SW_HIDE);
         ShowWindow(m_hPanelCLI, SW_HIDE);
    } else if (tabIndex == 1) {
         ShowWindow(m_hPanelFileTracking, SW_HIDE);
         ShowWindow(m_hPanelWindowMonitoring, SW_SHOW);
         ShowWindow(m_hPanelCLI, SW_HIDE);
    } else if (tabIndex == 2) {
         ShowWindow(m_hPanelFileTracking, SW_HIDE);
         ShowWindow(m_hPanelWindowMonitoring, SW_HIDE);
         ShowWindow(m_hPanelCLI, SW_SHOW);
         SetFocus(m_hCLIEdit);
    }
}

// -------------------------------------------------------------------------
// InitListViewControls: Initialize columns for ListView controls
// -------------------------------------------------------------------------
void MainWindow::InitListViewControls() {
    // File Tracking ListView: 2 columns
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = const_cast<LPWSTR>(L"File Name");
    lvc.cx = 200;
    ListView_InsertColumn(m_hListViewFileTracking, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(L"Status");
    lvc.cx = 150;
    ListView_InsertColumn(m_hListViewFileTracking, 1, &lvc);

    // Window Monitoring ListView: 10 columns
    const wchar_t* wmCols[10] = {
        L"Associated Files", L"Window Title", L"Process Name", L"State",
        L"HWND", L"Class Name", L"Left", L"Top", L"Right", L"Bottom"
    };
    int wmColWidths[10] = { 150, 200, 200, 100, 100, 150, 50, 50, 50, 50 };
    for (int i = 0; i < 10; ++i) {
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = const_cast<LPWSTR>(wmCols[i]);
        lvc.cx = wmColWidths[i];
        ListView_InsertColumn(m_hListViewWindowMonitoring, i, &lvc);
    }

    // CLI ListView: 2 columns (File Name, Status)
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.pszText = const_cast<LPWSTR>(L"File Name");
    lvc.cx = 200;
    ListView_InsertColumn(m_hCLIListView, 0, &lvc);
    lvc.pszText = const_cast<LPWSTR>(L"Status");
    lvc.cx = 150;
    ListView_InsertColumn(m_hCLIListView, 1, &lvc);
}

// -------------------------------------------------------------------------
// PopulateListView: Populate the File Tracking ListView with files
// -------------------------------------------------------------------------
void MainWindow::PopulateListView() {
    ListView_DeleteAllItems(m_hListViewFileTracking);
    int index = 0;
    try {
        for (const auto &entry : fs::directory_iterator(PROJECT_FOLDER)) {
            if (entry.is_regular_file()) {
                std::wstring fileName = entry.path().filename().wstring();
                LVITEM item = {0};
                item.mask = LVIF_TEXT;
                item.iItem = index;
                item.iSubItem = 0;
                item.pszText = const_cast<LPWSTR>(fileName.c_str());
                ListView_InsertItem(m_hListViewFileTracking, &item);

                LVITEM statusItem = {0};
                statusItem.mask = LVIF_TEXT;
                statusItem.iItem = index;
                statusItem.iSubItem = 1;
                statusItem.pszText = const_cast<LPWSTR>(L"Not launched");
                ListView_SetItem(m_hListViewFileTracking, &statusItem);

                ListView_SetCheckState(m_hListViewFileTracking, index, FALSE);
                ++index;
            }
        }
    }
    catch (const std::exception &e) {
        MessageBox(m_hwnd, L"Failed to enumerate project folder. Please check the PROJECT_FOLDER path.",
                   L"Error", MB_OK | MB_ICONERROR);
    }
}

// -------------------------------------------------------------------------
// PopulateCLIListView: Populate the CLI ListView with files from the project folder
// -------------------------------------------------------------------------
void MainWindow::PopulateCLIListView() {
    ListView_DeleteAllItems(m_hCLIListView);
    int index = 0;
    try {
        for (const auto &entry : fs::directory_iterator(PROJECT_FOLDER)) {
            if (entry.is_regular_file()) {
                std::wstring fileName = entry.path().filename().wstring();
                LVITEM item = {0};
                item.mask = LVIF_TEXT;
                item.iItem = index;
                item.iSubItem = 0;
                item.pszText = const_cast<LPWSTR>(fileName.c_str());
                ListView_InsertItem(m_hCLIListView, &item);

                LVITEM statusItem = {0};
                statusItem.mask = LVIF_TEXT;
                statusItem.iItem = index;
                statusItem.iSubItem = 1;
                statusItem.pszText = const_cast<LPWSTR>(L"Not launched");
                ListView_SetItem(m_hCLIListView, &statusItem);

                ListView_SetCheckState(m_hCLIListView, index, FALSE);
                ++index;
            }
        }
    }
    catch (const std::exception &e) {
        MessageBox(m_hwnd, L"Failed to enumerate project folder for CLI. Please check the PROJECT_FOLDER path.",
                   L"Error", MB_OK | MB_ICONERROR);
    }
}

// -------------------------------------------------------------------------
// MainWindow Implementation
// -------------------------------------------------------------------------
MainWindow::MainWindow()
    : m_hwnd(nullptr)
    , m_hTabControl(nullptr)
    , m_hPanelFileTracking(nullptr)
    , m_hPanelWindowMonitoring(nullptr)
    , m_hPanelCLI(nullptr)
    , m_hListViewFileTracking(nullptr)
    , m_hListViewWindowMonitoring(nullptr)
    , m_hCLIEdit(nullptr)
    , m_hCLIListView(nullptr)
    , m_browserPanel(nullptr) // for integrated WebView2
{
    m_prevWindowList.clear();
}

MainWindow::~MainWindow() {
    // Cleanup if necessary
    if (m_browserPanel) {
        m_browserPanel->Destroy();
        delete m_browserPanel;
        m_browserPanel = nullptr;
    }
}

BOOL MainWindow::Create(LPCWSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle,
                        int x, int y, int nWidth, int nHeight, HWND hWndParent) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"MainWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClass(&wc);
    m_hwnd = CreateWindowEx(dwExStyle, wc.lpszClassName, lpWindowName, dwStyle,
                            x, y, nWidth, nHeight, hWndParent, nullptr,
                            GetModuleHandle(nullptr), this);
    return (m_hwnd ? TRUE : FALSE);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (pThis) {
        if (uMsg == WM_CONTEXTMENU) {
            POINT pt;
            if (lParam == -1)
                GetCursorPos(&pt);
            else {
                pt.x = LOWORD(lParam);
                pt.y = HIWORD(lParam);
            }
            RECT rc;
            GetWindowRect(pThis->m_hListViewFileTracking, &rc);
            if (PtInRect(&rc, pt)) {
                POINT ptClient = pt;
                ScreenToClient(pThis->m_hListViewFileTracking, &ptClient);
                LVHITTESTINFO hitTest = {0};
                hitTest.pt = ptClient;
                int iItem = ListView_HitTest(pThis->m_hListViewFileTracking, &hitTest);
                if (iItem != -1) {
                    ListView_SetItemState(pThis->m_hListViewFileTracking, iItem,
                        LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
                    HMENU hPopup = CreatePopupMenu();
                    AppendMenu(hPopup, MF_STRING, ID_CLOSE_WINDOW, L"Close Window");
                    TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, pThis->m_hwnd, nullptr);
                    DestroyMenu(hPopup);
                }
            }
            return 0;
        }
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        OnCreate();
        SetTimer(m_hwnd, TIMER_ID, TIMER_INTERVAL, nullptr);
        LoadTrackingMapping(TRACKING_FILE, m_fileWindowMap, m_hwnd);
        return 0;

    case WM_SIZE: {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            SetWindowPos(m_hTabControl, NULL, 0, 0, rc.right, TAB_CONTROL_HEIGHT, SWP_NOZORDER);
            if (m_browserPanel) {
                // Split view layout
                int leftPaneWidth = rc.right / 3;
                int panelY = TAB_CONTROL_HEIGHT;
                int panelHeight = rc.bottom - panelY;
                // Position file tracking panel on left
                SetWindowPos(m_hPanelFileTracking, NULL, 0, panelY, leftPaneWidth, panelHeight, SWP_NOZORDER);
                SetWindowPos(m_hListViewFileTracking, NULL, 0, 0, leftPaneWidth, panelHeight, SWP_NOZORDER);
                // Position browser panel on right side
                RECT browserRect = { leftPaneWidth, panelY, rc.right, rc.bottom };
                m_browserPanel->SetBounds(browserRect);
            } else {
                // Normal layout
                int panelY = TAB_CONTROL_HEIGHT;
                int launcherAreaHeight = BUTTON_HEIGHT + 10;
                int panelHeight = rc.bottom - panelY - launcherAreaHeight;
                if (m_hPanelFileTracking) {
                    SetWindowPos(m_hPanelFileTracking, NULL, 0, panelY, rc.right, panelHeight, SWP_NOZORDER);
                    SetWindowPos(m_hListViewFileTracking, NULL, 0, 0, rc.right, panelHeight, SWP_NOZORDER);
                }
                if (m_hPanelWindowMonitoring) {
                    SetWindowPos(m_hPanelWindowMonitoring, NULL, 0, panelY, rc.right, panelHeight, SWP_NOZORDER);
                    SetWindowPos(m_hListViewWindowMonitoring, NULL, 0, 0, rc.right, panelHeight, SWP_NOZORDER);
                }
                if (m_hPanelCLI) {
                    SetWindowPos(m_hPanelCLI, NULL, 0, panelY, rc.right, panelHeight, SWP_NOZORDER);
                    int cliEditHeight = 25;
                    SetWindowPos(m_hCLIEdit, NULL, 0, 0, rc.right, cliEditHeight, SWP_NOZORDER);
                    SetWindowPos(m_hCLIListView, NULL, 0, cliEditHeight, rc.right, panelHeight - cliEditHeight, SWP_NOZORDER);
                }
            }
        }
        break;

    case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId >= ID_LAUNCHER_BUTTON_BASE && wmId < (ID_LAUNCHER_BUTTON_BASE + MAX_LAUNCHER_BUTTONS)) {
                auto itButton = m_launcherButtonMap.find(wmId);
                if (itButton != m_launcherButtonMap.end()) {
                    std::wstring filePath = itButton->second;
                    // If .url, read and show integrated browser
                    std::wstring lowerPath = filePath;
                    for (auto &c : lowerPath) c = towlower(c);
                    if (lowerPath.size() >= 4 && lowerPath.rfind(L".url") == lowerPath.size() - 4) {
                        std::wifstream ifs(filePath.c_str());
                        if (!ifs) {
                            MessageBox(m_hwnd, L"Failed to open .url file.", L"Error", MB_OK | MB_ICONERROR);
                            return 0;
                        }
                        std::wstring line, url;
                        while (std::getline(ifs, line)) {
                            if (line.find(L"URL=") == 0) {
                                url = line.substr(4);
                                break;
                            }
                        }
                        ifs.close();
                        if (!url.empty()) {
                            ShowIntegratedBrowser(url);
                            return 0;
                        }
                        // fallback to normal logic
                    }
                    // Normal logic
                    auto it = m_fileWindowMap.find(filePath);
                    if (it != m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                        TrackedWindow currentFp = GetFingerprint(it->second.hwnd);
                        if (!CompareStableAttributes(it->second, currentFp))
                            it->second = currentFp;
                        ActivateTrackedWindow(it->second);
                    } else {
                        std::wstring fileName = filePath.substr(filePath.find_last_of(L'\\') + 1);
                        HWND hwndFound = GetWindowHandleByFileName(fileName);
                        if (hwndFound)
                            ActivateTrackedWindow(m_fileWindowMap[filePath] = GetFingerprint(hwndFound));
                        else
                            LaunchFile(filePath, m_hwnd, m_fileWindowMap);
                    }
                }
            }
            else if (wmId == 1001) {
                // Old log button is hidden.
            }
            else if (wmId == ID_CLOSE_WINDOW) {
                int iSel = ListView_GetNextItem(m_hListViewFileTracking, -1, LVNI_SELECTED);
                if (iSel != -1) {
                    wchar_t buffer[256] = {0};
                    LVITEM item = {0};
                    item.mask = LVIF_TEXT;
                    item.iItem = iSel;
                    item.iSubItem = 0;
                    item.pszText = buffer;
                    item.cchTextMax = 256;
                    ListView_GetItem(m_hListViewFileTracking, &item);
                    std::wstring fileName(buffer);
                    std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                    auto it = m_fileWindowMap.find(filePath);
                    if (it != m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                        PostMessage(it->second.hwnd, WM_CLOSE, 0, 0);
                    }
                }
            }
        }
        break;

    case WM_NOTIFY: {
            LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->hwndFrom == m_hTabControl) {
                if (pnmh->code == TCN_SELCHANGE) {
                    int sel = TabCtrl_GetCurSel(m_hTabControl);
                    SwitchPanel(sel);
                }
            }
            else if (pnmh->hwndFrom == m_hListViewFileTracking) {
                if (pnmh->code == LVN_ITEMCHANGED) {
                    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(lParam);
                    if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_STATEIMAGEMASK)) {
                        int iItem = pnmv->iItem;
                        if (iItem >= 0) {
                            BOOL checked = ListView_GetCheckState(m_hListViewFileTracking, iItem);
                            wchar_t buffer[256] = {0};
                            LVITEM lvItem = {0};
                            lvItem.mask = LVIF_TEXT;
                            lvItem.iItem = iItem;
                            lvItem.iSubItem = 0;
                            lvItem.pszText = buffer;
                            lvItem.cchTextMax = 256;
                            ListView_GetItem(m_hListViewFileTracking, &lvItem);
                            std::wstring fileName(buffer);
                            std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                            m_launcherMap[filePath] = (checked != 0);
                            RefreshLauncherButtons();
                        }
                    }
                }
                else if (pnmh->code == NM_DBLCLK) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(m_hListViewFileTracking, &pt);
                    LVHITTESTINFO ht = {0};
                    ht.pt = pt;
                    int rowIndex = ListView_HitTest(m_hListViewFileTracking, &ht);
                    if (rowIndex != -1) {
                        wchar_t buffer[256] = {0};
                        LVITEM lvItem = {0};
                        lvItem.mask = LVIF_TEXT;
                        lvItem.iItem = rowIndex;
                        lvItem.iSubItem = 0;
                        lvItem.pszText = buffer;
                        lvItem.cchTextMax = 256;
                        ListView_GetItem(m_hListViewFileTracking, &lvItem);
                        std::wstring fileName(buffer);
                        if (!fileName.empty()) {
                            size_t start = fileName.find_first_not_of(L" \t");
                            size_t end   = fileName.find_last_not_of(L" \t");
                            if (start != std::wstring::npos && end != std::wstring::npos)
                                fileName = fileName.substr(start, end - start + 1);
                            std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                            // Check if .url
                            std::wstring lowerPath = filePath;
                            for (auto &c : lowerPath) c = towlower(c);
                            if (lowerPath.size() >= 4 && lowerPath.rfind(L".url") == lowerPath.size() - 4) {
                                // Read URL= and show integrated browser
                                std::wifstream ifs(filePath.c_str());
                                if (!ifs) {
                                    MessageBox(m_hwnd, L"Failed to open .url file.", L"Error", MB_OK | MB_ICONERROR);
                                    return 0;
                                }
                                std::wstring line, url;
                                while (std::getline(ifs, line)) {
                                    if (line.find(L"URL=") == 0) {
                                        url = line.substr(4);
                                        break;
                                    }
                                }
                                ifs.close();
                                if (!url.empty()) {
                                    ShowIntegratedBrowser(url);
                                    return 0;
                                }
                                // fallback to normal logic
                            }
                            // Normal logic
                            auto it = m_fileWindowMap.find(filePath);
                            if (it != m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                                TrackedWindow currentFp = GetFingerprint(it->second.hwnd);
                                if (!CompareStableAttributes(it->second, currentFp))
                                    it->second = currentFp;
                                ActivateTrackedWindow(it->second);
                            } else {
                                HWND hwndFound = GetWindowHandleByFileName(fileName);
                                if (hwndFound)
                                    ActivateTrackedWindow(m_fileWindowMap[filePath] = GetFingerprint(hwndFound));
                                else
                                    LaunchFile(filePath, m_hwnd, m_fileWindowMap);
                            }
                        }
                    }
                }
            }
            else if (pnmh->hwndFrom == m_hCLIListView) {
                if (pnmh->code == LVN_ITEMCHANGED) {
                    LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(lParam);
                    if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_STATEIMAGEMASK)) {
                        int iItem = pnmv->iItem;
                        if (iItem >= 0) {
                            BOOL checked = ListView_GetCheckState(m_hCLIListView, iItem);
                            wchar_t buffer[256] = {0};
                            LVITEM lvItem = {0};
                            lvItem.mask = LVIF_TEXT;
                            lvItem.iItem = iItem;
                            lvItem.iSubItem = 0;
                            lvItem.pszText = buffer;
                            lvItem.cchTextMax = 256;
                            ListView_GetItem(m_hCLIListView, &lvItem);
                            std::wstring fileName(buffer);
                            std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                            m_launcherMap[filePath] = (checked != 0);
                            RefreshLauncherButtons();
                        }
                    }
                }
                else if (pnmh->code == NM_DBLCLK) {
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(m_hCLIListView, &pt);
                    LVHITTESTINFO ht = {0};
                    ht.pt = pt;
                    int rowIndex = ListView_HitTest(m_hCLIListView, &ht);
                    if (rowIndex != -1) {
                        wchar_t buffer[256] = {0};
                        LVITEM lvItem = {0};
                        lvItem.mask = LVIF_TEXT;
                        lvItem.iItem = rowIndex;
                        lvItem.iSubItem = 0;
                        lvItem.pszText = buffer;
                        lvItem.cchTextMax = 256;
                        ListView_GetItem(m_hCLIListView, &lvItem);
                        std::wstring fileName(buffer);
                        if (!fileName.empty()) {
                            size_t start = fileName.find_first_not_of(L" \t");
                            size_t end   = fileName.find_last_not_of(L" \t");
                            if (start != std::wstring::npos && end != std::wstring::npos)
                                fileName = fileName.substr(start, end - start + 1);
                            std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                            // Check if .url
                            std::wstring lowerPath = filePath;
                            for (auto &c : lowerPath) c = towlower(c);
                            if (lowerPath.size() >= 4 && lowerPath.rfind(L".url") == lowerPath.size() - 4) {
                                std::wifstream ifs(filePath.c_str());
                                if (!ifs) {
                                    MessageBox(m_hwnd, L"Failed to open .url file.", L"Error", MB_OK | MB_ICONERROR);
                                    return 0;
                                }
                                std::wstring line, url;
                                while (std::getline(ifs, line)) {
                                    if (line.find(L"URL=") == 0) {
                                        url = line.substr(4);
                                        break;
                                    }
                                }
                                ifs.close();
                                if (!url.empty()) {
                                    ShowIntegratedBrowser(url);
                                    return 0;
                                }
                                // fallback
                            }
                            // Normal logic
                            auto it = m_fileWindowMap.find(filePath);
                            if (it != m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                                TrackedWindow currentFp = GetFingerprint(it->second.hwnd);
                                if (!CompareStableAttributes(it->second, currentFp))
                                    it->second = currentFp;
                                ActivateTrackedWindow(it->second);
                            } else {
                                HWND hwndFound = GetWindowHandleByFileName(fileName);
                                if (hwndFound)
                                    ActivateTrackedWindow(m_fileWindowMap[filePath] = GetFingerprint(hwndFound));
                                else
                                    LaunchFile(filePath, m_hwnd, m_fileWindowMap);
                            }
                        }
                    }
                }
            }
        }
        break;

    case WM_TIMER:
        {
            // Update File Tracking ListView statuses
            int count = ListView_GetItemCount(m_hListViewFileTracking);
            for (int i = 0; i < count; ++i) {
                wchar_t buffer[256] = {0};
                LVITEM item = {0};
                item.mask = LVIF_TEXT;
                item.iItem = i;
                item.iSubItem = 0;
                item.pszText = buffer;
                item.cchTextMax = 256;
                ListView_GetItem(m_hListViewFileTracking, &item);
                std::wstring fileName(buffer);
                std::wstring status;
                std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                HWND hwndFound = nullptr;
                auto it = m_fileWindowMap.find(filePath);
                if (it != m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                    TrackedWindow currentFp = GetFingerprint(it->second.hwnd);
                    if (CompareStableAttributes(it->second, currentFp))
                        hwndFound = it->second.hwnd;
                    else {
                        it->second = currentFp;
                        hwndFound = currentFp.hwnd;
                    }
                } else {
                    hwndFound = GetWindowHandleByFileName(fileName);
                }
                status = (hwndFound) ? GetStateString(hwndFound) : L"Not launched";
                LVITEM statusItem = {0};
                statusItem.mask = LVIF_TEXT;
                statusItem.iItem = i;
                statusItem.iSubItem = 1;
                statusItem.pszText = const_cast<LPWSTR>(status.c_str());
                ListView_SetItem(m_hListViewFileTracking, &statusItem);
            }

            // Update CLI ListView statuses
            int cliCount = ListView_GetItemCount(m_hCLIListView);
            for (int i = 0; i < cliCount; ++i) {
                wchar_t buffer[256] = {0};
                LVITEM item = {0};
                item.mask = LVIF_TEXT;
                item.iItem = i;
                item.iSubItem = 0;
                item.pszText = buffer;
                item.cchTextMax = 256;
                ListView_GetItem(m_hCLIListView, &item);
                std::wstring fileName(buffer);
                std::wstring status;
                std::wstring filePath = PROJECT_FOLDER + L"\\" + fileName;
                HWND hwndFound = nullptr;
                auto it = m_fileWindowMap.find(filePath);
                if (it != m_fileWindowMap.end() && IsWindow(it->second.hwnd)) {
                    TrackedWindow currentFp = GetFingerprint(it->second.hwnd);
                    if (CompareStableAttributes(it->second, currentFp))
                        hwndFound = it->second.hwnd;
                    else {
                        it->second = currentFp;
                        hwndFound = currentFp.hwnd;
                    }
                } else {
                    hwndFound = GetWindowHandleByFileName(fileName);
                }
                status = (hwndFound) ? GetStateString(hwndFound) : L"Not launched";
                LVITEM statusItem = {0};
                statusItem.mask = LVIF_TEXT;
                statusItem.iItem = i;
                statusItem.iSubItem = 1;
                statusItem.pszText = const_cast<LPWSTR>(status.c_str());
                ListView_SetItem(m_hCLIListView, &statusItem);
            }

            // Update Window Monitoring ListView
            WindowMonitor monitor;
            std::vector<WindowInfo> windows = monitor.EnumerateWindows();
            if (!WindowListsEqual(windows, m_prevWindowList))
                m_prevWindowList = windows;
            ListView_SetRedraw(m_hListViewWindowMonitoring, FALSE);
            int currentCount = ListView_GetItemCount(m_hListViewWindowMonitoring);
            int newCount = static_cast<int>(windows.size());
            for (int i = 0; i < newCount; ++i) {
                const auto &win = windows[i];
                std::wstring title = win.title;
                std::wstring processName = win.processName;
                std::wstring stateStr;
                switch (win.state) {
                    case WindowState::Normal:     stateStr = L"Normal";     break;
                    case WindowState::Minimized:  stateStr = L"Minimized";  break;
                    case WindowState::Maximized:  stateStr = L"Maximized";  break;
                    default:                      stateStr = L"Unknown";    break;
                }
                if (win.hwnd == GetForegroundWindow())
                    stateStr += L" (Focused)";
                std::wstring hwndStr = std::to_wstring(reinterpret_cast<uintptr_t>(win.hwnd));
                std::wstring className = win.className;
                std::wstring leftStr = std::to_wstring(win.rect.left);
                std::wstring topStr = std::to_wstring(win.rect.top);
                std::wstring rightStr = std::to_wstring(win.rect.right);
                std::wstring bottomStr = std::to_wstring(win.rect.bottom);
                std::wstring associatedFiles;
                int topCount = ListView_GetItemCount(m_hListViewFileTracking);
                std::wstring winTitleLower = title;
                for (auto &c : winTitleLower)
                    c = towlower(c);
                for (int j = 0; j < topCount; ++j) {
                    wchar_t fileBuf[256] = {0};
                    LVITEM fileItem = {0};
                    fileItem.mask = LVIF_TEXT;
                    fileItem.iItem = j;
                    fileItem.iSubItem = 0;
                    fileItem.pszText = fileBuf;
                    fileItem.cchTextMax = 256;
                    ListView_GetItem(m_hListViewFileTracking, &fileItem);
                    std::wstring fileName(fileBuf);
                    std::wstring fileNameLower = fileName;
                    for (auto &c : fileNameLower)
                        c = towlower(c);
                    fileNameLower = StripLnkExtension(fileNameLower);
                    if (!fileNameLower.empty() && winTitleLower.find(fileNameLower) != std::wstring::npos) {
                        if (!associatedFiles.empty())
                            associatedFiles += L", ";
                        associatedFiles += fileName;
                    }
                }
                if (i < currentCount) {
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 0, const_cast<LPWSTR>(associatedFiles.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 1, const_cast<LPWSTR>(title.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 2, const_cast<LPWSTR>(processName.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 3, const_cast<LPWSTR>(stateStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 4, const_cast<LPWSTR>(hwndStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 5, const_cast<LPWSTR>(className.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 6, const_cast<LPWSTR>(leftStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 7, const_cast<LPWSTR>(topStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 8, const_cast<LPWSTR>(rightStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 9, const_cast<LPWSTR>(bottomStr.c_str()));
                } else {
                    LVITEM lvi = {0};
                    lvi.mask = LVIF_TEXT;
                    lvi.iItem = i;
                    lvi.iSubItem = 0;
                    lvi.pszText = const_cast<LPWSTR>(associatedFiles.c_str());
                    ListView_InsertItem(m_hListViewWindowMonitoring, &lvi);
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 1, const_cast<LPWSTR>(title.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 2, const_cast<LPWSTR>(processName.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 3, const_cast<LPWSTR>(stateStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 4, const_cast<LPWSTR>(hwndStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 5, const_cast<LPWSTR>(className.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 6, const_cast<LPWSTR>(leftStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 7, const_cast<LPWSTR>(topStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 8, const_cast<LPWSTR>(rightStr.c_str()));
                    ListView_SetItemText(m_hListViewWindowMonitoring, i, 9, const_cast<LPWSTR>(bottomStr.c_str()));
                }
            }
            while (ListView_GetItemCount(m_hListViewWindowMonitoring) > newCount)
                ListView_DeleteItem(m_hListViewWindowMonitoring, newCount);
            ListView_SetRedraw(m_hListViewWindowMonitoring, TRUE);
            InvalidateRect(m_hListViewWindowMonitoring, nullptr, TRUE);
            UpdateWindow(m_hListViewWindowMonitoring);
        }
        return 0;

    case WM_DESTROY:
        SaveTrackingMapping(TRACKING_FILE, m_fileWindowMap);
        KillTimer(m_hwnd, TIMER_ID);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void MainWindow::OnCreate() {
    // Create the tab control
    CreateTabControl();
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int panelY = TAB_CONTROL_HEIGHT;
    int launcherAreaHeight = BUTTON_HEIGHT + 10;
    int panelHeight = rc.bottom - panelY - launcherAreaHeight;
    // Create container panels
    m_hPanelFileTracking = CreateWindowEx(0, L"STATIC", L"FileTrackingPanel", WS_CHILD | WS_VISIBLE,
                                           0, panelY, rc.right, panelHeight,
                                           m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SetWindowSubclass(m_hPanelFileTracking, PanelSubclassProc, 1, 0);
    m_hPanelWindowMonitoring = CreateWindowEx(0, L"STATIC", L"WindowMonitoringPanel", WS_CHILD,
                                               0, panelY, rc.right, panelHeight,
                                               m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_hPanelCLI = CreateWindowEx(0, L"STATIC", L"CLI Panel", WS_CHILD,
                                  0, panelY, rc.right, panelHeight,
                                  m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    // Subclass the CLI panel
    SetWindowSubclass(m_hPanelCLI, PanelSubclassProc, 1, 0);
    SwitchPanel(0);

    // Create embedded controls
    m_hListViewFileTracking = CreateWindowEx(0, WC_LISTVIEW, L"",
                    WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                    0, 0, rc.right, panelHeight,
                    m_hPanelFileTracking, nullptr, GetModuleHandle(nullptr), nullptr);
    ListView_SetExtendedListViewStyleEx(m_hListViewFileTracking, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);

    m_hListViewWindowMonitoring = CreateWindowEx(0, WC_LISTVIEW, L"",
                    WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                    0, 0, rc.right, panelHeight,
                    m_hPanelWindowMonitoring, nullptr, GetModuleHandle(nullptr), nullptr);

    m_hCLIEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_MULTILINE | ES_WANTRETURN,
                    0, 0, rc.right, 25,
                    m_hPanelCLI, nullptr, GetModuleHandle(nullptr), nullptr);

    m_hCLIListView = CreateWindowEx(0, WC_LISTVIEW, L"",
                    WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                    0, 25, rc.right, panelHeight - 25,
                    m_hPanelCLI, nullptr, GetModuleHandle(nullptr), nullptr);
    // Enable checkboxes
    ListView_SetExtendedListViewStyleEx(m_hCLIListView, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);

    InitListViewControls();
    PopulateListView();
    PopulateCLIListView();

    // Subclass the CLI edit
    SetWindowSubclass(m_hCLIEdit, MainWindow::CLIEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
}

// ---------------------------- End of file: MainWindow.cpp (Version: 1.61.0) ----------------------------
