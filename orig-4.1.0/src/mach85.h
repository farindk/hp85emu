/*******************************************
 MACH85.H - HP-85 machine emulation header
	Copyright 2003-17 Everett Kaser
	All rights reserved.
 See HP85EM.TXT for legal usage.
 *******************************************/

#include	"support.h"
#include	"dlg.h"

#define	MAXROMS		20

// MAX_DISKS controls the maximum # of different diskettes (in a single folder) the emulator
// is willing to deal with.  If this is increased, the hacked together DSKD_ dialog
// will need to be modified.  It really should be a scroll-able list box.  Someday.
#define	MAX_FOLDERS	10
#define	MAX_DISKS	30

// MAX_TAPES controls the maximum # of different tape cartridges the emulator
// is willing to deal with.  If this is increased, the hacked together TAPD_ dialog
// will need to be modified.  It really should be a scroll-able list box.  Someday.
#define	MAX_TAPES	30

#define	HP85ADDR(n)		(((n)>>16)&0x0000FFFF)
#define	GETREGW(n)		((WORD)Reg[n] | (((WORD)Reg[n+1])<<8))
#define	GETLASTREGW(n)	((WORD)LastReg[n] | (((WORD)LastReg[n+1])<<8))
#define SETREGW(n, w)	{\
			Reg[n] = (BYTE)((w) & 0x00FF);\
			Reg[(n)+1]=(BYTE)(((w)>>8) & 0x00FF);\
		}
#define	GETROMW(a)		((WORD)((WORD)LoadMem(a)+((WORD)LoadMem((WORD)((a)+1))<<8)))
#define SETROMW(a, w)	{\
			StoreMem((WORD)(a), (BYTE)((w) & 0x00FF));\
			StoreMem((WORD)((a)+1), (BYTE)(((w)>>8) & 0x00FF));\
		}

#define	PTH		12

//***********************
// CfgFlags
#define	CFG_BIGCRT			0x00000001
#define	CFG_REOPEN_DLG		0x00000002
#define	CFG_AUTORUN			0x00000004
#define	CFG_NATURAL_SPEED	0x00000008
#define	CFG_3INCH_DRIVE		0x00000010

// HP-85 speed adjusting
#define	CYCLESPERSEC	613062
#define	CYCLES60		(CYCLESPERSEC/60)
#define	CYCLESCLK		(CYCLESPERSEC/888)
#define	CYCLESCLKBUSY	(CYCLESCLK/4)

// INTERRUPT flags
extern WORD	Int85;
#define	INT_PWO	1
#define	INT_SP0	2
#define	INT_KEY	4
#define	INT_CL0	16
#define	INT_CL1	32
#define	INT_CL2	64
#define	INT_CL3	128
#define	INT_IO	256
#define	INT_SP1	512

///*************************************
/// NOTE: While we use MAX_DEVICES in
/// many places, the DEVICES dialog is
/// designed for a maximum of 32 devices.
/// So if MAX_DEVICES is ever changed,
/// then the DEVICES dialog would need
/// to be changed as well.
///*************************************
#define	MAX_DEVICES	32
extern DEV	Devices[MAX_DEVICES];
extern int (*devProcs[])(long reason, int sc, int tla, int d, int val);

//**********************
// Disassembly stuff
#define	REC_COD	0
#define	REC_BYT	1
#define	REC_ASC	2
#define	REC_ASP	3
#define	REC_BSZ	4
#define	REC_DEF	5
#define	REC_VAL	6
#define	REC_RAM	7
#define	REC_I_O	8
#define	REC_DRP	9
#define	REC_REM	10

struct codrec {
	BYTE		type;
	BYTE		label[9];
	WORD		datover;	// over ride of how many data bytes there are for disassembly
	DWORD		address;
		// Note: the 'address' field contains the actual HP-85 address shifted over 8 bits.
		// If the lower 16 bits is 0xFFFF, then this record applies to real bytes in the HP-85.
		// Otherwise (lower 16 bits == 0x0000 to 0xFFFE), this record merely contains a full-line
		// comment that precedes the actual HP-85 bytes.  It's done this way for ease of
		// sorting/finding/positioning full-line comments and HP-85 addresses relative to
		// each other.  This allows up to 65535 lines of comments before any one line of HP-85
		// code.  Should be enough.
	char		*Comment;
	struct codrec		*pNext;
	struct codrec		*pPrev;
};

typedef struct codrec DISASMREC;

#define	FindRecNext(f)	(f->pNext)
#define	FindRecPrev(f)	(f->pPrev)

DISASMREC *AddRec(BYTE typ, char *lbl, DWORD addr, char *rem, WORD dostate);
DISASMREC *DeleteRec(DISASMREC *pRec);
DISASMREC	*FindRecAddress(DWORD addr, DWORD optrom);

//**************************************************
// Event-handler functions
void HP85OnStartup();
void HP85OnShutdown();
void HP85OnTimer();
void HP85OnIdle();
BOOL HP85OnChar(DWORD c);
BOOL HP85OnKeyDown(DWORD key);
BOOL HP85OnKeyUp(DWORD key);
void HP85OnLButtDown(DWORD x, DWORD y, DWORD flags);
void HP85OnLButtUp(long x, long y, DWORD flags);
void HP85OnMButtDown(DWORD x, DWORD y, DWORD flags);
void HP85OnMButtUp(DWORD x, DWORD y, DWORD flags);
void HP85OnRButtDown(DWORD x, DWORD y, DWORD flags);
void HP85OnRButtUp(DWORD x, DWORD y, DWORD flags);
void HP85OnResize();
void HP85OnMouseMove(long x, long y, DWORD flags);
void HP85OnWheel(long x, long y, long count);

//********************************************
// misc utility functions
void StoreDisAsmRecs();
void LoadDisAsmRecs();
void Exec85(WORD);
void HP85InvalidateCRT();
void DrawDisAsm(PBMB hBM);
void DrawAll();
void DrawDebug();
void DrawTape();
void DownCursor();
void HP85PWO(BOOL first);
BOOL SetupDispVars();
void DrawPrinter();
void SelectROM(DWORD romnum);
void ScrollUp(DWORD n);
void NewTape();
void EjectTape();
void WriteTape();
BOOL LoadTape();
BYTE LoadMem(WORD addr);
void StoreMem(WORD addr, BYTE val);
void HP85InvalidateCRTbyte();
void HP85InvalidateCRT();
void DrawTapeStatus();
void DrawTape();
void PaperAdv(long advH, BOOL draw);
void Print85(PBMB hBM, int x, int y, BYTE *str, DWORD len, DWORD clr);
void PrintGraphics(PBMB hBM, int x, int y, BYTE *buf, DWORD len, DWORD clr);
BOOL PPromPresent();
void RequestHPIBreset(int skipcnt);

//********************************************
// globals
extern BYTE		HP85font[135][8];
extern int		HP87font[128];
extern DWORD	LineIndex[1024];
extern DWORD	CursLine;
extern DWORD	base;
extern DISASMREC *LineRecs[1024], *CurRec;
//extern BYTE		ROM[65536];
extern BYTE		*ROM;
extern DWORD	DisStart;
extern DWORD	Model;
extern DWORD	RomLoad, RomLoads[3], RomMUST[3], RomCANT[3];
extern DWORD	RomList[];
extern DWORD	RomListCnt;
extern BOOL		CtlDown, ShiftDown;
extern DWORD	CfgFlags;
extern DWORD	RamSize, RamSizes[3], RamSizeExt[3];
extern DWORD	TraceLines;
extern BYTE		E;
extern WORD		Arp, Drp;
extern BOOL		Halt85;
extern char		CurTape[64];
extern BYTE		IO_TAPSTS, IO_TAPCART;
extern char		FilePath[PATHLEN];
extern DWORD	Cycles, SecondsCycles;
extern PBMB		hp85BM;
extern DWORD	MainWndClr, MainWndTextClr, MenuSelBackClr, MenuSelTextClr, MenuBackClr, MenuTextClr, MenuHiClr, MenuLoClr, PrintClr;
extern long		KeyMemLen;
extern BYTE		*pKeyMem, *pKeyPtr;
extern BYTE		ScratBuf[8192];
extern PBMB		keyBM, tapeBM, disk525BM, disk35BM, disk8BM, disk5MBBM, prtBM, rlBM, hp85BM;
extern int		BRTx, BRTy, BRTw, BRTh;
extern int		RLx, RLy, RLw, RLh;
extern int		CRTx, CRTy, KEYx, KEYy, TAPEx, TAPEy;
extern int		PRTx, PRTy, PRTw, PRTh, PRTlen, PRTscroll;
extern BYTE		Reg[64], LastReg[64];
extern DWORD	Ptr1, Ptr2, Ptr1Cnt, Ptr2Cnt;
extern BYTE		(*ioProc[256])(WORD address, long val);
extern BYTE		IO_SC;			// interrupting SC
extern DWORD	IO_PendingInt;	// one bit per SC if wants to interrupt, checked during Exec85() loop
extern BOOL		IOcardsIntEnabled;
extern int		MenuDiskSC, MenuDiskTLA, MenuDiskDrive;
