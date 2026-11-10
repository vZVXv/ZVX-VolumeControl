#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <unordered_map>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SETTINGS 1001
#define ID_TRAY_RESTART  1002
#define ID_TRAY_EXIT     1003

wchar_t g_iniPath[MAX_PATH];
wchar_t g_hotkeyName[64] = L"AppsKey";
wchar_t g_currentLang[8] = L"ru";
UINT g_vkCode = VK_APPS;

bool g_isAdjusting = false;
bool g_firstMouseMotion = false;
POINT g_lastMousePos = { 0, 0 };
float g_accumulatedDelta = 0.0f;

HWND g_hwndMain = NULL;
HWND g_hwndSettings = NULL;
NOTIFYICONDATAW g_nid = {};

HHOOK g_hMouseHook = NULL;
HHOOK g_hKeyHook = NULL;

struct Strings {
    std::wstring traySettings;
    std::wstring trayRestart;
    std::wstring trayExit;
    std::wstring settingsTitle;
    std::wstring tabGeneral;
    std::wstring tabAbout;
    std::wstring lblKey;
    std::wstring lblHint;
    std::wstring lblLang;
    std::wstring btnSave;
    std::wstring errKey;
    std::wstring errKeyTitle;
    std::wstring successMsg;
    std::wstring successTitle;
    std::wstring aboutText;
};

std::unordered_map<std::wstring, Strings> g_LangMap = {
    { L"ru",{
        L"\x041d\x0430\x0441\x0442\x0440\x043e\x0439\x043a\x0438",
        L"\x041f\x0435\x0440\x0435\x0437\x0430\x043f\x0443\x0441\x0442\x0438\x0442\x044c \x0441\x043a\x0440\x0438\x0444\x0442",
        L"\x0412\x044b\x0445\x043e\x0434",
        L"\x041d\x0430\x0441\x0442\x0440\x043e\x0439\x043a\x0438 ZVX Volume Control",
        L"\x041e\x0441\x043d\x043e\x0432\x043d\x044b\x0435",
        L"\x041e\x0431 \x0430\x0432\x0442\x043e\x0440\x0435",
        L"\x0413\x043e\x0440\x044f\x0447\x0430\x044f \x043a\x043b\x0430\x0432\x0438\x0448\x0430 \x0434\x043b\x044f \x0440\x0435\x0433\x0443\x043b\x0438\x0440\x043e\x0432\x043a\x0438 \x0433\x0440\x043e\x043c\x043a\x043e\x0441\x0442\x0438:",
        L"\x041f\x043e\x0434\x0441\x043a\x0430\x0437\x043a\x0430: AppsKey, F1, F2, F3, RWin, LWin, CapsLock.",
        L"\x042f\x0437\x044b\x043a \x0438\x043d\x0442\x0435\x0440\x0444\x0435\x0439\x0441\x0430 / Language:",
        L"\x0421\x043e\x0445\x0440\x0430\x043d\x0438\x0442\x044c",
        L"\x041d\x0435 \x0443\x0434\x0430\x043b\x043e\x0441\x044c \x043d\x0430\x0437\x043d\x0430\x0447\x0438\x0442\x044c \x043a\x043b\x0430\x0432\x0438\x0448\x0443: ",
        L"\x041e\x0448\x0438\x0431\x043a\x0430",
        L"\x0413\x043e\x0440\x044f\x0447\x0430\x044f \x043a\x043b\x0430\x0432\x0438\x0448\x0430 \x0443\x0441\x043f\x0435\x0445\x043d\x043e \x0438\x0437\x043c\x0435\x043d\x0435\x043d\x0430 \x043d\x0430: ",
        L"\x0413\x043e\x0442\x043e\x0432\x043e",
        L"ZVX Volume Control (C++ Edition)\n\n\x0421\x043e\x0437\x0434\x0430\x0442\x0435\x043b\x044c: \x0415\x0432\x0433\x0435\x043d\x0438\x0439 \x0413\x0443\x0431\x0430\x0440\x0435\x0432 (ZVX)\n\x0412\x0435\x0440\x0441\x0438\x044f: 3.2"
    } },
    { L"en",{
        L"Settings", L"Restart Script", L"Exit",
        L"ZVX Volume Control Settings", L"General", L"About",
        L"Volume Control Hotkey:",
        L"Tip: AppsKey, F1, F2, F3, RWin, LWin, CapsLock.",
        L"Interface Language:", L"Save",
        L"Failed to assign hotkey: ", L"Error",
        L"Hotkey successfully changed to: ", L"Success",
        L"ZVX Volume Control (C++ Edition)\n\nCreator: Evgeny Gubarev (ZVX)\nVersion: 3.2"
    } }
};

Strings& GetL() {
    if (g_LangMap.find(g_currentLang) != g_LangMap.end())
        return g_LangMap[g_currentLang];
    return g_LangMap[L"ru"];
}

UINT ParseVK(const wchar_t* keyStr) {
    std::wstring s = keyStr;
    if (s == L"AppsKey" || s == L"apps") return VK_APPS;
    if (s == L"RWin") return VK_RWIN;
    if (s == L"LWin") return VK_LWIN;
    if (s == L"F1") return VK_F1;
    if (s == L"F2") return VK_F2;
    if (s == L"F3") return VK_F3;
    if (s == L"CapsLock") return VK_CAPITAL;
    return VK_APPS;
}

// Симуляция нажатия мультимедийной клавиши громкости для вызова окна Windows 11
void SimulateVolumeKey(WORD vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_isAdjusting && wParam == WM_MOUSEMOVE) {
        MSLLHOOKSTRUCT* pMouse = (MSLLHOOKSTRUCT*)lParam;

        if (g_firstMouseMotion) {
            g_lastMousePos = pMouse->pt;
            g_firstMouseMotion = false;
            g_accumulatedDelta = 0.0f;
            return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
        }

        int deltaX = pMouse->pt.x - g_lastMousePos.x;
        if (deltaX != 0) {
            g_accumulatedDelta += deltaX;
            const float threshold = 8.0f; // Шаг чувствительности перемещения мыши

            while (g_accumulatedDelta >= threshold) {
                SimulateVolumeKey(VK_VOLUME_UP);
                g_accumulatedDelta -= threshold;
            }
            while (g_accumulatedDelta <= -threshold) {
                SimulateVolumeKey(VK_VOLUME_DOWN);
                g_accumulatedDelta += threshold;
            }

            g_lastMousePos = pMouse->pt;
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        if (pKey->vkCode == g_vkCode) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                if (!g_isAdjusting) {
                    g_isAdjusting = true;
                    g_firstMouseMotion = true;
                    GetCursorPos(&g_lastMousePos);
                    g_accumulatedDelta = 0.0f;

                    g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
                }
                return 1;
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                if (g_isAdjusting) {
                    g_isAdjusting = false;
                    g_firstMouseMotion = false;
                    if (g_hMouseHook) {
                        UnhookWindowsHookEx(g_hMouseHook);
                        g_hMouseHook = NULL;
                    }
                }
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hKeyHook, nCode, wParam, lParam);
}

void AddTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"ZVX Volume Control");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    Strings& L = GetL();
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L.traySettings.c_str());
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTART, L.trayRestart.c_str());
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L.trayExit.c_str());

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void ShowSettingsDialog(HWND hParent);

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_SETTINGS:
            ShowSettingsDialog(hwnd);
            break;
        case ID_TRAY_RESTART: {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            ShellExecuteW(NULL, NULL, szPath, NULL, NULL, SW_SHOWNORMAL);
            PostQuitMessage(0);
            break;
        }
        case ID_TRAY_EXIT:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hComboKey = NULL, hComboLang = NULL, hTab = NULL;
    switch (uMsg) {
    case WM_CREATE: {
        Strings& L = GetL();
        hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 10, 345, 180, hwnd, (HMENU)200, GetModuleHandle(NULL), NULL);

        TCITEMW tie = { 0 };
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L.tabGeneral.c_str();
        TabCtrl_InsertItem(hTab, 0, &tie);
        tie.pszText = (LPWSTR)L.tabAbout.c_str();
        TabCtrl_InsertItem(hTab, 1, &tie);

        CreateWindowW(L"STATIC", L.lblKey.c_str(), WS_CHILD | WS_VISIBLE, 25, 45, 300, 20, hwnd, (HMENU)201, NULL, NULL);
        hComboKey = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            25, 68, 300, 120, hwnd, (HMENU)202, NULL, NULL);

        const wchar_t* keys[] = { L"AppsKey", L"RWin", L"LWin", L"F1", L"F2", L"F3", L"CapsLock" };
        for (auto k : keys) SendMessageW(hComboKey, CB_ADDSTRING, 0, (LPARAM)k);
        SendMessageW(hComboKey, CB_SELECTSTRING, -1, (LPARAM)g_hotkeyName);

        CreateWindowW(L"STATIC", L.lblHint.c_str(), WS_CHILD | WS_VISIBLE, 25, 95, 300, 20, hwnd, (HMENU)203, NULL, NULL);
        CreateWindowW(L"STATIC", L.lblLang.c_str(), WS_CHILD | WS_VISIBLE, 25, 125, 140, 20, hwnd, (HMENU)204, NULL, NULL);

        hComboLang = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            170, 122, 155, 120, hwnd, (HMENU)205, NULL, NULL);
        SendMessageW(hComboLang, CB_ADDSTRING, 0, (LPARAM)L"\x0420\x0443\x0441\x0441\x043a\x0438\x0439");
        SendMessageW(hComboLang, CB_ADDSTRING, 0, (LPARAM)L"English");
        SendMessageW(hComboLang, CB_SETCURSEL, wcscmp(g_currentLang, L"en") == 0 ? 1 : 0, 0);

        CreateWindowW(L"STATIC", L.aboutText.c_str(), WS_CHILD, 25, 50, 300, 120, hwnd, (HMENU)206, NULL, NULL);

        CreateWindowW(L"BUTTON", L.btnSave.c_str(), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            240, 200, 115, 30, hwnd, (HMENU)IDOK, NULL, NULL);
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->idFrom == 200 && pnmh->code == TCN_SELCHANGE) {
            int tabIndex = TabCtrl_GetCurSel(hTab);
            BOOL showGen = (tabIndex == 0) ? SW_SHOW : SW_HIDE;
            BOOL showAbout = (tabIndex == 1) ? SW_SHOW : SW_HIDE;

            for (int id = 201; id <= 205; ++id) ShowWindow(GetDlgItem(hwnd, id), showGen);
            ShowWindow(GetDlgItem(hwnd, 206), showAbout);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            wchar_t kBuf[64];
            GetWindowTextW(hComboKey, kBuf, 64);
            int langIdx = (int)SendMessageW(hComboLang, CB_GETCURSEL, 0, 0);
            wcscpy_s(g_hotkeyName, kBuf);
            wcscpy_s(g_currentLang, langIdx == 1 ? L"en" : L"ru");

            WritePrivateProfileStringW(L"Settings", L"Hotkey", g_hotkeyName, g_iniPath);
            WritePrivateProfileStringW(L"Settings", L"Language", g_currentLang, g_iniPath);
            g_vkCode = ParseVK(g_hotkeyName);

            Strings& L = GetL();
            MessageBoxW(hwnd, (L.successMsg + g_hotkeyName).c_str(), L.successTitle.c_str(), MB_OK | MB_ICONINFORMATION);
            DestroyWindow(hwnd);
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        g_hwndSettings = NULL;
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void ShowSettingsDialog(HWND hParent) {
    if (g_hwndSettings && IsWindow(g_hwndSettings)) {
        SetForegroundWindow(g_hwndSettings);
        return;
    }
    Strings& L = GetL();

    WNDCLASSW wc = { 0 };
    if (!GetClassInfoW(GetModuleHandle(NULL), L"ZVXSettingsClass", &wc)) {
        wc.lpfnWndProc = SettingsProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"ZVXSettingsClass";
        RegisterClassW(&wc);
    }

    g_hwndSettings = CreateWindowExW(WS_EX_TOPMOST, L"ZVXSettingsClass", L.settingsTitle.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 280, hParent, NULL, GetModuleHandle(NULL), NULL);

    ShowWindow(g_hwndSettings, SW_SHOW);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitCommonControls();

    GetModuleFileNameW(NULL, g_iniPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(g_iniPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(g_iniPath, L"settings.ini");

    GetPrivateProfileStringW(L"Settings", L"Hotkey", L"AppsKey", g_hotkeyName, 64, g_iniPath);
    GetPrivateProfileStringW(L"Settings", L"Language", L"ru", g_currentLang, 8, g_iniPath);
    g_vkCode = ParseVK(g_hotkeyName);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ZVXMainClass";
    RegisterClassW(&wc);

    // Создаем легкое невидимое окно для обработки системного трея и сообщений
    g_hwndMain = CreateWindowExW(0, L"ZVXMainClass", L"", WS_POPUP,
        0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    AddTrayIcon(g_hwndMain);
    g_hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hKeyHook) UnhookWindowsHookEx(g_hKeyHook);
    if (g_hMouseHook) UnhookWindowsHookEx(g_hMouseHook);

    return (int)msg.wParam;
}