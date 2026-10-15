	#include "resource.h"
	#define UNICODE
	#define _UNICODE
	#include <windows.h>
	#include <string>
	#include <thread>

	#pragma comment(lib, "user32.lib")
	#pragma comment(lib, "gdi32.lib")
	#pragma comment(lib, "shell32.lib")

	#define ID_EDIT_TEXT          1001
	#define ID_TRAY_SHOW          2003
	#define ID_TRAY_EXIT          2002
	#define ID_EDIT_SET_HOTKEY    2004
	#define ID_VIEW_SHOW_HOTKEY   2005
	#define IDT_HIDE_STATUS       100
	#define WM_APP_TRAY           (WM_APP + 1)
	#define WM_APP_DO_TYPE        (WM_APP + 2)
	#define HOTKEY_ID             1

	static HINSTANCE g_hInst   = NULL;
	static HWND      g_hWnd    = NULL;
	static HWND      g_hEdit   = NULL;
	static HWND      g_hStatus = NULL;
	static NOTIFYICONDATA g_nid = {};
	static UINT g_mod = MOD_CONTROL | MOD_ALT;
	static UINT g_key = VK_F8;
	static bool g_bCapturing = false;

	// ---------- 热键描述 ----------
	static std::wstring VkName(UINT vk) {
		switch (vk) {
		case VK_F1:  return L"F1";  case VK_F2:  return L"F2";
		case VK_F3:  return L"F3";  case VK_F4:  return L"F4";
		case VK_F5:  return L"F5";  case VK_F6:  return L"F6";
		case VK_F7:  return L"F7";  case VK_F8:  return L"F8";
		case VK_F9:  return L"F9";  case VK_F10: return L"F10";
		case VK_F11: return L"F11"; case VK_F12: return L"F12";
		case VK_SPACE:  return L"Space";
		case VK_RETURN: return L"Enter";
		case VK_TAB:    return L"Tab";
		case VK_DELETE: return L"Delete";
		default:
			if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
				return std::wstring(1, (wchar_t)vk);
			wchar_t b[8] = {};
			wsprintfW(b, L"0x%02X", vk);
			return b;
		}
	}

	static std::wstring HotkeyLabel() {
		std::wstring s;
		if (g_mod & MOD_CONTROL) s += L"Ctrl+";
		if (g_mod & MOD_ALT)     s += L"Alt+";
		if (g_mod & MOD_SHIFT)   s += L"Shift+";
		if (g_mod & MOD_WIN)     s += L"Win+";
		s += VkName(g_key);
		return s;
	}

	static void UpdateStatusText(const std::wstring& txt) {
		SetWindowTextW(g_hStatus, txt.c_str());
	}

	static void ShowStatusMomentarily(const std::wstring& txt) {
		ShowWindow(g_hStatus, SW_SHOW);
		UpdateStatusText(txt);
	}

	static void HideStatus() {
		ShowWindow(g_hStatus, SW_HIDE);
	}

	// ---------- 热键注册 ----------
	static void ApplyHotkey() {
		UnregisterHotKey(g_hWnd, HOTKEY_ID);
		RegisterHotKey(g_hWnd, HOTKEY_ID, g_mod, g_key);
	}

	// ---------- 托盘 ----------
	static void AddTrayIcon() {
		ZeroMemory(&g_nid, sizeof(g_nid));
		g_nid.cbSize = sizeof(NOTIFYICONDATA);
		g_nid.hWnd = g_hWnd;
		g_nid.uID = 1;
		g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		g_nid.uCallbackMessage = WM_APP_TRAY;
		g_nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
		wcscpy_s(g_nid.szTip, L"打字模拟器");
		Shell_NotifyIconW(NIM_ADD, &g_nid);
	}
	static void RemoveTrayIcon() {
		Shell_NotifyIconW(NIM_DELETE, &g_nid);
	}

	static void ShowTrayMenu() {
		POINT pt; GetCursorPos(&pt);
		HMENU m = CreatePopupMenu();
		AppendMenuW(m, MF_STRING, ID_TRAY_SHOW, L"显示窗口");
		AppendMenuW(m, MF_SEPARATOR, 0, NULL);
		AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, L"退出");
		SetForegroundWindow(g_hWnd);
		TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, NULL);
		DestroyMenu(m);
	}

	// ---------- 模拟键入 ----------
	static void SimulateTextInput(std::wstring text) {
		INPUT rel[3] = {};
		WORD vklist[] = { VK_CONTROL, VK_MENU, VK_SHIFT };
		for (int i = 0; i < 3; ++i) {
			rel[i].type = INPUT_KEYBOARD;
			rel[i].ki.wVk = vklist[i];
			rel[i].ki.dwFlags = KEYEVENTF_KEYUP;
		}
		SendInput(3, rel, sizeof(INPUT));
		Sleep(80);

		for (wchar_t ch : text) {
			if (ch == L'\r') continue;
			INPUT in[2] = {};
			if (ch == L'\n') {
				in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_RETURN;
				in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = VK_RETURN;
				in[1].ki.dwFlags = KEYEVENTF_KEYUP;
			} else {
				in[0].type = INPUT_KEYBOARD; in[0].ki.wScan = ch;
				in[0].ki.dwFlags = KEYEVENTF_UNICODE;
				in[1].type = INPUT_KEYBOARD; in[1].ki.wScan = ch;
				in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
			}
			SendInput(2, in, sizeof(INPUT));
			Sleep(20);
		}
	}

	// ---------- 菜单 ----------
	static void BuildMenu() {
		HMENU root = CreateMenu();
		HMENU mFile = CreatePopupMenu();
		HMENU mEdit = CreatePopupMenu();
		HMENU mView = CreatePopupMenu();
		AppendMenuW(mEdit, MF_STRING, ID_EDIT_SET_HOTKEY,  L"设置快捷键(&S)");
		AppendMenuW(mView, MF_STRING, ID_VIEW_SHOW_HOTKEY, L"当前快捷键(&C)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mFile, L"文件(&F)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mEdit,  L"编辑(&E)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mView,  L"查看(&V)");
		SetMenu(g_hWnd, root);
	}

	// ---------- 窗口过程 ----------
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

		if (msg == WM_CREATE) {
			g_hWnd = hWnd;
			BuildMenu();
			g_hEdit = CreateWindowExW(
				WS_EX_CLIENTEDGE, L"EDIT", L"",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL |
				ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
				0, 0, 0, 0, hWnd, (HMENU)(UINT_PTR)ID_EDIT_TEXT, g_hInst, NULL);
			g_hStatus = CreateWindowExW(
				0, L"STATIC", L"", WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
				0, 0, 0, 24, hWnd, NULL, g_hInst, NULL);
			HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessageW(g_hEdit,   WM_SETFONT, (WPARAM)hf, TRUE);
			SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)hf, TRUE);
			HideStatus();
			AddTrayIcon();
			ApplyHotkey();
			return 0;
		}

		if (msg == WM_SIZE) {
			RECT rc; GetClientRect(hWnd, &rc);
			int barH = 24;
			MoveWindow(g_hEdit, 0, 0, rc.right, rc.bottom - barH, TRUE);
			MoveWindow(g_hStatus, 0, rc.bottom - barH, rc.right, barH, TRUE);
			return 0;
		}

		if (msg == WM_COMMAND) {
			switch (LOWORD(wParam)) {
			case ID_EDIT_SET_HOTKEY:
				g_bCapturing = true;
				UnregisterHotKey(g_hWnd, HOTKEY_ID);
				ShowStatusMomentarily(L"请按下新快捷键（按 ESC 取消）...");
				SetFocus(g_hWnd);
				break;
			case ID_VIEW_SHOW_HOTKEY: {
				std::wstring tip = L"当前快捷键: " + HotkeyLabel();
				ShowStatusMomentarily(tip);
				SetTimer(g_hWnd, IDT_HIDE_STATUS, 2000, NULL);
				break;
			}
			case ID_TRAY_SHOW:
				ShowWindow(hWnd, SW_SHOW);
				SetForegroundWindow(hWnd);
				break;
			case ID_TRAY_EXIT:
				DestroyWindow(hWnd);
				break;
			}
			return 0;
		}

		if (msg == WM_TIMER && wParam == IDT_HIDE_STATUS) {
			KillTimer(g_hWnd, IDT_HIDE_STATUS);
			if (!g_bCapturing) HideStatus();
			return 0;
		}

		if (msg == WM_HOTKEY && wParam == HOTKEY_ID) {
			int len = GetWindowTextLengthW(g_hEdit);
			if (len > 0) {
				std::wstring buf(len + 1, L'\0');
				GetWindowTextW(g_hEdit, &buf[0], len + 1);
				buf.resize(len);
				std::thread thr([buf]() { SimulateTextInput(buf); });
				thr.detach();
			}
			return 0;
		}

		if (msg == WM_APP_TRAY) {
			if (lParam == WM_RBUTTONUP) ShowTrayMenu();
			else if (lParam == WM_LBUTTONUP) {
				ShowWindow(hWnd, SW_SHOW);
				SetForegroundWindow(hWnd);
			}
			return 0;
		}

		if (msg == WM_KEYDOWN && g_bCapturing) {
			if (wParam == VK_ESCAPE) {
				g_bCapturing = false;
				HideStatus();
				ApplyHotkey();
				return 0;
			}
			if (wParam == VK_CONTROL || wParam == VK_MENU ||
				wParam == VK_SHIFT || wParam == VK_LWIN || wParam == VK_RWIN)
				return 0;
			return 0;
		}

		if (msg == WM_KEYUP && g_bCapturing) {
			if (wParam == VK_CONTROL || wParam == VK_MENU ||
				wParam == VK_SHIFT || wParam == VK_LWIN || wParam == VK_RWIN)
				return 0;

			UINT snap = 0;
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000) snap |= MOD_CONTROL;
			if (GetAsyncKeyState(VK_MENU)    & 0x8000) snap |= MOD_ALT;
			if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) snap |= MOD_SHIFT;
			if ((GetAsyncKeyState(VK_LWIN)&0x8000)||(GetAsyncKeyState(VK_RWIN)&0x8000)) snap |= MOD_WIN;

			g_mod = snap;
			g_key = (UINT)wParam;
			g_bCapturing = false;
			HideStatus();
			ApplyHotkey();
			return 0;
		}

		if (msg == WM_CLOSE) {
			ShowWindow(hWnd, SW_HIDE);
			return 0;
		}

		if (msg == WM_DESTROY) {
			UnregisterHotKey(g_hWnd, HOTKEY_ID);
			RemoveTrayIcon();
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	// ---------- 入口 ----------
	int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
		g_hInst = hInst;

		HANDLE hMutex = CreateMutexW(NULL, TRUE, L"TypingSimulator_SingleInstance_v2");
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			HWND hOther = FindWindowW(L"TypingSimulatorClassV2", NULL);
			if (hOther) { ShowWindow(hOther, SW_SHOW); SetForegroundWindow(hOther); }
			if (hMutex) CloseHandle(hMutex);
			return 0;
		}

		HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCE(IDI_ICON1));

		WNDCLASSEXW wc = {};
		wc.cbSize        = sizeof(WNDCLASSEXW);
		wc.lpfnWndProc   = WndProc;
		wc.hInstance     = hInst;
		wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wc.lpszClassName = L"TypingSimulatorClassV2";
		wc.hIcon         = hIcon;
		wc.hIconSm       = hIcon;

		if (!RegisterClassExW(&wc)) return 1;

		HWND hw = CreateWindowExW(
			0, L"TypingSimulatorClassV2", L"记事本",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 620, 420,
			NULL, NULL, hInst, NULL);
		if (!hw) return 1;

		ShowWindow(hw, nCmdShow);
		UpdateWindow(hw);

		MSG msg;
		while (GetMessageW(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (hMutex) CloseHandle(hMutex);
		return (int)msg.wParam;
	}