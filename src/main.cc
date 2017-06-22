/*******************************************
 MAIN.C - HP Series 80 Emulator
	Copyright 2006-17 Everett Kaser
	All rights reserved.
 See HP85EM.TXT for legal usage.
 *******************************************/

/*********************  Header Files  *********************/

#include "main.hh"
#include "mach85.hh"
#include "mainwindow.h"
#include "tape.h"

#include <stdio.h>
#include <stdlib.h>

#if TODO
#include <mmsystem.h>
#include <commdlg.h>
#include <io.h>
#endif

#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

//#include <winbase.h>

#if TODO
#include "resource.h"
#endif


Environment_Linux linux_environment;

const Environment* getEnvironment()
{
  return &linux_environment;
}


FILE* Environment_Linux::openEmulatorFile(std::string path) const
{
  std::string fullPath = "../";
  fullPath += path;

  return fopen(fullPath.c_str(), "rb");
}


/*
Bitmap* Environment_Linux::createBitmap(int w,int h) const
{
  return new Bitmap_Console85(w,h);
}
*/


static HPMachine machine;
static TapeDrive tapeDrive;

HPMachine* getMachine()
{
  return &machine;
}


Bitmap* EmulatorUI_Qt::createCRTBitmap(int w,int h) const
{
  mMainWindow->getCRTWidget()->createBitmap(w,h);
  return mMainWindow->getCRTWidget();
}


static EmulatorUI_Qt ui;

EmulatorUI* getUI()
{
  return &ui;
}

#include <QApplication>
#include <QTimer>


void EmulatorUI_Qt::create()
{
  mMainWindow = new MainWindow();
  mMainWindow->show();
}


void EmulatorUI_Qt::runIdle()
{
  machine.HP85OnIdle();
  QTimer::singleShot(0, [&]() { runIdle(); });
}


int qwe_main(int argc, char** argv)
{
  machine.setConfig(CFG_AUTORUN);
  machine.HP85OnStartup();

  for (;;) {
  machine.HP85OnIdle();
  }

  return 0;
}


int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  //MainWindow* win = new MainWindow;
  //win->show();

  ui.create();

  machine.setConfig(CFG_AUTORUN);

  machine.addPeripheral(&tapeDrive);
  strcpy(CurTape,"Standard Pak");
  //tapeDrive.LoadTape();

  machine.HP85OnStartup();

  //QTimer::singleShot(0, []() { machine.HP85OnIdle(); });
  QTimer::singleShot(0, []() { ui.runIdle(); });

  return app.exec();
}



#if TODO
/*******************  Global Variables ********************/

HANDLE		ghInstance;
HWND		hWndMain=NULL;
HDC			ddc, sdc;
HBITMAP		oldBMs, oldBMd;
HCURSOR		hCursorOld;

int		bitspixel, planes, numcolors, sizepalette, colormode;

int		PhysicalScreenW, PhysicalScreenH, PhysicalWndW, PhysicalWndH;
int		LastMainWndX=32767, LastMainWndY;

WORD	MinFlag=FALSE;

UINT	TimerID=0;

char	PgmPath[PATHLEN], TmpPath[PATHLEN], RenPath[PATHLEN];

char	szAppName[] = TEXT("Series 80 Emulator 4.0.6");
char	KWinName[] = TEXT("Series 80 Sub-window");

int		CurCur=0;

WINDOWPLACEMENT	WndPlace;
struct _finddata_t	fdat;

DWORD			hPRT;
PAGESETUPDLG	psd;
HFONT			hPRTFont=NULL;
DOCINFO			PRTdocinfo = {sizeof(DOCINFO), "HP-85", NULL, NULL, 0};

KWND KWnds[MAXKWND];
long KWndCnt=0;

//********************************************************************

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
//********************************************************************
// callback for enumerate windows to bring previous instance of this program to the foreground
// for each top-level window, get it's class name.  If it matches "our" class name, then
// send it a message to get a ptr to it's program path.  If it matches THIS program path,
// we've found the previous instance, and we bring it to the foreground.
BOOL CALLBACK FindPrevWnd(HWND hWnd, LPARAM lParam) {

	GetClassName(hWnd, TmpPath, PATHLEN);
	if( !strcmp(TmpPath, szAppName) ) {
		if( !strcmp((char *)SendMessage(hWnd, UM_GETPATH, 0, 0), PgmPath) ) {
			SetForegroundWindow(hWnd);
			return FALSE;
		}
	}
	return TRUE;
}

//********************************************************************
// Main entry point for Windows programs
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpszCmdLine, int nCmdShow) {

	WNDCLASS	wc, kc;
	MSG			msg;
	int			i;
//	HANDLE		hmutex;

	ghInstance = hInstance;

// Get path program was run from and save in PgmPath
// ALL other file access by this program are relative to PgmPath.
	i = GetModuleFileName((HMODULE)ghInstance, PgmPath, PATHLEN-1);

	while( i && PgmPath[i]!='\\' && PgmPath[i]!=':' ) --i;
	if( PgmPath[i]=='\\' || PgmPath[i]==':' ) ++i;
	if( !i || PgmPath[i-1]!='\\' ) PgmPath[i++] = '\\';
	PgmPath[i] = 0;

// see if a copy of this program is already running
// if so, just activate it and kill this copy,
// else let this copy proceed.

	// convert program folder path backslashes to underscores
	for(i=0; PgmPath[i]; i++) {
		if( PgmPath[i]=='\\' ) TmpPath[i] = '_';
		else TmpPath[i] = PgmPath[i];
	}
	TmpPath[i] = 0;
	// create a Windows mutex.  if it succeeds, this is the first instance of the program
//	hmutex =
	CreateMutex(NULL, FALSE, TmpPath);
	// if it fails, there's another instance already running
	if( GetLastError()==ERROR_ALREADY_EXISTS ) {
		// find the other instance and bring it to the foreground, then terminate this instance
		EnumWindows((WNDENUMPROC)FindPrevWnd, 0);
		return FALSE;
	}

// register our window class
	wc.lpfnWndProc		= MainWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInstance;
	wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= szAppName;
	wc.style			= CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	wc.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	RegisterClass( &wc );

	// register sub window class
	kc.lpfnWndProc		= KWndProc;
	kc.cbClsExtra		= 0;
	kc.cbWndExtra		= 0;
	kc.hInstance		= hInstance;
	kc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	kc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	kc.lpszMenuName		= NULL;
	kc.lpszClassName	= KWinName;
	kc.style			= CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
	kc.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	RegisterClass( &kc );

	memset(&KWnds[0], 0, sizeof(KWnds));	// set all sub-window handles to destroyed.

	SupportStartup();

//===================================================================
// create the default (screen) bitmap.
// the code does ALL of it's drawing to KWnds[0].pBM, a memory bitmap.
// when Windows needs to update the REAL screen, the WM_PAINT message
// in this program merely BitBlt's from KWnds[0].pBM to the screen DC.
	InitScreen(0, 0);
// create main window
	hWndMain = CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW,
		475, 10, KINT_WND_SIZEW, KINT_WND_SIZEH, NULL, NULL, hInstance, NULL);

	if( !hWndMain ) {
		SupportShutdown();
		return FALSE;
	} else {
		long int sc;
		long	x, y, w, h;

		KGetWindowPos(NULL, &x, &y, &w, &h, &sc);

		KWnds[0].hWnd = KWnds[0].pBM->hWnd = hWndMain;
		KWnds[0].x = x;
		KWnds[0].y = y;
		KWnds[0].w = w;
		KWnds[0].h = h;
		strncpy(KWnds[0].title, szAppName, 95);
		KWnds[0].title[95] = 0;	// make sure nul-terminated if src was longer than 95 chars
		KWndCnt = 1;
	}
//====================================================================

	timeBeginPeriod(1);

	HP85OnStartup();

// cause the window to be shown on the display.
	ShowWindow(hWndMain, nCmdShow);

// this is the main "message pump" loop.
	for(;;) {
		// check to see if we can do idle work
/****/	while( !PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) ) {
			HP85OnIdle();
			if( Halt85 ) Sleep(0);
		}
		// we have a message
		if( GetMessage(&msg, NULL, 0, 0)==TRUE ) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else break;
	}
	return msg.wParam;
}

//********************************************************************************
// This is the main MS Windows message ("event") handling function.
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {

	long			i, x, y;
	HBITMAP			holdbm;
	HDC				hdc, bdc;
	PAINTSTRUCT		ps;
	RECT			rect;

/*
	These are here for debugging purposes, so particular (unknown) messages
	can get trapped without trapping on a bunch of VERY common messages that
	are of no interest.

	if( msg!=WM_SETFOCUS && msg!=WM_KILLFOCUS && msg!=WM_PAINT && msg!=WM_ERASEBKGND
	  && msg!=WM_SHOWWINDOW && msg!=WM_SETCURSOR && msg!=WM_TIMER
	   && msg!=WM_WINDOWPOSCHANGING && msg!=WM_GETMINMAXINFO && msg!=WM_NCCALCSIZE
	    && msg!=WM_NCPAINT && msg!=WM_GETTEXT && msg!=WM_WINDOWPOSCHANGED
	    && msg!=WM_NCHITTEST && msg!=WM_NCACTIVATE && msg!=WM_MOUSEMOVE
		&& msg!=WM_ACTIVATEAPP && msg!=WM_ACTIVATE
		 && msg!=WM_CREATE && msg!=WM_SETTEXT && msg!=WM_MOVE && msg!=WM_SIZE
		 && msg!=WM_KEYUP && msg!=WM_KEYDOWN && msg!=WM_LBUTTONDOWN
		 && msg!=WM_LBUTTONUP && msg!=WM_NCCREATE && msg!=WM_NCMOUSEMOVE
		) {
	  msg = msg;
	}
*/
	switch( msg ) {
	 case UM_GETPATH:
	// our own custom message to get the path to the other instances of
	// the program, so that we can figure out if programs by the same
	// name are actually the same program or not.  While
	// the name of the EXE *MIGHT* be the same, the folders will be different,
	// so we check BOTH the program AND the folder from which it was run.)
	// See the FindPrevWnd() function above.
		return (LRESULT)PgmPath;
	 case 0x020A:	// Mouse WHEEL
		GetWindowRect(hWnd, &rect);
		x = GET_X_LPARAM(lParam) - (rect.left+GetSystemMetrics(SM_CXSIZEFRAME));//+GetSystemMetrics(SM_CXBORDER));
		y = GET_Y_LPARAM(lParam) - (rect.top+WINDOW_TITLEY);//+GetSystemMetrics(SM_CYBORDER));
		HP85OnWheel(x, y, ((short)(HIWORD(wParam)))/120);

		return 0;
	 case WM_ERASEBKGND:
	// never erase background, because we always draw everything
		return TRUE;
	 case WM_SIZE:
	// the program window has been resized, minimized, or maximized
		if( wParam==SIZE_MINIMIZED ) {
			MinFlag = TRUE;
		} else {
			RECT	rc;

			GetClientRect(hWnd, &rc);
			KWnds[0].pBM->logW = KWnds[0].w = (WORD)(rc.right-rc.left);
			KWnds[0].pBM->logH = KWnds[0].h = (WORD)(rc.bottom-rc.top);
			ClipBMB(0, -1, 0, 0, 0);
			HP85OnResize();
			MinFlag = FALSE;
		}
		return 0;
	 case WM_MOVE:
	 	x = (long)((short)LOWORD(lParam));
	 	y = (long)((short)HIWORD(lParam));
	 	DefWindowProc(hWnd, msg, wParam, lParam);
	 	if( LastMainWndX<32767 ) {	// don't adjust other Windows on the first placement of this window
			for(i=1; i<KWndCnt; i++) {
				if( KWnds[i].hWnd!=NULL ) {
					KWnds[i].x += (x - LastMainWndX);
					KWnds[i].y += (y - LastMainWndY);
					KSetWindowPos(KWnds[i].hWnd, KWnds[i].x, KWnds[i].y, KWnds[i].w, KWnds[i].h, SWP_NOSIZE | SWP_NOZORDER);
				}
			}
	 	}
		KWnds[0].x = LastMainWndX = x;
		KWnds[0].y = LastMainWndY = y;
		return 0;
	 case WM_TIMER:
	// the timer time period has expired
		HP85OnTimer();
		return 0;
	 case WM_SYSKEYDOWN:
		if( wParam==VK_F10 ) {
			HP85OnKeyDown(wParam);
			return 0;
		}
		break;
	 case WM_SYSKEYUP:
		if( wParam==VK_F10 ) {
			HP85OnKeyUp(wParam);
			return 0;
		}
		break;
	 case WM_KEYDOWN:
		if( HP85OnKeyDown(wParam) ) return 0;
		break;
	 case WM_KEYUP:
		if( HP85OnKeyUp(wParam) ) return 0;
		break;
	 case WM_CHAR:
//	 	if( wParam=='w' ) {
/*
char tbuff[128];
sprintf(tbuff, "physWH=(%d,%d) logWH=(%d,%d) clip=(%d,%d)(%d,%d)",
		KWnds[0].pBM->physW, KWnds[0].pBM->physH,
		KWnds[0].pBM->logW, KWnds[0].pBM->logH,
		KWnds[0].pBM->clipX1, KWnds[0].pBM->clipY1,
		KWnds[0].pBM->clipX2, KWnds[0].pBM->clipY2
		);
MessageBox(NULL, tbuff, "DEBUG", MB_OK);
*/
//	 	}
		if( wParam>=' ' ) HP85OnChar(wParam);
		return 0;
	 case WM_MOUSEMOVE:
	// the mouse (pointer) has been moved
	// x = (long)(short)(WORD)LOWORD(lParam);
	// y = (long)(short)(WORD)HIWORD(lParam);
	// flags = wParam;
		HP85OnMouseMove((long)(short)(WORD)LOWORD(lParam), (long)(short)(WORD)HIWORD(lParam), wParam);
		return 0;
	 case WM_LBUTTONDOWN:
	// the LEFT mouse button has been pressed
/*
WM_LBUTTONDOWN
	wParam = key flags
		MK_CONTROL	Set if the CTRL key is down.
		MK_LBUTTON	Set if the left mouse button is down.
		MK_MBUTTON	Set if the middle mouse button is down.
		MK_RBUTTON	Set if the right mouse button is down.
		MK_SHIFT	Set if the SHIFT key is down.
	LOWORD(lParam) = horizontal position of cursor
	HIWORD(lParam) = vertical position of cursor
*/
		HP85OnLButtDown(LOWORD(lParam), HIWORD(lParam), wParam);
		return 0;
	 case WM_LBUTTONUP:
		HP85OnLButtUp((long)(short)(WORD)LOWORD(lParam), (long)(short)(WORD)HIWORD(lParam), wParam);
		return 0;
	 case WM_MBUTTONDOWN:
	// the MIDDLE mouse button has been pressed
		HP85OnMButtDown(LOWORD(lParam), HIWORD(lParam), wParam);
		return 0;
	 case WM_MBUTTONUP:
	// the MIDDLE mouse button has been released
		HP85OnMButtUp(LOWORD(lParam), HIWORD(lParam), wParam);
		return 0;
	 case WM_RBUTTONDOWN:
	// the RIGHT mouse button has been pressed
		HP85OnRButtDown(LOWORD(lParam), HIWORD(lParam), wParam);
		return 0;
	 case WM_RBUTTONUP:
	// the RIGHT mouse button has been released
		HP85OnRButtUp(LOWORD(lParam), HIWORD(lParam), wParam);
		return 0;
	 case WM_PAINT:
	// the program window needs updating, repaint it from the kBM bitmap
		hdc = BeginPaint(hWnd, &ps);

		SetBkColor(hdc, PALETTERGB(255,255,255));
		SetTextColor(hdc, PALETTERGB(0,0,0));

		bdc=CreateCompatibleDC(hdc);
		holdbm = (HBITMAP)SelectObject(bdc, KWnds[0].pBM->hBitmap);

		if( NULLREGION !=GetClipBox(hdc, &rect) ) {
			BitBlt(hdc, rect.left, rect.top, rect.right-rect.left+1, rect.bottom-rect.top+1, bdc, rect.left, rect.top, SRCCOPY);
		}
		SelectObject(bdc, holdbm);
		DeleteDC(bdc);

		EndPaint(hWnd, &ps);
		return 0;
	 case WM_DESTROY:
		if( TimerID!=0 ) {
			KillTimer(hWndMain, TimerID);
			TimerID = 0;
		}

		HP85OnShutdown();
		SupportShutdown();

		timeEndPeriod(1);

		PostQuitMessage(0);
		return 0;
	}
	return( DefWindowProc(hWnd, msg, wParam, lParam));
}

//**********************************************************************
//* Called from PMACH.CPP when the .KE wants the program to terminate
//* Do whatever's necessary to make the program terminate.
//*
void KillProgram() {

	DestroyWindow(hWndMain);
}

//***********************************************************************
void KGetWndWH(long *w, long *h) {

	RECT	rc;

	GetClientRect(hWndMain, &rc);
	*w = (WORD)(rc.right-rc.left);
	*h = (WORD)(rc.bottom-rc.top);
}

//***********************************************************************
void KSetWindowPos(HWND wnd, long x, long y, long w, long h, DWORD sc) {

	WndPlace.length = sizeof(WINDOWPLACEMENT);
	WndPlace.flags = 0;
	WndPlace.showCmd = (UINT)sc;
	WndPlace.rcNormalPosition.left = (int)x;
	WndPlace.rcNormalPosition.top  = (int)y;
	WndPlace.rcNormalPosition.right= (int)x+(int)w;
	WndPlace.rcNormalPosition.bottom=(int)y+(int)h;
	WndPlace.ptMaxPosition.x = 0;
	WndPlace.ptMaxPosition.y = 0;
	SetWindowPlacement(wnd==NULL?hWndMain:wnd, &WndPlace);

//	SetWindowPos(wnd==NULL?hWndMain:wnd, NULL, x, y, w, h, SWP_NOZORDER);
}

//**************************************************************************
void KGetWindowPos(HWND hWnd, long *x, long *y, long *w, long *h, long *sc) {

	UINT	showCmd;

	WndPlace.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(hWnd==NULL?hWndMain:hWnd, &WndPlace);
	*x = (DWORD)(WndPlace.rcNormalPosition.left);
	*y = (DWORD)(WndPlace.rcNormalPosition.top);
	*w = (DWORD)(WndPlace.rcNormalPosition.right-WndPlace.rcNormalPosition.left);
	*h = (DWORD)(WndPlace.rcNormalPosition.bottom-WndPlace.rcNormalPosition.top);
	showCmd = WndPlace.showCmd;
	if( showCmd==SW_MINIMIZE ) showCmd = SW_RESTORE;
	if( showCmd==SW_SHOWMINIMIZED ) showCmd = SW_SHOWNORMAL;
	*sc = (DWORD)showCmd;

}

//*******************************************************************
void KPlaySound(char *pMem) {

	BuildPath(pMem);
	sndPlaySound(TmpPath, SND_ASYNC | SND_NODEFAULT);
}

//******************************************************************
void StartTimer(DWORD timval) {

// create periodic timer
	if( TimerID==0 ) TimerID = SetTimer(hWndMain, 1, (UINT)timval, 0);
}

//******************************************************************
void StopTimer() {

	if( TimerID!=0 ) KillTimer(hWndMain, TimerID);
	TimerID = 0;
}

//****************************************
void InitScreen(long minW, long minH) {

	HDC			hDC;

	hDC = GetDC(NULL);
	PhysicalScreenW = GetDeviceCaps(hDC, HORZRES);
	PhysicalScreenH = GetDeviceCaps(hDC, VERTRES);
	ReleaseDC(NULL, hDC);

	if( KWndCnt && KWnds[0].pBM!=NULL ) DeleteBMB(KWnds[0].pBM);

	if( PhysicalScreenW > minW ) minW = PhysicalScreenW;
	if( PhysicalScreenH > minH ) minH = PhysicalScreenH;

	KWnds[0].pBM = CreateBMB((WORD)minW, (WORD)minH, (HWND)-1);// dummy non-NULL HWND, will get fixed up after main window creation
	if( NULL==KWnds[0].pBM ) {
		SystemMessage("Failed creating screen BITMAP");
		exit(0);
	}
// initialize the ENTIRE screen bitmap to black
	RectBMB(0, 0, 0, KWnds[0].pBM->physW, KWnds[0].pBM->physH, 0x00000000, -1);

	KWnds[0].hWnd = KWnds[0].pBM->hWnd = NULL;
	KWnds[0].w = KINT_WND_SIZEW;
	KWnds[0].h = KINT_WND_SIZEH;
}

//********************************************************************
// Display the NULL-terminated message in 'pMem' using OS resources.
void SystemMessage(char *pMem) {

	MessageBox(NULL, pMem, TEXT("DEBUG"), MB_OK);
}

//*********************************************************************
// store the appropriate End-Of-Line sequence into buffer 'pMem' and
// return the number of bytes written to 'pMem'.
int GetSystemEOL(BYTE *pMem) {

	*pMem++ = '\r';
	*pMem++ = '\n';
	*pMem = 0;
	return 2;
}

//***************************************
// hWnd==-1 when creating a screen window BMB
// hWnd==NULL for strictly "memory" bitmaps
PBMB CreateBMB(WORD w, WORD h, HWND hWnd) {

	PBMB	pX;

	pX = (PBMB)malloc(sizeof(BMB));
	if( pX!=NULL ) {
		pX->linelen = 4*((3*w+3)/4);	// force line of pixels to take even number of DWORDS
		pX->pBits = NULL;
		if( hWnd!=(HWND)-1 && NULL==(pX->pBits=(BYTE *)malloc(pX->linelen*h)) ) {
			free(pX);
			pX = NULL;
		} else {
			pX->pAND = NULL;
			pX->pNext = (void *)pbFirst;
			if( pbFirst!=NULL ) pbFirst->pPrev = (void *)pX;
			pX->pPrev = NULL;
			pbFirst = pX;
			pX->hBitmap = NULL;
			pX->physW = w;
			pX->physH = h;
			pX->clipX1 = pX->clipY1 = 0;
			pX->clipX2 = (int)w;
			pX->clipY2 = (int)h;
			pX->logW = w;
			pX->logH = h;
			pX->hWnd = hWnd;
			if( hWnd==(HWND)-1 ) {
				BITMAPINFO			pbmi;

				pbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				pbmi.bmiHeader.biWidth = (long)w;
				pbmi.bmiHeader.biHeight = -(long)h;	// negative to make bitmap "top-down" addressing
				pbmi.bmiHeader.biPlanes = 1;
				pbmi.bmiHeader.biBitCount = 24;
				pbmi.bmiHeader.biSizeImage = pX->linelen*h;
				pbmi.bmiHeader.biXPelsPerMeter = pbmi.bmiHeader.biYPelsPerMeter = pbmi.bmiHeader.biClrUsed = pbmi.bmiHeader.biClrImportant = pbmi.bmiHeader.biCompression = 0;

				if( NULL==(pX->hBitmap=CreateDIBSection(NULL, &pbmi, DIB_RGB_COLORS, (void **)&pX->pBits, NULL, 0)) ) {
					DeleteBMB(pX);
					pX = NULL;
				}
			}
		}
	}
	return pX;
}

//***************************************
void DeleteBMB(PBMB pX) {

	if( pX!=NULL ) {
		if( pX->pAND!= NULL ) free(pX->pAND);
		if( pX->hBitmap != NULL ) DeleteObject(pX->hBitmap);
		else if( pX->pBits != NULL ) free(pX->pBits);
		if( pX==pbFirst ) {
			pbFirst = (PBMB)pX->pNext;
			if( pbFirst!=NULL ) pbFirst->pPrev = NULL;
		} else {
			if( pX->pNext!=NULL ) ((PBMB)pX->pNext)->pPrev = pX->pPrev;
			if( pX->pPrev!=NULL ) ((PBMB)pX->pPrev)->pNext = pX->pNext;
		}
		free(pX);
	}
}

//**********************************************************************
void KInvalidate(PBMB pBM, int x1, int y1, int x2, int y2) {

	int		i, t;
	RECT	rect;

	for(i=0; i<KWndCnt; i++) {
		if( KWnds[i].hWnd!=NULL && pBM==KWnds[i].pBM ) {
			if( x1>x2 ) {
				t  = x1;
				x1 = x2;
				x2 = t;
			}
			if( y1>y2 ) {
				t  = y1;
				y1 = y2;
				y2 = t;
			}
			rect.left = x1;
			rect.top  = y1;
			rect.right= x2;
			rect.bottom=y2;

			InvalidateRect(KWnds[i].hWnd, &rect, FALSE);
			break;
		}
	}
}

//**********************************************************************
void FlushScreen() {

	UpdateWindow(hWndMain);
}

//******************************************************************
void KInvalidateAll() {

	int		i;

	for(i=0; i<KWndCnt; i++) {
		if( KWnds[i].hWnd!=NULL ) InvalidateRect(KWnds[i].hWnd, NULL, TRUE);
	}
}

//*********************************************************
// This routine MUST build the full file path in 'TmpPath', as that's
// where the calling functions expect to find it.
//
void BuildPath(char *pMem) {

	strcpy(TmpPath, PgmPath);
	strcat(TmpPath, pMem);
}

//*********************************************************************************
// make a sub-folder of the program folder or a sub-folder thereof
// return non-zero if successful, else return zero if fail
int KMakeFolder(char *fname) {

	BuildPath(fname);
	return CreateDirectory(TmpPath, NULL);
}

//*********************************************************************************
// Delete an empty folder (can NOT contain ANY files)
void KDelFolder(char *fname) {

	BuildPath(fname);
	RemoveDirectory(TmpPath);
}

//*********************************************************************************
// return file handle if successful, else -1
int Kopen(char *fname, int oflags) {

	BuildPath(fname);

	return _open(TmpPath, oflags, _S_IREAD | _S_IWRITE);
}

//*********************************************************************************
// return file handle if successful, else -1
int KopenAbs(char *fname, int oflags) {

	return _open(fname, oflags, _S_IREAD | _S_IWRITE);
}

//********************************************************************************
// 'origin' has these values (in case your language is different):
//		SEEK_SET    0
//		SEEK_CUR    1
//		SEEK_END    2
// return new file pointer position or -1 if error
long Kseek(int fhandle, DWORD foffset, int origin) {

	return _lseek((int)fhandle, (long)foffset, origin);
}

//*********************************************************************************
void Kclose(int fhandle) {

	_close(fhandle);
}

//********************************************************************************
//	return 1 if at EOF, 0 if not at EOF, -1 if error
int Keof(int fhandle) {

	return _eof(fhandle);
}

//******************************************************
long Kread(int fhandle, BYTE *pMem, long numbytes) {

	long	bytesread, chunk, need;

	bytesread = 0;
	while( numbytes ) {
		if( numbytes<16384 ) need = numbytes;
		else need = 16384;
		chunk = _read(fhandle, (void *)pMem, need);
		if( chunk==-1 ) {
			bytesread = -1;
			break;
		}
		if( chunk==0 ) break;
		bytesread += chunk;
		pMem += chunk;
		numbytes -= chunk;
	}
	return bytesread;
}

//*******************************************************
long Kwrite(int fhandle, BYTE *pMem, long numbytes) {

	long	byteswritten, chunk, need;

	byteswritten = 0;
	while( numbytes ) {
		if( numbytes<16384 ) need = numbytes;
		else need = 16384;
		chunk = _write(fhandle, (void *)pMem, need);
		if( chunk==-1 ) {
			byteswritten = -1;
			break;
		}
		byteswritten += chunk;
		pMem += chunk;
		numbytes -= chunk;
	}
	return byteswritten;
}

//******************************************************************
// Search for a filename (possibly containing '*' as wild card)
long Kfindfirst(char *fname, BYTE *fbuf, BYTE *attrib) {

	long	ffhandle;
	char	*pChar;

	if( fname[0]!='\\' && fname[1]!=':' ) BuildPath(fname);
	else strcpy(TmpPath, fname);

	ffhandle = _findfirst(TmpPath, &fdat);

	if( ffhandle != -1 ) {
		*attrib = (BYTE)fdat.attrib & _A_SUBDIR;

	// find JUST the filename portion
		for(pChar=fdat.name+strlen(fdat.name); pChar>fdat.name && *(pChar-1)!='\\'; --pChar) ;
		strcpy((char *)fbuf, pChar);
	}

	return ffhandle;
}

//********************************************************************
// Search for next filename in sequence started by Kfindfirst()
long Kfindnext(long ffhandle, BYTE *fbuf, BYTE *attrib) {

	long	err;
	char	*pChar;

	err = _findnext((long)ffhandle, &fdat);

	if( !err ) {
		*attrib = (BYTE)fdat.attrib & _A_SUBDIR;
	// find JUST the filename portion
		for(pChar=fdat.name+strlen(fdat.name); pChar>fdat.name && *(pChar-1)!='\\'; --pChar) ;
		strcpy((char *)fbuf, pChar);
	}
	return err;
}

//****************************************************
// Terminate the Kfindfirst()/Kfindnext() process
void Kfindclose(long ffhandle) {

	_findclose(ffhandle);
}

//*************************************************
void Kdelete(char *fname) {

	BuildPath(fname);
	_unlink(TmpPath);
}

//*************************************************
long KFileSize(char *fname) {

	int		fh;
	long	len=0;

	if( -1!=(fh=Kopen(fname, O_RDBIN)) ) {
		len = Kseek(fh, 0, SEEK_END);
		Kclose(fh);
	}
	return len;
}

//*************************************************
void Krename(char *oname, char *nname) {

	BuildPath(oname);
	strcpy(RenPath, PgmPath);
	strcat(RenPath, nname);
	rename(TmpPath, RenPath);
}

//*****************************************************
WORD KCopyFile(char *src, char *dst) {

	BuildPath(src);
	return (WORD)CopyFile(TmpPath, dst, FALSE);
}

//*****************************************************
void KGetTime(KTIME *pT) {

	SYSTEMTIME	curtime;

	GetLocalTime(&curtime);
	pT->y = (WORD)curtime.wYear;
	pT->mo= (WORD)curtime.wMonth;
	pT->d = (WORD)curtime.wDay;
	pT->h = (WORD)curtime.wHour;
	pT->mi= (WORD)curtime.wMinute;
	pT->s = (WORD)curtime.wSecond;
}

//******************************************************
void KTimeToString(BYTE *pMem) {

	SYSTEMTIME	curtime;

	GetLocalTime(&curtime);
	sprintf((char *)pMem, "%d-%d-%d %2.2d:%2.2d:%2.2d", curtime.wMonth, curtime.wDay, curtime.wYear, curtime.wHour, curtime.wMinute, curtime.wSecond);
}

//****************************************************************************
//		Return a millisecond count which doesn't necessarily bear
//		any resemblance to "real" time, but rather is a counter that is
//		a reasonably accurate "millisecond counter" that may eventually
//		roll over to 0, but which should not do so more often than once a day,
//		and who's count value continues upwards (until such time as a roll-over
//		might occur).  This is accomplished on the MS-Windows platform using
//		the GetTickCount() function.  Other platforms may choose to implement
//		this in whatever way seems easiest, while meeting the above criteria.
//	It could be done getting the time of day, and subtracting that from the
//  time of day when the program started, but the Windows GetTickCount() function
//	works nicely, too.
DWORD KGetTicks() {

//	return GetTickCount();
	return timeGetTime();
}
#endif

uint32_t Environment_Linux::getTicks() const
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  uint32_t msecs = tv.tv_sec * 1000 + tv.tv_usec/1000;
}


#if TODO

//**********************************************************
void KSetTitle(BYTE *pMem) {

	SetWindowText(hWndMain, (char *)pMem);
}

//***********************************************************
// Return 0 if fail, non-0 if succeed (and set pw, ph, Xppi, Yppi)
// Get everything setup for printing which is to follow
DWORD KPrintStart(DWORD *pw, DWORD *ph, DWORD *Xppi, DWORD *Yppi) {

	HDC					hdc;
/*
typedef struct tagPSD {  // psd
    DWORD           lStructSize;
    HWND            hwndOwner;
    HGLOBAL         hDevMode;
    HGLOBAL         hDevNames;
    DWORD           Flags;
    POINT           ptPaperSize;
    RECT            rtMinMargin;
    RECT            rtMargin;
    HINSTANCE       hInstance;
    LPARAM          lCustData;
    LPPAGESETUPHOOK lpfnPageSetupHook;
    LPPAGEPAINTHOOK lpfnPagePaintHook;
    LPCTSTR         lpPageSetupTemplateName;
    HGLOBAL         hPageSetupTemplate;
} PAGESETUPDLG, * LPPAGESETUPDLG;
*/
	char	*pNames;

	psd.lStructSize = sizeof(PAGESETUPDLG);
	psd.hwndOwner = hWndMain;
	psd.hDevMode  = NULL;
	psd.hDevNames = NULL;
	psd.Flags =    PSD_DISABLEMARGINS
				 | PSD_DISABLEPAGEPAINTING
				 | PSD_DISABLEPAPER
				 | PSD_INTHOUSANDTHSOFINCHES
//				 | PSD_RETURNDEFAULT
				 ;
	psd.hInstance = NULL;//ghInstance;
	psd.lCustData = 0;
	psd.lpfnPageSetupHook = NULL;
	psd.lpfnPagePaintHook = NULL;
	psd.lpPageSetupTemplateName = NULL;
	psd.hPageSetupTemplate = NULL;
	if( PageSetupDlg(&psd) ) {
		pNames = (char *)GlobalLock(psd.hDevNames);
		hdc = CreateDC(	pNames + ((DEVNAMES *)pNames)->wDriverOffset,
						pNames + ((DEVNAMES *)pNames)->wDeviceOffset, NULL, NULL);
		GlobalUnlock(psd.hDevNames);

		*pw = (DWORD)(long)GetDeviceCaps(hdc, HORZRES);
		*ph = (DWORD)(long)GetDeviceCaps(hdc, VERTRES);
		*Xppi = (DWORD)(long)GetDeviceCaps(hdc, LOGPIXELSX);
		*Yppi = (DWORD)(long)GetDeviceCaps(hdc, LOGPIXELSY);
		if( StartDoc(hdc, &PRTdocinfo) <= 0 ) {
			DeleteDC(hdc);
			hdc = NULL;
		}
	} else {
		hdc = NULL;
	}

	hPRT = (DWORD)hdc;
	return hPRT;
}

//*******************************************************
// Shutdown and clean up after printing, we're all through
void KPrintStop() {

	if( hPRT!=0 ) {
		EndDoc((HDC)hPRT);
		DeleteDC((HDC)hPRT);
		hPRT = 0;
	}
	if( hPRTFont!=NULL ) DeleteObject(hPRTFont);
}

//*****************************************************
//	return non-0 if successful, 0 if fail
BYTE KPrintBeginPage() {

	return (StartPage((HDC)hPRT) > 0);
}

//*****************************************************
//	return non-0 if successful, 0 if fail
BYTE KPrintEndPage() {

	return (EndPage((HDC)hPRT) > 0);
}

//******************************************************
void KPrintText(int x, int y, char *str, DWORD clr) {

	char	*pT;
	HFONT	hOldFont=NULL;

	SetBkMode((HDC)hPRT, TRANSPARENT);
	SetTextColor((HDC)hPRT, (COLORREF)clr);
	if( hPRTFont!=NULL ) hOldFont = SelectObject((HDC)hPRT, hPRTFont);
	SetTextAlign((HDC)hPRT, TA_LEFT | TA_TOP);
	pT = str;
	TextOut((HDC)hPRT, x, y, pT, strlen(pT));
	if( hPRTFont!=NULL ) SelectObject((HDC)hPRT, hOldFont);
}

//***********************************************************
void KPrintGetTextSize(char *str, DWORD *dw, DWORD *dh, DWORD *dasc) {

	char		*pT;
	TEXTMETRIC	tm;
	SIZE		txtsize;
	HFONT		hOldFont=NULL;

	if( hPRTFont!=NULL ) hOldFont = SelectObject((HDC)hPRT, hPRTFont);
	SetTextAlign((HDC)hPRT, TA_LEFT | TA_TOP);
	pT = str;
	GetTextMetrics((HDC)hPRT, &tm);
	GetTextExtentPoint32((HDC)hPRT, pT, strlen(pT), &txtsize);
	*dw = txtsize.cx;
	*dh = txtsize.cy;
	*dasc=tm.tmAscent;
	if( hPRTFont!=NULL ) SelectObject((HDC)hPRT, hOldFont);
}

//**************************************************************************
void KPrintBMB(int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs) {

	int	oldstretchs, oldstretchd;
	PBMB	bmP;

	if( bmS==0 ) {
		bmP = KWnds[0].pBM;
	} else {
		bmP = CreateBMB((WORD)ws, (WORD)hs, NULL);
		BltBMB(bmP, 0, 0, bmS, xs, ys, ws, hs, (int)FALSE);
		xs = ys = 0;
	}

	oldBMs = SelectObject(sdc, bmP->hBitmap);

	oldstretchs = SetStretchBltMode(sdc, STRETCH_DELETESCANS);
	oldstretchd = SetStretchBltMode(ddc, STRETCH_DELETESCANS);

	StretchBlt((HDC)hPRT, xd, yd, wd, hd, sdc, xs, ys, ws, hs, SRCCOPY);

	SetStretchBltMode(sdc, oldstretchs);
	SetStretchBltMode(ddc, oldstretchd);
	SelectObject(sdc, oldBMs);
	if( bmP!=KWnds[0].pBM ) DeleteBMB(bmP);
}

//**************************************************************************
void KPrintRect(int xd, int yd, int wd, int hd, DWORD fillcolor, DWORD edgecolor) {

	HPEN	p, op;
	POINT	lp[5];
	int		x, y;

	if( fillcolor!=-1 && edgecolor!=-1 ) {
		p = CreatePen(PS_SOLID, 1, (COLORREF)edgecolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);

		lp[0].x = lp[3].x = lp[4].x = xd;
		lp[0].y = lp[1].y = lp[4].y = yd;
		lp[1].x = lp[2].x = xd+wd-1;
		lp[2].y = lp[3].y = yd+hd-1;
		Polyline((HDC)hPRT, lp, 5);

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);

		++xd;
		++yd;
		--wd;--wd;
		--hd;--hd;

		p = CreatePen(PS_SOLID, 1, (COLORREF)fillcolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);

		if( wd>hd ) {
			lp[0].x = xd;
			lp[1].x = xd+wd-1;
			for(y=yd; hd; ++y, --hd) {
				lp[0].y = lp[1].y = y;
				Polyline((HDC)hPRT, lp, 2);
			}
		} else {
			lp[0].y = yd;
			lp[1].y = yd+hd-1;
			for(x=xd; wd; ++x, --wd) {
				lp[0].x = lp[1].x = x;
				Polyline((HDC)hPRT, lp, 2);
			}
		}

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);
	} else if( fillcolor==-1 ) {
		p = CreatePen(PS_SOLID, 1, (COLORREF)edgecolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);

		lp[0].x = lp[3].x = lp[4].x = xd;
		lp[0].y = lp[1].y = lp[4].y = yd;
		lp[1].x = lp[2].x = xd+wd-1;
		lp[2].y = lp[3].y = yd+hd-1;
		Polyline((HDC)hPRT, lp, 5);

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);
	} else {
		p = CreatePen(PS_SOLID, 1, (COLORREF)fillcolor);
		op = (HPEN)SelectObject((HDC)hPRT, p);

		if( wd>hd ) {
			lp[0].x = xd;
			lp[1].x = xd+wd-1;
			for(y=yd; hd; ++y, --hd) {
				lp[0].y = lp[1].y = y;
				Polyline((HDC)hPRT, lp, 2);
			}
		} else {
			lp[0].y = yd;
			lp[1].y = yd+hd-1;
			for(x=xd; wd; ++x, --wd) {
				lp[0].x = lp[1].x = x;
				Polyline((HDC)hPRT, lp, 2);
			}
		}

		SelectObject((HDC)hPRT, op);
		DeleteObject(p);
	}
}

//************************************************************************************
void KPrintFont(WORD hlim, WORD flags, WORD *h, WORD *asc) {

	int			logpixy;
	LOGFONT		sFont;
	HFONT		hOldFont;
	TEXTMETRIC	tm;

	if( hPRTFont ) DeleteObject(hPRTFont);

	logpixy = GetDeviceCaps((HDC)hPRT, LOGPIXELSY);
	sFont.lfWidth = 0;
	sFont.lfEscapement = sFont.lfOrientation = 0;
	sFont.lfWeight = (flags & 2)?FW_BOLD:FW_NORMAL;
	sFont.lfItalic = (flags & 4)?TRUE:FALSE;
	sFont.lfUnderline = FALSE;
	sFont.lfStrikeOut = FALSE;
	sFont.lfCharSet = ANSI_CHARSET;
	sFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	sFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	sFont.lfQuality = 2;//PROOF_QUALITY;
	if( flags & 1 ) {
		sFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
		strcpy(sFont.lfFaceName, "Courier New");
	} else {
		sFont.lfPitchAndFamily = VARIABLE_PITCH | FF_MODERN;
		strcpy(sFont.lfFaceName, "Tahoma");
	}
	sFont.lfHeight = -(hlim*logpixy/72);
	hPRTFont = CreateFontIndirect(&sFont);

	hOldFont = SelectObject((HDC)hPRT, hPRTFont);
	GetTextMetrics((HDC)hPRT, &tm);
	SelectObject((HDC)hPRT, hOldFont);
	*h = (WORD)tm.tmHeight;
	*asc = (WORD)tm.tmAscent;
}

//*********************************************************
void KPrintLines(DWORD *pMem, WORD numpoints, DWORD clr) {

	HPEN	hPen, holdPen;
	int		i, *pI;

	hPen = CreatePen(PS_SOLID, 1, clr);
	holdPen = SelectObject((HDC)hPRT, hPen);

	if( NULL!=(pI=(int *)malloc(2*numpoints*sizeof(int))) ) {
		for(i=0; i<(int)numpoints; i++) {
			pI[2*i] = (int)(long)(*pMem++);
			pI[2*i+1] = (int)(long)(*pMem++);
		}
		Polyline((HDC)hPRT, (POINT *)pI, (int)numpoints);
		free(pI);
	}
	SelectObject((HDC)hPRT, holdPen);
	DeleteObject(hPen);
}

//****************************************************************
void KCaptureMouse(BOOL cap) {

	if( cap ) SetCapture(hWndMain);
	else ReleaseCapture();
}

LRESULT CALLBACK KWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

///********************************************************************
/// Create a sub-window, return a handle from 0 to (MAXKWND-1)
/// else return -1 for failure.
long KCreateWnd(long x, long y, long w, long h, char *title, LRESULT CALLBACK (*wndProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam), BOOL closable) {

	long	i;
	DWORD	style;
	RECT	rc;

	GetWindowRect(KWnds[0].hWnd, &rc);
	y -= KWnds[0].y-rc.top;

	//look for unused handle
	for(i=0; i<KWndCnt; i++) if( KWnds[i].hWnd==NULL ) break;
	if( i>=MAXKWND ) return -1;	// if no available handles

	// create BMB for this window
	KWnds[i].pBM = CreateBMB((WORD)PhysicalScreenW, (WORD)PhysicalScreenH, (HWND)-1);// dummy non-NULL HWND, will get fixed up after window creation
	if( NULL==KWnds[i].pBM ) {
		sprintf((char*)ScratBuf, "Failed creating BITMAP for sub-window %s", title);
		SystemMessage((char*)ScratBuf);
		return -1;
	}
	// initialize the ENTIRE bitmap to black
	RectBMB(KWnds[i].pBM, 0, 0, KWnds[i].pBM->physW, KWnds[i].pBM->physH, 0x00000000, -1);

	KWnds[i].etc = NULL;	// make sure it's initialized
	KWnds[i].x = x;
	KWnds[i].y = y;
	KWnds[i].w = w;
	KWnds[i].h = h;
	if( i ) KWnds[i].misc = KWnds[0].misc;
	KWnds[i].proc = wndProc;
	strncpy(KWnds[i].title, title, 95);
	KWnds[i].title[95] = 0;	// make sure nul-terminated if src was longer than 95 chars

	if( i>=KWndCnt ) ++KWndCnt;	// if room available but at end, bump end count

	// create window and return index
	/* WS_OVERLAPPEDWINDOW = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX */
	style = WS_OVERLAPPED | WS_CAPTION | WS_THICKFRAME /*| WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX*/;
	if( closable ) style |= WS_SYSMENU;
	KWnds[i].hWnd = KWnds[i].pBM->hWnd = CreateWindow(KWinName, title, style, x, y, w, h, hWndMain, NULL, ghInstance, NULL);

	GetClientRect(KWnds[i].hWnd, &rc);
	KWnds[i].vw = rc.right-rc.left;
	KWnds[i].vh = rc.bottom-rc.top;

	SendMessage(KWnds[i].hWnd, UM_CREATED, 0, 0);	// so KWnd[] can do whatever initialization it needs

	return i;
}

//********************************************************************
// return -1 if didn't find hWnd
long KFindWnd(HWND hWnd) {

	long	i;

	for(i=0; i<KWndCnt; i++) if( KWnds[i].hWnd==hWnd ) return i;
	return -1;
}

//********************************************************************
// Destroy a sub-window.
void KDestroyWnd(long i) {

	if( i<KWndCnt && KWnds[i].hWnd!=NULL ) {
		DestroyWindow(KWnds[i].hWnd);
		KWnds[i].hWnd = NULL;
		// now trash any "end of list" unused structures
		for(i=KWndCnt-1; i>=0; i--) if( KWnds[i].hWnd ) break;
		KWndCnt = i+1;
	}
}

///********************************************************************************
/// Generic KWnd message ("event") handling function
LRESULT CALLBACK KWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {

	long			k;
	int				cw, ch;
	RECT			rect, rw, rc;
	HBITMAP			holdbm;
	HDC				hdc, bdc;
	PAINTSTRUCT		ps;

	k = KFindWnd(hWnd);
	if( k==-1 ) return DefWindowProc(hWnd, msg, wParam, lParam);	// if couldn't find KWnds[] entry

	switch( msg ) {
	 case WM_CLOSE:
		if( KWnds[k].proc ) KWnds[k].proc(hWnd, msg, wParam, lParam);
	 	KDestroyWnd(k);
		break;
	 case WM_ERASEBKGND:
	// never erase background, because we always draw everything
		if( KWnds[k].proc ) return KWnds[k].proc(hWnd, msg, wParam, lParam);
		return TRUE;
	 case WM_PAINT:
		if( KWnds[k].proc==NULL || KWnds[k].proc(hWnd, msg, wParam, lParam) ) {
		// the window needs updating, repaint it from the kBM bitmap
			hdc = BeginPaint(hWnd, &ps);

			SetBkColor(hdc, PALETTERGB(0,255,192));
			SetTextColor(hdc, PALETTERGB(0,0,0));

			bdc=CreateCompatibleDC(hdc);
			holdbm = (HBITMAP)SelectObject(bdc, KWnds[k].pBM->hBitmap);

			if( NULLREGION !=GetClipBox(hdc, &rect) ) {
				BitBlt(hdc, rect.left, rect.top, rect.right-rect.left+1, rect.bottom-rect.top+1, bdc, rect.left, rect.top, SRCCOPY);
			}
			SelectObject(bdc, holdbm);
			DeleteDC(bdc);

			EndPaint(hWnd, &ps);
		}
		return 0;
	 case WM_MOVE:
		GetWindowRect(KWnds[k].hWnd, &rw);
		GetClientRect(KWnds[k].hWnd, &rc);
		cw = GetSystemMetrics(SM_CYSIZEFRAME);
		KWnds[k].x = (long)((short)LOWORD(lParam))-cw;
		ch = WINDOW_TITLEY;
		KWnds[k].y = (long)((short)HIWORD(lParam))-ch;

		if( KWnds[k].proc ) return KWnds[k].proc(hWnd, msg, wParam, lParam);
		break;
	 case WM_SIZE:
		GetWindowRect(KWnds[k].hWnd, &rw);
		GetClientRect(KWnds[k].hWnd, &rc);
		KWnds[k].w = rw.right-rw.left;
		KWnds[k].h = rw.bottom-rw.top;
		KWnds[k].vw = rc.right;
		KWnds[k].vh = rc.bottom;

		if( KWnds[k].proc ) return KWnds[k].proc(hWnd, msg, wParam, lParam);
		break;
	 default:
		if( KWnds[k].proc ) return KWnds[k].proc(hWnd, msg, wParam, lParam);
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

///**************************************
///**************************************
///**************************************
/// HELP WINDOW STUFF
///**************************************
///**************************************
///**************************************
HWND		hwndHelp=NULL;
long		helpON=0, helpX, helpY, helpW=-1, helpH;
HFONT		hHelpFont;
BYTE		*pHelpMem=NULL;
int			iHelpWnd;


//********************************************************************************
// This is the KWnd message ("event") handling function for the HELP window
LRESULT CALLBACK HelpWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {

	long			hK, f, s;

	hK = KFindWnd(hWnd);
	if( hK==-1 ) return DefWindowProc(hWnd, msg, wParam, lParam);	// if couldn't find KWnds[] entry

	switch( msg ) {
	 case WM_ERASEBKGND:
		return 0;
	 case WM_SHOWWINDOW:
	 	if( pHelpMem==NULL ) {
			// read HELP.TXT
			if( -1!=(f=Kopen("docs\\readme.txt", O_RDBIN)) ) {
				s = Kseek(f, 0, SEEK_END);
				if( NULL != (pHelpMem = (BYTE *)malloc(s+1)) ) {	// one extra for NUL on last line in case no CR/LF on last line
					Kseek(f, 0, SEEK_SET);
					Kread(f, pHelpMem, s);
					Kclose(f);

					hwndHelp = CreateWindowEx(0, "EDIT",   // predefined class
									NULL,         // no window title
									WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
									ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,// | ES_READONLY,
									0, 0, 0, 0,   // set size in WM_SIZE message
									hWnd,         // parent window
									(HMENU)ID_EDITCHILD,   // edit control ID
									ghInstance,
									NULL);        // pointer not needed

					hHelpFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, NULL);
					SetWindowFont(hwndHelp, hHelpFont, FALSE);
					// Add text to the window.
					SendMessage(hwndHelp, WM_SETTEXT, 0, (LPARAM) pHelpMem);
				} else 	Kclose(f);
			}
	 	}
		break;
	 case WM_SIZE:
		// Make the edit control the size of the window's client area.
		helpX = KWnds[iHelpWnd].x;
		helpY = KWnds[iHelpWnd].y + WINDOW_TITLEY;
		helpW = KWnds[iHelpWnd].w;
		helpH = KWnds[iHelpWnd].h;
		MoveWindow(hwndHelp, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
		return 0;
	 case WM_MOVE:
		// Make the edit control the size of the window's client area.
		helpX = KWnds[iHelpWnd].x;
		helpY = KWnds[iHelpWnd].y + WINDOW_TITLEY;
		helpW = KWnds[iHelpWnd].w;
		helpH = KWnds[iHelpWnd].h;
		return 0;
	 case WM_CLOSE:
	 	free(pHelpMem);
	 	pHelpMem = NULL;
	 	DestroyWindow(hwndHelp);
	 	hwndHelp = NULL;
		DeleteObject(hHelpFont);
		return 0;
	 case WM_SYSKEYDOWN:
	 case WM_SYSKEYUP:
	 case WM_KEYDOWN:
	 case WM_KEYUP:
	 case WM_CHAR:
		SetFocus(hWndMain);
		return SendMessage(hWndMain, msg, wParam, lParam);
	 default:
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

///**************************************
///**************************************
///**************************************
/// 82901 DISK WINDOW STUFF
///**************************************
///**************************************
///**************************************
void DeleteDiskList(int iDiskWnd, int drive) {

	DISKETC	*e;

	e = (DISKETC*)KWnds[iDiskWnd].etc;

	ListBox_ResetContent(drive?e->hDiskList[1]:e->hDiskList[0]);
}

int tabsets[]={4*12, 4*18, 4*24};

//**************************************
void AddDiskList(int iDiskWnd, int drive, char *fname) {

	HWND	hwnd;
	HDC		hdc;
	int		dx;
	int		i;
	SIZE	sz;
	char	tb[256];
	DISKETC	*e;

	e = (DISKETC*)KWnds[iDiskWnd].etc;

	hwnd = drive?e->hDiskList[1]:e->hDiskList[0];
	ListBox_AddString(hwnd, fname);

	hdc = GetDC(hwnd);
	// Windows "dialog units" to "pixels" concept is terminally sick
	// and since we're using a fixed point font instead of Tahoma or something,
	// we have to just wing it here.  72% of the string length SEEMS to work
	// out about right for triggering the horizontal scroll bars at the right point.
	dx = strlen(fname);
	for(i=0; i<dx; i++) tb[i] = 'M';
	tb[i] = 0;
//	dx = GetDialogBaseUnits() & 0x0FFFF;
//	dx *= (GetTabbedTextExtent(hdc, fname, dx, 3, tabsets) & 0x0FFFF);
	GetTextExtentPoint32(hdc, tb, dx, &sz);
	ReleaseDC(hwnd, hdc);
	dx = 72*sz.cx/100;

	if( ListBox_GetHorizontalExtent(hwnd)<dx ) ListBox_SetHorizontalExtent(hwnd, dx);
}
///***********************************************
void AdjustDiskLists(int iDiskWnd) {

	RECT	r;
	int		i;
	DISKETC	*e;

	e = (DISKETC*)KWnds[iDiskWnd].etc;

	RelocateDisks(e->sc, e->tla, 0);
	for(i=0; i<2; i++) {
		GetDiskListRect(e->sc, e->tla, i, &r);
		if( r.left==r.right ) ShowWindow(e->hDiskList[i], SW_HIDE);
		else {
			ShowWindow(e->hDiskList[i], SW_SHOW);
			MoveWindow(e->hDiskList[i], r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
		}
	}
}

/*
typedef struct {
	HWND	hDiskList[2];
	long	sc;
	HFONT	hDiskFont;
} DISKETC;
*/

//********************************************************************************
// This is the KWnd message ("event") handling function for the DISK window
LRESULT CALLBACK DiskWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam ) {

	int		u, sel;
	long	k;
	RECT	r;
	DISKETC	*e;

	k = KFindWnd(hWnd);
	if( k==-1 ) return DefWindowProc(hWnd, msg, wParam, lParam);	// if couldn't find KWnds[] entry
	e = (DISKETC*)KWnds[k].etc;

	switch( msg ) {
	 case UM_CREATED:
	 	KWnds[k].etc = malloc(sizeof(DISKETC));
	 	((DISKETC*)(KWnds[k].etc))->hDiskList[0] = ((DISKETC*)(KWnds[k].etc))->hDiskList[1] = NULL;
		return 0;
	 case WM_SHOWWINDOW:
	 	RelocateDisks(e->sc, e->tla, 0);
		GetDiskListRect(e->sc, e->tla, 0, &r);
	 	if( e->hDiskList[0]==NULL ) {
			e->hDiskList[0] = CreateWindow("listbox",
				 NULL,
				 WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | LBS_USETABSTOPS | LBS_NOTIFY,
				 r.left, r.top,
				 r.right - r.left,
				 r.bottom - r.top,
				 hWnd,
				 (HMENU)(ID_SC_BASE + e->sc*100+0),	// ID for auto-cat list 0
				 ghInstance,
				 NULL);
			e->hDiskFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, NULL);
			ListBox_SetTabStops(e->hDiskList[0], 3, tabsets);
			SetWindowFont(e->hDiskList[0], e->hDiskFont, FALSE);
	 	} else MoveWindow(e->hDiskList[0], r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
		GetDiskListRect(e->sc, e->tla, 1, &r);
		if( e->hDiskList[1]==NULL ) {
			e->hDiskList[1] = CreateWindow("listbox",
				 NULL,
				 WS_CHILD | WS_VISIBLE | WS_BORDER | WS_HSCROLL | WS_VSCROLL | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | LBS_USETABSTOPS | LBS_NOTIFY,
				 r.left, r.top,
				 r.right - r.left,
				 r.bottom - r.top,
				 hWnd,
				 (HMENU)(ID_SC_BASE + e->sc*100+1),	// ID for auto-cat list 1
				 ghInstance,
				 NULL);
			ListBox_SetTabStops(e->hDiskList[1], 3, tabsets);
			SetWindowFont(e->hDiskList[1], e->hDiskFont, FALSE);
		} else MoveWindow(e->hDiskList[1], r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
	 	break;
	 case WM_COMMAND:
		if( HIWORD(wParam)==LBN_DBLCLK ) {
			u = LOWORD(wParam);
			if( u==ID_SC_BASE+e->sc*100 || u==ID_SC_BASE+e->sc*100+1 ) {				// unit 0
				sel = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0);
				if( LB_ERR != SendMessage((HWND)lParam, LB_GETTEXT, sel, (LPARAM)ScratBuf) ) {
					if( !strncmp((char*)ScratBuf+11, "PROG", 4) ) {
						KeyMemLen = 256;
						if( NULL != (pKeyMem = (BYTE *)malloc(KeyMemLen)) ) {
							u = (u-ID_SC_BASE)%100;
							sprintf((char*)pKeyMem, "LOAD \"%10.10s:D%d%d%d\"\r", ScratBuf, (int)(3+e->sc), 0, u);
							pKeyPtr = pKeyMem;
						}
					} else if( !strncmp((char*)ScratBuf+11, "BPGM", 4) ) {
						KeyMemLen = 256;
						if( NULL != (pKeyMem = (BYTE *)malloc(KeyMemLen)) ) {
							u = (u-ID_SC_BASE)%100;
							sprintf((char*)pKeyMem, "LOADBIN \"%10.10s:D%d%d%d\"\r", ScratBuf, (int)(3+e->sc), 0, u);
							pKeyPtr = pKeyMem;
						}
					}
				}
			}
		}
		break;
	 case WM_LBUTTONUP:
		SetFocus(hWndMain);
		DiskMouseClick(e->sc, e->tla, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
		return 0;
	 case WM_PAINT:
		// NOTE: *MUST* return non-zero for WM_PAINT unless we do the painting, so that KWndProc will do the real WM_PAINT
		return 1;
	 case WM_SIZE:
		AdjustDisks(e->sc, e->tla);
		DrawDisks(e->sc, e->tla);
		return 0;
	 case WM_CLOSE:
		DeleteObject(e->hDiskFont);
		break;
	 case WM_SYSKEYDOWN:
	 case WM_SYSKEYUP:
	 case WM_KEYDOWN:
	 case WM_KEYUP:
	 case WM_CHAR:
		SetFocus(hWndMain);
		return SendMessage(hWndMain, msg, wParam, lParam);
	 default:
		break;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

//***************
//* END OF FILE *
//***************
#endif
