// File: BrowserPanel.cpp
// Version: 1.3 (Removed WIL references, replaced with WRL::ComPtr usage to fix 'wil/com.h' not found)
// -------------------------------------------------------------------------
// This file implements the BrowserPanel class, which hosts an embedded
// WebView2 control without relying on WIL. All references to wil/com.h
// have been removed or replaced by WRL::ComPtr. This way, we do not need
// the Windows Implementation Library (WIL) installed.
//
// Changes in Version 1.3:
// 1) Removed #include <wil/com.h> and replaced wil::com_ptr with WRL::ComPtr.
// 2) Added #include <wrl.h> and #include <wrl/client.h> for ComPtr.
// 3) The rest of the environment/controller creation logic remains the same,
//    except that we use Microsoft::WRL::ComPtr instead of wil.
//
// NOTE: If you previously had code referencing other WIL macros, those
// references are now removed. This file compiles without requiring wil/com.h.
//
// -------------------------------------------------------------------------

#include "BrowserPanel.h"
#include <string>
#include <sstream>
#include <windows.h>
#include <cstdio> // [TEST] Simple line to demonstrate patch
#include <objbase.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlwapi.h>
#include "FingerprintUtils.h"
#include "Config.h"

// For WRL
#include <wrl.h>
#include <wrl/client.h>

// For WebView2
#include <WebView2.h>

// Helper macro for logging to OutputDebugString
static void LogDebug(const wchar_t* msg)
{
    OutputDebugStringW(msg);
    OutputDebugStringW(L"\n");
}

BrowserPanel::BrowserPanel()
    : m_hWnd(nullptr)
    , m_controller(nullptr)
    , m_webView(nullptr)
{
}

BrowserPanel::~BrowserPanel()
{
    Destroy();
}

bool BrowserPanel::Create(HWND hParent, const RECT &rc)
{
    // Create a child window to represent this panel
    m_hWnd = CreateWindowEx(
        0,
        L"STATIC",
        L"BrowserPanel",
        WS_CHILD | WS_VISIBLE,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        hParent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    if (!m_hWnd) {
        MessageBox(hParent, L"Failed to create BrowserPanel window.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    // Store a pointer to this class in GWLP_USERDATA
    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Attempt to create the WebView2 environment
    LogDebug(L"[BrowserPanel] Creating WebView2 environment...");
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, // browserExecutableFolder
        nullptr, // userDataFolder
        nullptr, // options
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hParent, rc](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(result)) {
                    std::wstringstream ss;
                    ss << L"[BrowserPanel] Failed to create WebView2 environment. HRESULT=0x"
                       << std::hex << result;
                    LogDebug(ss.str().c_str());
                    MessageBox(hParent, ss.str().c_str(), L"WebView2 Error", MB_OK | MB_ICONERROR);
                    return result;
                }
                LogDebug(L"[BrowserPanel] WebView2 environment created OK. Creating controller...");

                // Create the WebView2 controller
                env->CreateCoreWebView2Controller(m_hWnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, rc, hParent](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(result) || !controller) {
                                std::wstringstream ss;
                                ss << L"[BrowserPanel] Failed to create WebView2 controller. HRESULT=0x"
                                   << std::hex << result;
                                LogDebug(ss.str().c_str());
                                MessageBox(hParent, ss.str().c_str(), L"WebView2 Error", MB_OK | MB_ICONERROR);
                                return result;
                            }
                            // Store the controller
                            m_controller = controller;

                            // Retrieve the CoreWebView2
                            HRESULT hrLocal = m_controller->get_CoreWebView2(&m_webView);
                            if (FAILED(hrLocal) || !m_webView) {
                                LogDebug(L"[BrowserPanel] get_CoreWebView2 returned null or failed!");
                                MessageBox(hParent, L"get_CoreWebView2 returned null!", L"WebView2 Error", MB_OK | MB_ICONERROR);
                                return E_FAIL;
                            }

                            // Resize the controller to fill this panel
                            RECT bounds = rc;
                            bounds.right -= bounds.left;
                            bounds.bottom -= bounds.top;
                            bounds.left = 0;
                            bounds.top = 0;
                            m_controller->put_Bounds(bounds);

                            LogDebug(L"[BrowserPanel] WebView2 controller created OK.");
                            return S_OK;
                        }
                    ).Get());
                return S_OK;
            }
        ).Get()
    );

    if (FAILED(hr)) {
        std::wstringstream ss;
        ss << L"[BrowserPanel] CreateCoreWebView2EnvironmentWithOptions call failed immediately. HRESULT=0x"
           << std::hex << hr;
        LogDebug(ss.str().c_str());
        MessageBox(hParent, ss.str().c_str(), L"WebView2 Error", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

void BrowserPanel::Destroy()
{
    if (m_controller) {
        m_controller->Close();
        m_controller = nullptr;
    }
    m_webView = nullptr;
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void BrowserPanel::SetBounds(const RECT &rc)
{
    if (m_hWnd && IsWindow(m_hWnd)) {
        SetWindowPos(m_hWnd, nullptr,
            rc.left,
            rc.top,
            rc.right - rc.left,
            rc.bottom - rc.top,
            SWP_NOZORDER
        );
    }
    if (m_controller) {
        RECT local = {0, 0, rc.right - rc.left, rc.bottom - rc.top};
        m_controller->put_Bounds(local);
    }
}

void BrowserPanel::Navigate(const std::wstring &url)
{
    if (!m_webView) {
        LogDebug(L"[BrowserPanel] Navigate called but m_webView is null!");
        MessageBox(nullptr, L"WebView not yet initialized.", L"WebView2 Error", MB_OK | MB_ICONERROR);
        return;
    }
    std::wstringstream ss;
    ss << L"[BrowserPanel] Navigating to: " << url;
    LogDebug(ss.str().c_str());
    m_webView->Navigate(url.c_str());
}

// End of file: BrowserPanel.cpp (Version: 1.3)
