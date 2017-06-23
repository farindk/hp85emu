/*******************************************
 SUPPORT.H - Graphics, etc, routines
	Copyright 2003-17 Everett Kaser
	All rights reserved.
 See HP85EM.TXT for legal usage.
 *******************************************/

#ifndef SUPPORT_INCLUDED
#define SUPPORT_INCLUDED

#include "config.h"


#if TODO

#define	DEBUG		0

#if DEBUG
void WriteDebug(BYTE *p);
void DebLog(BYTE *p);
#endif // DEBUG

#endif // TODO

#define	CLR_BLACK		0x00000000
#define	CLR_WHITE		0x00FFFFFF
#define	CLR_NADA		0xFFFFFFFF
#define	CLR_YELLOW		0x0000FFFF
#define	CLR_RED			0x000000FF
#define	CLR_LIGHTRED	0x00C0C0FF
#define	CLR_GREEN		0x0000FF00
#define	CLR_LIGHTGREEN	0x00C0FFC0
#define	CLR_BLUE		0x00FF0000
#define	CLR_LIGHTBLUE	0x00FFC0C0
#define	CLR_ORANGE		0x000078FF
#define	CLR_GRAY		0x00808080

#if TODO

#define	CLR_MAINBACK	0x00AEBEBD
#define	CLR_MAINTEXT	0x00000000

#define	CLR_TAPSTS_WE	0x00E0FFE0
#define	CLR_TAPSTS_WP	0x00E0E0FF
#define	CLR_TAPSTS_RD	0x0000FF00
#define	CLR_TAPSTS_WR	0x000000FF
#define	CLR_TAPSTS_HI	0x00FF0000

#define	CLR_REGBACK		0x00000000
#define	CLR_REGFRAME	0x00FFFFFF
#define	CLR_REGNAMES	0x0000FFFF
#define	CLR_REGCHANGE	0x000078FF
#define	CLR_REGNOCHANGE	0x00FFFFFF

#define	CLR_DLGBACK		0x009098A8
#define	CLR_DLGHI		0x00C8D0D8
#define	CLR_DLGLO		0x00586078
#define	CLR_SELBACK		0x007B7800
#define	CLR_GRAYTEXT1	0x00C8D0D8
#define	CLR_GRAYTEXT2	0x00586078
#define	CLR_DLGTEXT		0x00000000
#define	CLR_SELTEXT		0x00E7E7E7
#define	CLR_EDITBACK	0x00C8C0D8
#define	CLR_EDITTEXT	0x00000000
#define	CLR_EDITCURSOR	0x00BF2B3F

#define	CLR_MENUSELBACK	0x00808000
#define	CLR_MENUSELTEXT	0x00000000
#define	CLR_MENUBACK	0x00A41C06
#define	CLR_MENUTEXT	0x00FFFFFF
#define	CLR_MENUHI		0x00FFC0B0
#define	CLR_MENULO		0x00200000

#define	CLR_PRINT		0x00FF0000

#define	CLR_MARK		0x00FF8080

#define	CLR_DNKEYBACK	0x00404040

//*****************************
//* function prototypes
//*****************************
void SupportStartup();
void SupportShutdown();

// graphic function prototypes
void BltBMB(PBMB bmD, int xd, int yd, PBMB bmS, int xs, int ys, int ws, int hs, int masked);
void StretchBMB(PBMB bmD, int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs);
void LineBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD clr);
void RectBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD fill, DWORD edge);
void PointBMB(PBMB bmD, int x, int y, DWORD clr);
PBMB LoadBMB(char *fname, int transX1, int transY1, int transX2, int transY2);
void Label85BMB(PBMB hBM, int x, int y, int rowH, BYTE *str, int len, DWORD clr);
void ClipBMB(PBMB hBM, int x, int y, int w, int h);
void DrawHiLoBox(PBMB hBM, long x, long y, long w, long h, DWORD clrhi, DWORD clrlo);

extern PBMB		pbFirst;

#if 0
// CFG writing and reading
BOOL ssWBegin();
BOOL ssRBegin();
void ssEnd();

void ssWBlockBegin(char *str);
void ssWBlockEnd();
void ssWValuesBegin();
void ssWValuesEnd();
void ssWValueDec(long val);
void ssWValueOct(long val);
void ssWValueHex(DWORD val);
void ssWValueStr(char *str);

char *ssRBlockBegin();		// Chew up and return ptr to NAME from {NAME
long ssRBlockBeginIni();	// Chew up and return index into IniStr[] of NAME string matching {NAME
BOOL ssRIsEnd();			// Is "end of file"?
BOOL ssRIsEndBlock();		// Returns TRUE if '}' is next char
BOOL ssRBlockEnd();			// Chew up }
BOOL ssRValuesBegin();		// Chew up {
BOOL ssRIsValue();			// return TRUE/FALSE whether there's a number or string coming up next
long ssRValueStrIni();		// Chew up "STRING" and return 0 or index into IniStr[] of matching STRING, similar to ssRBlockBeginIni() but expects "STRING" rather than {STRING
BOOL ssRValuesEnd();		// Chew up }
BOOL ssRValueNum(long *pNum);// Chew up and return numeric value (dec, oct, or hex)
char *ssRValueStr();		// Chew up "STRING" and return pointer to STRING
void ReadIniError();		// report an error processing the INI file
#endif // 0

extern BYTE	*iniSTR[];
extern long	iniSTRcnt, iniREV;

#define	INIS_NOGOOD		0
#define	INIS_INI		1
#define	INIS_REV		2
#define	INIS_EMULATOR	3
#define	INIS_WINDOW		4
#define	INIS_MODEL		5
#define	INIS_HP85A		6	// the code assumes that 85A, 85B, and 87 are in this order, sequentially
#define	INIS_HP85B		7
#define	INIS_HP87		8
#define	INIS_FLAGS		9
#define	INIS_HELP		10
//#define	INIS_		11
//#define	INIS_	12
//#define	INIS_	13
#define	INIS_ROMS		14
#define	INIS_RAM		15
#define	INIS_EXTRAM		16
#define	INIS_BASE		17
#define	INIS_MARKS		18
#define	INIS_BRKPTS		19
#define	INIS_BRIGHT		20
#define	INIS_COLORS		21
#define	INIS_CARDS		22
#define	INIS_DISK525	23
#define	INIS_DISK35		24
#define	INIS_DISK8		25
#define	INIS_DISK5MB	26

#define	INIS_UNIT0		28
#define	INIS_UNIT1		29
#define	INIS_TAPE		30
#define	INIS_FILEPRT	31
#define	INIS_HARDPRT	32
#define	INIS_HPIB		33
#define	INIS_DEV		34

long iniRead(BYTE *fname);
long iniWrite(BYTE *fname);
void iniClose();
long iniFindSec(BYTE *str);
long iniFindVar(BYTE *str);
long iniGetNum(DWORD *var);
long iniGetStr(BYTE *var, WORD maxlen);
BOOL iniEndVar();
long iniSetSec(BYTE *str);
long iniSetVar(BYTE *var, BYTE *str);
void iniDelSec();
long iniDelVar(BYTE *varname);
long iniNextSec(BYTE *var, WORD maxlen);
void iniDelAll();
BOOL iniBadSum();
long iniAddVarNum(BYTE *var, DWORD num, DWORD form);
long iniAddVarStr(BYTE *var, BYTE *str);

#endif

#endif
