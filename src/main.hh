/*******************************************
 MAIN.H - Header file for MAIN.C
	Copyright 2006-2017 Everett Kaser
	All rights reserved.
 See HP85EM.TXT for legal usage.
 *******************************************/

#define	UM_GETPATH	(WM_USER+0)

#define	KINT_WND_SIZEW	760
#define	KINT_WND_SIZEH	600

#include "config.h"
#include <stdio.h>
#include <string>

#include "bitmap.h"


class Environment
{
public:
  virtual ~Environment() { }

  virtual std::string getEmulatorFilesPath() const = 0;
  virtual FILE* openEmulatorFile(std::string path) const = 0;

  virtual uint32_t getTicks() const = 0;
};


class Environment_Linux : public Environment
{
public:
  virtual std::string getEmulatorFilesPath() const;
  virtual FILE* openEmulatorFile(std::string path) const;

  virtual uint32_t getTicks() const;
};


const Environment* getEnvironment();


// TODO: this should only be a temporary hack until we get rid of global variables
class HPMachine* getMachine();



class EmulatorUI
{
public:
  virtual Bitmap* createCRTBitmap(int w,int h) const = 0;

  virtual void connectToHPMachine(HPMachine*) = 0;
  virtual void disconnectFromHPMachine(HPMachine*) = 0;
};


class EmulatorUI_Qt : public EmulatorUI
{
public:
  EmulatorUI_Qt();

  virtual Bitmap* createCRTBitmap(int w,int h) const;

  virtual void connectToHPMachine(HPMachine*);
  virtual void disconnectFromHPMachine(HPMachine*);

  //void create();
  void runIdle();

private:
  class MainWindow* mMainWindow;

  HPMachine* mMachine;
};


class EmulatorUI* getUI();


#if TODO
#include <windows.h>
#include <windowsx.h>	// for GET_X_LPARAM and GET_Y_LPARAM that get SIGNED values for LOWORD(lParam) and HIWORD(lParam)

#define	Kmin(a, b)	min(a, b)
#define	Kmax(a, b)	max(a, b)

typedef struct {
	BYTE		*pBits;
	BYTE		*pAND;
	WORD		linelen, physW, physH, logW, logH;
	int			clipX1, clipX2, clipY1, clipY2;
	HBITMAP		hBitmap;
	HWND		hWnd;
	void		*pPrev, *pNext;
} BMB, *PBMB;


#define	MAXKWND	100
#define	pKPBMB(k)	(KWnds[k].pBM)

typedef struct {
	HWND	hWnd;
	PBMB	pBM;
	long	x, y, w, h, vw, vh, misc;
	// (w,h) is FULL window (the outside size), (vw,vh) is the VIEWING area of the window (the inside size)
	// misc is the window-position 'mode' argument in MS Windows

	LRESULT CALLBACK (*proc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	char	title[96];
	void	*etc;	// pointer to whatever you want, a variable, a structure, the stars...
} KWND;

#define	UM_CREATED	(WM_USER+1)

extern KWND KWnds[MAXKWND];
extern long KWndCnt;

long KCreateWnd(long x, long y, long w, long h, char *title, LRESULT CALLBACK (*wndProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam), BOOL closable);
void KDestroyWnd(long i);

#define	WINDOW_FRAMEX	(GetSystemMetrics(SM_CXFRAME))
#define	WINDOW_FRAMEY	(GetSystemMetrics(SM_CYFRAME))
#define	WINDOW_TITLEY	(GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYSIZEFRAME))

// malloc one of these for each 82901 created and store the pointer to it into KWnds[].etc
typedef struct {
	HWND	hDiskList[2];
	int		sc, tla;
	HFONT	hDiskFont;
} DISKETC;

typedef struct {
	WORD	y, mo, d, h, mi, s;
} KTIME;

// The following are the file-open flag definitions.
// These are consistent with standard C library #define values
// but if this is being translated to another language, it's
// good to know what the code is going to be passing you and
// what it means....
#define	O_RDBIN		0x8000	// read only, binary
#define	O_WRBIN		0x8002	// read/write, binary
#define	O_WRBINNEW	0x8302	// read/write, binary, create or truncate

void InitScreen(long minW, long minH);
void KillProgram();
void KPlaySound(char *pMem);
void StartTimer(DWORD timval);
void StopTimer();
void SystemMessage(char *pMem);
int GetSystemEOL(BYTE *pMem);
void BuildPath(char *fname);

int Kopen(char *fname, int oflags);
int KopenAbs(char *fname, int oflags);
long Kseek(int fhandle, DWORD foffset, int origin);
void Kclose(int fhandle);
int Keof(int fhandle);
long Kread(int fhandle, BYTE *pMem, long numbytes);
long Kwrite(int fhandle, BYTE *pMem, long numbytes);
long Kfindfirst(char *fname, BYTE *fbuf, BYTE *attrib);
long Kfindnext(long ffhandle, BYTE *fbuf, BYTE *attrib);
void Kfindclose(long ffhandle);
void Kdelete(char *fname);
void Krename(char *oname, char *nname);
WORD KCopyFile(char *src, char *dst);
int KMakeFolder(char *fname);
void KDelFolder(char *fname);
long KFileSize(char *fname);

void KGetTime(KTIME *pT);
void KTimeToString(BYTE *pMem);
DWORD KGetTicks();
void KSetCursor(WORD curtype);
void KSetTitle(BYTE *pMem);

PBMB CreateBMB(WORD w, WORD h, HWND hWnd);
void DeleteBMB(PBMB pX);
void KInvalidate(PBMB pBM, int x1, int y1, int x2, int y2);
void KInvalidateAll();
void FlushScreen();

DWORD KPrintStart(DWORD *pw, DWORD *ph, DWORD *Xppi, DWORD *Yppi);
void KPrintStop();
BYTE KPrintBeginPage();
BYTE KPrintEndPage();
void KPrintText(int x, int y, char *str, DWORD clr);
void KPrintBMB(int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs);
void KPrintFont(WORD hlim, WORD flags, WORD *h, WORD *asc);
void KPrintLines(DWORD *pMem, WORD numpoints, DWORD clr);
void KPrintGetTextSize(char *str, DWORD *dw, DWORD *dh, DWORD *dasc);
void KPrintRect(int xd, int yd, int wd, int hd, DWORD fillcolor, DWORD edgecolor);
void KCaptureMouse(BOOL cap);

void KSetWindowPos(HWND wnd, long x, long y, long w, long h, DWORD sc);
void KGetWindowPos(HWND wnd, long *x, long *y, long *w, long *h, long *sc);
void KGetWndWH(long *w, long *h);

void DeleteDiskList(int iDiskWnd, int drive);
void AddDiskList(int iDiskWnd, int drive, char *fname);
void AdjustDiskLists(int iDiskWnd);

extern long		helpON, helpX, helpY, helpW, helpH;

extern char		PgmPath[PATHLEN], TmpPath[PATHLEN];
extern HWND		hWndMain;
extern int		PhysicalScreenW, PhysicalScreenH, PhysicalScreenSizeIndex;
extern int		CurCur;

extern BYTE		*pHelpMem;
extern int		iHelpWnd;

void SelectROM(DWORD romnum);
LRESULT CALLBACK KWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK HelpWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
LRESULT CALLBACK DiskWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
#endif
