#include <stdio.h>
#include <assert.h>
#include <tchar.h>
#include <windows.h>
#include <gdiplus.h>


#pragma comment(lib,"gdiplus.lib")
#pragma comment(lib,"msimg32.lib")

#define ARGB_TO_COLORREF(a) (((a & 0xff0000)>>16)|(a & 0x00ff00)|((a & 0x0000ff)<<16))
#define COLORREF_TO_ARGB(a) (0xff000000|((a & 0xff0000)>>16)|(a & 0x00ff00)|((a & 0x0000ff)<<16))

#define ARGB_GET_A(argb) ((argb & 0xff000000) >> 24)
#define ARGB_GET_R(argb) ((argb & 0x00ff0000) >> 16)
#define ARGB_GET_G(argb) ((argb & 0x0000ff00) >> 8)
#define ARGB_GET_B(argb) ((argb & 0x000000ff))

//create ARGB HBITMAP
HBITMAP CreateBit32Bitmap(int w, int h, void** outppbits) {
	void** ppbits = NULL;
	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(bih));
	bih.biSize = sizeof(bih);
	bih.biWidth = w;
	bih.biHeight = 0 - h;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;
	if (outppbits)ppbits = outppbits;
	return CreateDIBSection(NULL, (BITMAPINFO*)&bih, DIB_RGB_COLORS, ppbits, NULL, 0);
}

//load image as speciel width and height
HBITMAP LoadImageFromFile(LPCTSTR file, int w, int h) {
	HBITMAP result = NULL;
	Gdiplus::Bitmap* image = Gdiplus::Bitmap::FromFile(file);
	if (image && image->GetWidth()) {
		HDC dc = ::CreateCompatibleDC(NULL);
		result = CreateBit32Bitmap(w, h, NULL);
		SelectObject(dc, result);

		Gdiplus::Graphics gp(dc);
		gp.DrawImage(image, 0, 0, w, h);
		SelectObject(dc, NULL);
		DeleteDC(dc);
		delete image;
	}
	return result;
}

//merger mask image with color
HBITMAP CreateColoredImage(HBITMAP maskimage, COLORREF color, int w, int h) {
	int pixelsz = w * h;
	int bytesz = pixelsz * 4;
	void* buf = NULL;
	HBITMAP newbmp = CreateBit32Bitmap(w,h,&buf);
	if (!newbmp)return NULL;
	GetBitmapBits(maskimage, bytesz, buf);

	int dst; 
	int dst_a;
	int i;
	int* pixel = (int*)buf;
	int src = COLORREF_TO_ARGB(color);
	int src_r = GetRValue(color);
	int src_g = GetGValue(color);
	int src_b = GetBValue(color);
		
	for(i = 0;i<pixelsz;i++){
		dst = *pixel;
		dst_a = ARGB_GET_A(dst);
		if(dst_a == 255){
			*pixel = src;
		}else if(dst_a == 0){
			*pixel = 0;
		}else{
			*pixel = (dst & 0xff000000) | 
				((src_r * dst_a / 256) << 16) |
				((src_g * dst_a / 256) << 8) |
				((src_b * dst_a / 256));
		}
		pixel ++;
	}
	
	return newbmp;
}

//merger mask image with image
HBITMAP CreateMergeredImage(HBITMAP maskimage, HBITMAP image, int w, int h) {
	int pixelsz = w * h;
	int bytesz = pixelsz * 4;
	void* buf = NULL;
	HBITMAP newbmp = CreateBit32Bitmap(w, h, &buf);
	if (!newbmp)return NULL;
	LONG b1 = GetBitmapBits(maskimage, bytesz, buf);
	void* buf2 = malloc(bytesz);
	LONG b2 = GetBitmapBits(image, bytesz, buf2);

	int dst;
	int dst_a;
	int i;
	int* pixel = (int*)buf;
	int* pixelsrc = (int*)buf2;
	int src;

	for (i = 0; i<pixelsz; i++) {
		dst = *pixel;
		src = *pixelsrc;
		dst_a = ARGB_GET_A(dst);
		if (dst_a == 255) {
			*pixel = src;
		}
		else if (dst_a == 0) {
			*pixel = 0;
		}
		else {
			*pixel = (dst & 0xff000000) |
				((GetBValue(src) * dst_a / 256) << 16) |
				((GetGValue(src) * dst_a / 256) << 8) |
				((GetRValue(src) * dst_a / 256));
		}
		pixel++;
		pixelsrc++;
	}
	free(buf2);
	return newbmp;
}

//draw image
VOID DrawAlphaImage(HDC hdc, HBITMAP image, LPRECT rc) {
	int x, y, w, h;
	HDC memdc = CreateCompatibleDC(NULL);
	BLENDFUNCTION bf = { 0 };
	bf.SourceConstantAlpha = 255;
	bf.AlphaFormat = AC_SRC_ALPHA;
	
	x = rc->left;
	y = rc->top;
	w = rc->right - x;
	h = rc->bottom - y;
	SelectObject(memdc, image);
	BOOL ret = AlphaBlend(hdc, x, y, w, h, memdc, 0, 0, w, h, bf);
	DeleteDC(memdc);
}


//global resource

static HBITMAP maskimage1;
static HBITMAP maskimage2;
static HBITMAP image;

static HBITMAP image1withcolor;
static HBITMAP image2withcolor;
static HBITMAP image1mergerd;
static HBITMAP image2mergerd;

static COLORREF color;
static HBRUSH colorbrush;


BOOL LoadResources() {
	color = RGB(30,60,200);
	colorbrush = CreateSolidBrush(color);

	maskimage1 = LoadImageFromFile(TEXT("a.png"), 100, 100);	
	maskimage2 = LoadImageFromFile(TEXT("b.png"), 100, 100);	
	image = LoadImageFromFile(TEXT("cat.png"), 100, 100);	
	if (!(image && maskimage1 && maskimage2))return FALSE;

	image1withcolor = CreateColoredImage(maskimage1, color, 100, 100);
	image2withcolor = CreateColoredImage(maskimage2, color, 100, 100);

	image1mergerd = CreateMergeredImage(maskimage1, image, 100, 100);
	image2mergerd = CreateMergeredImage(maskimage2, image, 100, 100);

	return TRUE;
}


VOID PaintMainWindow(HWND hwnd){
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
	RECT rc = { 0,0,600,500 };
	RECT rc11 = { 10,10,110,110 };
	RECT rc12 = { 120,10,220,110 };
	RECT rc13 = { 230,10,330,110 };
	RECT rc21 = { 10,120,110,220 };
	RECT rc22 = { 120,120,220,220 };	
	RECT rc23 = { 230,120,330,220 };
	RECT rc31 = { 10,230,110,330 };
	RECT rc32 = { 120,230,220,330 };
	RECT rc33 = { 230,230,330,330 };
	
	FillRect(hdc, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));

	DrawAlphaImage(hdc, maskimage1, &rc11);
	DrawAlphaImage(hdc, maskimage2, &rc12);	

	DrawAlphaImage(hdc, image1withcolor, &rc21);
	DrawAlphaImage(hdc, image2withcolor, &rc22);
	FillRect(hdc, &rc23, colorbrush);

	DrawAlphaImage(hdc, image1mergerd, &rc31);
	DrawAlphaImage(hdc, image2mergerd, &rc32);
	DrawAlphaImage(hdc, image, &rc33);

    EndPaint(hwnd, &ps);
}


LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){	
    switch (msg){
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_PAINT:
            PaintMainWindow(hwnd);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 1;
}

int WINAPI _tWinMain(HINSTANCE hInst,HINSTANCE hPrev,LPTSTR lpCmdLine,int nCmdShow){
	//init gdiplus
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	
	//load images
	BOOL loadok = LoadResources();
	assert(loadok);

	//register class
    WNDCLASS wc={0};
    wc.lpfnWndProc = MainWindowProc;    
    wc.lpszClassName = TEXT("mainwnd");
    BOOL regok = RegisterClass(&wc);
	assert(regok);
    
	//create window
    HWND mainwnd = CreateWindowEx(0,TEXT("mainwnd"),_T("CustomAlphaBlend"),
        (WS_OVERLAPPED|WS_CAPTION|WS_MINIMIZEBOX|WS_SYSMENU),
        CW_USEDEFAULT,CW_USEDEFAULT,400,400,
        NULL,NULL,hInst,NULL);
	assert(mainwnd);    
    ShowWindow(mainwnd, nCmdShow);
        
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);        
    }
    return msg.wParam;
}

