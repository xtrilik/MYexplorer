// File: BrowserPanel.h
// Version: 1.3 (Synchronized with BrowserPanel.cpp to fix mismatched signatures and missing members)
// -------------------------------------------------------------------------
// This header declares the BrowserPanel class, which hosts an embedded
// WebView2 control without using WIL. It matches the BrowserPanel.cpp
// (Version 1.3) that references m_hWnd, m_controller, and m_webView.
//
// Changes in Version 1.3:
//  1) Added non-static bool Create(HWND hParent, const RECT &rc);
//  2) Added Destroy(), SetBounds(), and Navigate() methods.
//  3) Declared HWND m_hWnd, WRL::ComPtr<ICoreWebView2Controller> m_controller,
//     and WRL::ComPtr<ICoreWebView2> m_webView as private members.
//
// This resolves compiler errors like "illegal reference to non-static member"
// and "overloaded member function not found in 'BrowserPanel'." 
// Now BrowserPanel.h and BrowserPanel.cpp are in sync.
//
// -------------------------------------------------------------------------

#ifndef BROWSERPANEL_H
#define BROWSERPANEL_H

#include <windows.h>
#include <string>
#include <wrl.h>            // For Microsoft::WRL::ComPtr
#include <WebView2.h>       // For ICoreWebView2, ICoreWebView2Controller

class BrowserPanel
{
public:
    BrowserPanel();
    ~BrowserPanel();

    // Creates the child window for this panel, and asynchronously sets up WebView2.
    bool Create(HWND hParent, const RECT &rc);

    // Closes the WebView2 controller (if any) and destroys this panel window.
    void Destroy();

    // Adjusts this panel's window and the WebView2 controller bounds.
    void SetBounds(const RECT &rc);

    // Navigates the WebView2 control to the specified URL (once created).
    void Navigate(const std::wstring &url);

private:
    HWND m_hWnd;   // Panel window handle
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;
};

#endif // BROWSERPANEL_H

// ---------------------------- End of file: BrowserPanel.h (Version: 1.3) ----------------------------
