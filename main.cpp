	#include "resource.h"
	#include <windows.h>
	#include <commctrl.h>
	#include <string>
	#include <thread>

	#pragma comment(lib, "user32.lib")
	#pragma comment(lib, "gdi32.lib")
	#pragma comment(lib, "shell32.lib")
	#pragma comment(lib, "comctl32.lib")

	#pragma comment(linker, "\"/manifestdependency:type='win32' \
	name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
	processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

	#define ID_EDIT_TEXT          1001
	#define ID_TRAY_SHOW          2003
	#define ID_TRAY_EXIT          2002
	#define ID_EDIT_SET_HOTKEY    2004
	#define ID_VIEW_SHOW_HOTKEY   2005
	#define IDT_HIDE_STATUS       100
	#define IDT_WATCH_CARET       101
	#define WM_APP_TRAY           (WM_APP + 1)
	#define WM_APP_DO_TYPE        (WM_APP + 2)
	#define HOTKEY_ID             1

	// 状态栏分区
	#define SB_PART_COUNT  4

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

	// ---------- 状态栏（仿记事本）----------
	static void UpdateNotepadStatus() {
		DWORD selStart = 0, selEnd = 0;
		SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
		int line = (int)SendMessageW(g_hEdit, EM_LINEFROMCHAR, selStart, 0) + 1;
		int lineStart = (int)SendMessageW(g_hEdit, EM_LINEINDEX, line - 1, 0);
		int col = (int)selStart - lineStart + 1;

		wchar_t buf[128];
		wsprintfW(buf, L"第 %d 行，第 %d 列", line, col);
		SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)buf);
		SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)L"100%");
		SendMessageW(g_hStatus, SB_SETTEXTW, 2, (LPARAM)L"Windows (CRLF)");
		SendMessageW(g_hStatus, SB_SETTEXTW, 3, (LPARAM)L"UTF-8");
	}

	static void ShowStatusMomentarily(const std::wstring& txt) {
		SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)txt.c_str());
	}

	static void HideStatus() {
		UpdateNotepadStatus();
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

	// ---------- 菜单（仿记事本）----------
	static void BuildMenu() {
		HMENU root = CreateMenu();
		HMENU mFile = CreatePopupMenu();
		HMENU mEdit = CreatePopupMenu();
		HMENU mFormat = CreatePopupMenu();
		HMENU mView = CreatePopupMenu();
		HMENU mHelp = CreatePopupMenu();

		// 文件(F)
		AppendMenuW(mFile, MF_STRING, 0, L"新建(&N)\tCtrl+N");
		AppendMenuW(mFile, MF_STRING, 0, L"新建窗口(&W)\tCtrl+Shift+N");
		AppendMenuW(mFile, MF_STRING, 0, L"打开(&O)...\tCtrl+O");
		AppendMenuW(mFile, MF_STRING, 0, L"保存(&S)\tCtrl+S");
		AppendMenuW(mFile, MF_STRING, 0, L"另存为(&A)...\tCtrl+Shift+S");
		AppendMenuW(mFile, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mFile, MF_STRING, 0, L"页面设置(&U)...");
		AppendMenuW(mFile, MF_STRING, 0, L"打印(&P)...\tCtrl+P");
		AppendMenuW(mFile, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mFile, MF_STRING, ID_TRAY_EXIT, L"退出(&X)");

		// 编辑(E)
		AppendMenuW(mEdit, MF_STRING, 0, L"撤销(&U)\tCtrl+Z");
		AppendMenuW(mEdit, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mEdit, MF_STRING, 0, L"剪切(&T)\tCtrl+X");
		AppendMenuW(mEdit, MF_STRING, 0, L"复制(&C)\tCtrl+C");
		AppendMenuW(mEdit, MF_STRING, 0, L"粘贴(&P)\tCtrl+V");
		AppendMenuW(mEdit, MF_STRING, 0, L"删除(&L)\tDel");
		AppendMenuW(mEdit, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mEdit, MF_STRING, 0, L"使用 Bing 搜索(&S)...\tCtrl+E");
		AppendMenuW(mEdit, MF_STRING, 0, L"查找(&F)...\tCtrl+F");
		AppendMenuW(mEdit, MF_STRING, 0, L"查找下一个(&N)\tF3");
		AppendMenuW(mEdit, MF_STRING, 0, L"查找上一个(&V)\tShift+F3");
		AppendMenuW(mEdit, MF_STRING, 0, L"替换(&R)...\tCtrl+H");
		AppendMenuW(mEdit, MF_STRING, 0, L"转到(&G)...\tCtrl+G");
		AppendMenuW(mEdit, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mEdit, MF_STRING, 0, L"全选(&A)\tCtrl+A");
		AppendMenuW(mEdit, MF_STRING, 0, L"时间/日期(&D)\tF5");
		// 自定义功能
		AppendMenuW(mEdit, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mEdit, MF_STRING, ID_EDIT_SET_HOTKEY, L"设置触发快捷键(&S)...");

		// 格式(O)
		AppendMenuW(mFormat, MF_STRING | MF_CHECKED, 0, L"自动换行(&W)");
		AppendMenuW(mFormat, MF_STRING, 0, L"字体(&F)...");

		// 查看(V)
		AppendMenuW(mView, MF_STRING, 0, L"缩放(&Z)");
		AppendMenuW(mView, MF_STRING, 0, L"状态栏(&S)");
		AppendMenuW(mView, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mView, MF_STRING, ID_VIEW_SHOW_HOTKEY, L"当前快捷键(&C)");

		// 帮助(H)
		AppendMenuW(mHelp, MF_STRING, 0, L"查看帮助(&H)");
		AppendMenuW(mHelp, MF_STRING, 0, L"发送反馈(&F)");
		AppendMenuW(mHelp, MF_SEPARATOR, 0, NULL);
		AppendMenuW(mHelp, MF_STRING, 0, L"关于记事本(&A)");

		AppendMenuW(root, MF_POPUP, (UINT_PTR)mFile,   L"文件(&F)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mEdit,   L"编辑(&E)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mFormat, L"格式(&O)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mView,   L"查看(&V)");
		AppendMenuW(root, MF_POPUP, (UINT_PTR)mHelp,   L"帮助(&H)");
		SetMenu(g_hWnd, root);
	}

	// ---------- 窗口过程 ----------
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

		if (msg == WM_CREATE) {
			g_hWnd = hWnd;
			BuildMenu();

			// 编辑框：无边框，纯白背景
			g_hEdit = CreateWindowExW(
				0, L"EDIT", L"",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL |
				ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
				0, 0, 0, 0, hWnd, (HMENU)(UINT_PTR)ID_EDIT_TEXT, g_hInst, NULL);

			// 状态栏
			g_hStatus = CreateWindowExW(
				0, STATUSCLASSNAMEW, L"",
				WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				0, 0, 0, 0, hWnd, NULL, g_hInst, NULL);

			// 状态栏分区（先给个默认，WM_SIZE 里会重算）
			int parts[SB_PART_COUNT] = { 200, 280, 380, -1 };
			SendMessageW(g_hStatus, SB_SETPARTS, SB_PART_COUNT, (LPARAM)parts);

			HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			SendMessageW(g_hEdit,   WM_SETFONT, (WPARAM)hf, TRUE);
			SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)hf, TRUE);

			// 编辑框左右留 8px 内边距
			SendMessageW(g_hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
				MAKELPARAM(8, 8));

			AddTrayIcon();
			ApplyHotkey();
			UpdateNotepadStatus();

			// 轮询光标位置，替代 EDIT 不支持的 EN_SELCHANGE
			SetTimer(hWnd, IDT_WATCH_CARET, 200, NULL);
			return 0;
		}

		if (msg == WM_SIZE) {
			RECT rc; GetClientRect(hWnd, &rc);

			SendMessageW(g_hStatus, WM_SIZE, 0, 0);
			RECT rcStatus; GetWindowRect(g_hStatus, &rcStatus);
			int statusH = rcStatus.bottom - rcStatus.top;

			int editH = rc.bottom - statusH;
			if (editH < 0) editH = 0;

			MoveWindow(g_hEdit, 0, 0, rc.right, editH, TRUE);
			MoveWindow(g_hStatus, 0, editH, rc.right, statusH, TRUE);

			int w = rc.right;
			int parts[SB_PART_COUNT] = { w - 320, w - 220, w - 120, -1 };
			SendMessageW(g_hStatus, SB_SETPARTS, SB_PART_COUNT, (LPARAM)parts);
			return 0;
		}

		if (msg == WM_COMMAND) {
			WORD id = LOWORD(wParam);
			WORD code = HIWORD(wParam);
			if (id == ID_EDIT_TEXT &&
				(code == EN_CHANGE || code == EN_UPDATE ||
				 code == EN_SETFOCUS || code == EN_KILLFOCUS)) {
				UpdateNotepadStatus();
			}

			switch (id) {
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

		if (msg == WM_TIMER && wParam == IDT_WATCH_CARET) {
			static DWORD lastSel = 0xFFFFFFFF;
			DWORD selStart = 0, selEnd = 0;
			SendMessageW(g_hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
			if (selStart != lastSel) {
				lastSel = selStart;
				UpdateNotepadStatus();
			}
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
			KillTimer(g_hWnd, IDT_WATCH_CARET);
			UnregisterHotKey(g_hWnd, HOTKEY_ID);
			RemoveTrayIcon();
			PostQuitMessage(0);
			return 0;
		}

		// 编辑框背景刷白
		if (msg == WM_CTLCOLOREDIT) {
			HDC hdc = (HDC)wParam;
			SetBkColor(hdc, RGB(255, 255, 255));
			SetTextColor(hdc, RGB(0, 0, 0));
			static HBRUSH hWhite = CreateSolidBrush(RGB(255, 255, 255));
			return (LRESULT)hWhite;
		}
		if (msg == WM_CTLCOLORSTATIC) {
			HDC hdc = (HDC)wParam;
			SetBkColor(hdc, GetSysColor(COLOR_BTNFACE));
			return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
		}

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	// ---------- 入口 ----------
	int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
		g_hInst = hInst;

		// 初始化 Common Controls（状态栏需要）
		INITCOMMONCONTROLSEX icc = {};
		icc.dwSize = sizeof(icc);
		icc.dwICC  = ICC_BAR_CLASSES;
		InitCommonControlsEx(&icc);

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
		wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
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