
#include <tchar.h>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <stdio.h>
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"dwmapi.lib")



void OnActive(HWND hwnd, WPARAM wp) {
	MARGINS margins;

	margins.cxLeftWidth = 0xffff;  //for make whole client area aero 
	margins.cxRightWidth = 0xffff;
	margins.cyBottomHeight = 0;
	margins.cyTopHeight = 30;  //for draw title bar in to client area

	DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void OnCreate(HWND hwnd) {
	RECT rc;
	GetWindowRect(hwnd, &rc);
	//for generate WM_NCCALCSIZE message
	SetWindowPos(hwnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED);
}

//for remove title bar of standard window frame
LRESULT OnNcCalcSize(HWND hwnd, WPARAM wp, LPARAM lp) {
	if (wp) {
		NCCALCSIZE_PARAMS* pncsp = (NCCALCSIZE_PARAMS*)lp;
		memcpy(&pncsp->rgrc[2], &pncsp->rgrc[1], sizeof(RECT));
		memcpy(&pncsp->rgrc[1], &pncsp->rgrc[0], sizeof(RECT));
		pncsp->rgrc[0].left = pncsp->rgrc[0].left + 8;
		pncsp->rgrc[0].top = pncsp->rgrc[0].top + 0;
		pncsp->rgrc[0].right = pncsp->rgrc[0].right - 8;
		pncsp->rgrc[0].bottom = pncsp->rgrc[0].bottom - 8;
		return WVR_VALIDRECTS;
	}
	else {
		return DefWindowProc(hwnd, WM_NCCALCSIZE, wp, lp);
	}
}

//hit test of custom window frame
LRESULT OnNcHitTest(HWND hwnd, WPARAM wp, LPARAM lp, LRESULT dwmret) {
	if (dwmret)return dwmret;

	LRESULT ret = DefWindowProc(hwnd, WM_NCHITTEST, wp, lp);
	if (ret && ret != HTCLIENT)return ret;

	RECT rc;
	GetWindowRect(hwnd, &rc);
	int x = GET_X_LPARAM(lp);
	int y = GET_Y_LPARAM(lp);
	int row = -1;
	int col = -1;
	const int sizeborder = 3;
	const LRESULT hitresult[4][3] =
	{
		{ HTTOPLEFT,    HTTOP,     HTTOPRIGHT },
		{ HTLEFT,       HTCAPTION, HTRIGHT },
		{ HTLEFT,       HTCLIENT,  HTRIGHT },
		{ HTBOTTOMLEFT, HTBOTTOM,  HTBOTTOMRIGHT },
	};

	if (x >= rc.left && x <= rc.right) {
		if (x < rc.left + sizeborder) {
			col = 0;
		}
		else if (x > rc.right - sizeborder) {
			col = 2;
		}
		else {
			col = 1;
		}
	}
	if (y >= rc.top && y <= rc.bottom) {
		if (y < rc.top + sizeborder) {
			row = 0;
		}
		else if (y < rc.top + 30) {
			row = 1;
		}
		else if (y > rc.bottom - sizeborder) {
			row = 3;
		}
		else {
			row = 2;
		}
	}
	if (row < 0 || col < 0) {
		return HTNOWHERE;
	}
	return hitresult[row][col];
}

//fill alpha of dc 
void FillAlpha(HDC hdc, int x, int y, int w, int h, BYTE alpha) {
	UINT color = ((UINT)alpha << 24);
	BITMAPINFO bmi;
	memset(&bmi.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = 1;
	bmi.bmiHeader.biHeight = 0 - 1;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	ZeroMemory(bmi.bmiColors, sizeof(RGBQUAD));

	StretchDIBits(hdc, x, y, w, h, 0, 0, 1, 1, &color, &bmi, DIB_RGB_COLORS, SRCPAINT);
}

// clip region for exclude when painting
HRGN CreateClipRgn(HWND hwnd) {
	const int clipw = 140;
	const int cliph = 30;
	RECT rc;
	GetClientRect(hwnd, &rc);
	POINT pt[] = {
		{rc.right - clipw - cliph, 0},
		{rc.right, 0},
		{rc.right, cliph},
		{rc.right - clipw, cliph},
	};
	return CreatePolygonRgn(pt, 4, ALTERNATE);
}

//paint window
void OnPaint(HWND hwnd) {
	PAINTSTRUCT ps;
	HDC phdc = BeginPaint(hwnd, &ps);

	RECT wrc = { 0,0,0,0 };
	GetClientRect(hwnd, &wrc);

	HDC memdc = CreateCompatibleDC(phdc);
	HBITMAP membmp = CreateCompatibleBitmap(phdc, wrc.right - wrc.left, wrc.bottom - wrc.top);
	SelectObject(memdc, membmp);

	HDC hdc = memdc;

	//exlude min,max,close area of titlebar
	HRGN rgn = CreateClipRgn(hwnd);
	ExtSelectClipRgn(hdc, rgn, RGN_DIFF);

	//fill whole backgroud
	const int toph = 8;
	RECT bkrc = { 0,toph, wrc.right - wrc.left, wrc.bottom - wrc.top };
	FillRect(hdc, &bkrc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	//draw something
	RECT rc = { 10,8 + toph,100,100 };
	SelectObject(hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	DrawText(hdc, TEXT("hello world"), -1, &rc, 0);

	//fill alpha of dc to 0xff
	FillAlpha(hdc, 0, toph, wrc.right - wrc.left, wrc.bottom - wrc.top - toph, 0xff);

	BitBlt(phdc, 0, 0, wrc.right - wrc.left, wrc.bottom - wrc.top, memdc, 0, 0, SRCCOPY);

	EndPaint(hwnd, &ps);

	DeleteObject(rgn);
	DeleteObject(membmp);
	DeleteDC(memdc);
}


LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	LRESULT lRet = 0;
	//DwmDefWindowProc for hit test of close button, draw effects of titlebar, etc.
	BOOL bret = DwmDefWindowProc(hwnd, message, wParam, lParam, &lRet);

	switch (message) {
	case WM_CREATE:
		OnCreate(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ACTIVATE:
		OnActive(hwnd, wParam);
		break;
	case WM_PAINT:
		OnPaint(hwnd);
		break;
	case WM_NCCALCSIZE:
		return OnNcCalcSize(hwnd, wParam, lParam);
		break;
	case WM_NCHITTEST:
		return OnNcHitTest(hwnd, wParam, lParam, lRet);
		break;
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 1;
}


int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPTSTR lpCmdLine, int nCmdShow) {

	WNDCLASS wc = { 0 };
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpfnWndProc = MainWindowProc;
	wc.hInstance = hInst;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = TEXT("mainwnd");
	BOOL ret = RegisterClass(&wc);
	if (!ret) {
		return 1;
	}

	HWND wnd = CreateWindowEx(0, TEXT("mainwnd"), _T(""),
		(WS_OVERLAPPEDWINDOW),
		100, 100, 300, 230,
		NULL, NULL, hInst, NULL);
	if (!wnd) {
		return 1;
	}
	ShowWindow(wnd, nCmdShow);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

	}
	return msg.wParam;
}

