/*
author: churuxu
https://github.com/churuxu/WindowsEffects
*/

#include <assert.h>
#include <math.h>
#include <tchar.h>
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

//create ARGB HBITMAP
HBITMAP CreateBit32Bitmap(int w, int h, void** outppbits) {
	void** ppbits = outppbits? outppbits:NULL;
	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(bih));
	bih.biSize = sizeof(bih);
	bih.biWidth = w;
	bih.biHeight = 0 - h;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;	
	return CreateDIBSection(NULL, (BITMAPINFO*)&bih, DIB_RGB_COLORS, ppbits, NULL, 0);
}

//create HDC support opengl
HDC CreateDCOpenGL(HBITMAP membmp) {
	HDC dc = CreateCompatibleDC(NULL);
	SelectObject(dc, membmp);
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_BITMAP;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE;
	pfd.cAlphaBits = 8;
	int pf = ChoosePixelFormat(dc, &pfd);
	SetPixelFormat(dc, pf, &pfd);
	return dc;
}

//create opengl texture   
GLuint CreateTextureFromHBitmap(HBITMAP bitmap) {
	BITMAP bmp;
	GetObject(bitmap, sizeof(BITMAP), (LPBYTE)&bmp);
	/* 0x80E0 = GL_BGR_EXT   0x80E1 = GL_BGRA_EXT  */
	GLenum format = (bmp.bmBitsPixel == 32)? GL_RGBA: GL_RGB; 	
	GLuint texid = 0;
	glGenTextures(1, &texid);
	if (texid) {
		glBindTexture(GL_TEXTURE_2D, texid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glPixelStoref(GL_PACK_ALIGNMENT, 1);
		//width && height must be 2^n
		glTexImage2D(GL_TEXTURE_2D, 0, 3, bmp.bmWidth, bmp.bmHeight, 0, format, GL_UNSIGNED_BYTE, bmp.bmBits);
		GLenum err = glGetError();
		if (err) {			
			glDeleteTextures(1, &texid);
			texid = 0;
		}
	}
	return texid;
}

//
HBITMAP CaptureWindowImage(HWND hwnd) {
	void* bits = NULL;
	RECT rc = {0};
	HDC hdc = GetWindowDC(hwnd);
	HDC memdc = CreateCompatibleDC(hdc);	
	GetWindowRect(hwnd, &rc);
	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;
	HBITMAP bitmap = CreateBit32Bitmap(w, h, &bits);	
	SelectObject(memdc, bitmap);
	BOOL ret = BitBlt(memdc, 0, 0, w, h, hdc, 0, 0, SRCPAINT);	
	SelectObject(memdc, NULL);
	DeleteDC(memdc);
	ReleaseDC(hwnd, hdc);	
	return bitmap;
}

void RenderRectangleTexture(GLuint tex) {
	const GLfloat vtx[] = {
		-1,1,   1,1,
		-1,-1,  1,-1
	};
	const GLfloat cood[] = {
		0,1,  1,1,
		0,0,  1,0
	};		
	glBindTexture(GL_TEXTURE_2D, tex);	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);	
	glVertexPointer(2, GL_FLOAT, 0, vtx);
	glTexCoordPointer(2, GL_FLOAT, 0, cood);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);	
}

HGLRC InitGL(HDC hdc, int w, int h) {
	HGLRC hglrc = wglCreateContext(hdc);
	assert(hglrc);
	BOOL ret = wglMakeCurrent(hdc, hglrc);
		
	glEnable(GL_TEXTURE_2D);	
	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE);

	double z = 9;
	double c = sqrt(1.0f + z*z);
	double fovy = acos(z / c) * 360.0 / 3.1415926;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fovy, (float)w/(float)h, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();	
	gluLookAt(0, 0, z, 0, 0, 0, 0, 1, 0);

	return hglrc;
}


static HGLRC glrc_;
static HWND layerwnd_;
static HWND animwnd_;
static HBITMAP membmp_;
static void* membits_;
static HDC memdc_;
static HBITMAP wndcap_;
static GLuint wndcaptex_;
static int animstep_;
static UINT_PTR animtimer_;
static int w_ = 256;
static int h_ = 256;
static int x_;
static int y_;


BOOL CreateAnimationResources(HWND hwnd) {	
	wndcap_ = CaptureWindowImage(hwnd);
	wndcaptex_ = CreateTextureFromHBitmap(wndcap_);
	return (wndcap_ && wndcaptex_);
}

void DeleteAnimationResources() {
	DeleteObject(wndcap_);
	glDeleteTextures(1, &wndcaptex_);
}

void GLRender(int step) {
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT );
	glPushMatrix();
	if (step < 36) {
		GLfloat z = (GLfloat)((abs(step - 18) - 18) * (0.2));
		glTranslatef(0, 0, z);
		glRotatef((GLfloat)step*10.0f, 0.0, 1.0, 0.0);
	}
	RenderRectangleTexture(wndcaptex_);
	
	glPopMatrix();
	glFlush();	
}

void UpdateAnimateFrame(int step) {
	HDC hdc = GetDC(layerwnd_);

	GLRender(step);

	glReadPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, membits_);

	POINT ptwin = { x_, y_ };
	SIZE szwin = { w_, h_ };
	POINT ptsrc = { 0,0 };
	BLENDFUNCTION bf = { 0 };
	bf.SourceConstantAlpha = 255;
	bf.AlphaFormat = AC_SRC_ALPHA;
	BOOL ret = UpdateLayeredWindow(layerwnd_, hdc, &ptwin, &szwin, memdc_, &ptsrc, RGB(0,0,0), &bf, ULW_ALPHA);

	ReleaseDC(layerwnd_, hdc);	
}

void CALLBACK OnAnimateTimer(HWND hwnd, UINT message, UINT_PTR iTimerID, DWORD dwTime) {
	animstep_++;	
	UpdateAnimateFrame(animstep_);	
	if (animstep_ == 37) {
		ShowWindowAsync(animwnd_, SW_SHOW);
	}
	if (animstep_ > 40) {		
		DeleteAnimationResources();
		KillTimer(NULL, animtimer_);
		ShowWindow(layerwnd_, SW_HIDE);		
	}
}

void StartRotateWindow(HWND hwnd) {
	animwnd_ = hwnd;
	if (CreateAnimationResources(hwnd)) {
		RECT rc;
		GetWindowRect(hwnd, &rc);
		ShowWindow(hwnd, SW_HIDE);		
		x_ = rc.left;
		y_ = rc.top;
		animstep_ = 0;
		SetWindowPos(layerwnd_, NULL, x_, y_, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);		
		UpdateAnimateFrame(0);		
		animtimer_ = SetTimer(NULL,0,33, OnAnimateTimer);
	} else {
		MessageBox(NULL,_T("Error"),NULL,MB_OK);
	}
}

void MainWindowPaint(HWND hwnd) {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT rc = { 30,80,220,150 };
	SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
	SetBkMode(hdc, TRANSPARENT);
	DrawText(hdc, _T("click to rotate window"), -1, &rc, 0);
	EndPaint(hwnd, &ps); 
}
LRESULT CALLBACK MainWndProc(HWND hWnd,UINT nMsg, WPARAM wParam, LPARAM lParam){
    switch(nMsg){
		case WM_PAINT:
			MainWindowPaint(hWnd);
			break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_LBUTTONUP:
			StartRotateWindow(hWnd);			
            break;
		default:
			break;
    }
    return DefWindowProc(hWnd, nMsg, wParam, lParam);
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmd,int nShow){
	//class for main window
    WNDCLASS wc1 = {0};
    wc1.lpszClassName = _T("mainwnd");
    wc1.lpfnWndProc = MainWndProc;
	wc1.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	wc1.hCursor = LoadCursor(NULL, IDC_ARROW);
	BOOL bret1 = RegisterClass(&wc1);
	assert(bret1);

	//class for animation window
	WNDCLASS wc2 = { 0 };
	wc2.lpszClassName = _T("openglwnd");
	wc2.lpfnWndProc = DefWindowProc;
	BOOL  bret2 = RegisterClass(&wc2);
	assert(bret2);

	//create opengl resources
	layerwnd_ = CreateWindowEx(WS_EX_LAYERED, _T("openglwnd"), NULL, 0,
		CW_USEDEFAULT, CW_USEDEFAULT, w_, h_,
		NULL, NULL, hInst, NULL);
	assert(layerwnd_);	
	membmp_ = CreateBit32Bitmap(w_, h_, &membits_);
	assert(membmp_);
	memdc_ = CreateDCOpenGL(membmp_);	
	assert(memdc_);
	glrc_ = InitGL(memdc_, w_, h_);
	assert(glrc_);

	//create main window
    HWND hMainWnd = CreateWindowEx(0, _T("mainwnd"), _T("3D Animation Demo"), WS_CAPTION| WS_OVERLAPPED|WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, w_, h_,
        NULL, NULL, hInst, NULL);
    assert(hMainWnd);
    ShowWindow(hMainWnd, nShow);   

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

