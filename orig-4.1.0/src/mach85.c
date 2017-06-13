/*******************************************
 MACH85.C - HP-85 machine emulation
	Copyright 2003-17 Everett Kaser
	All rights reserved.
 See HP85EM.TXT for legal usage.
 *******************************************/

#include "main.h"
#include "mach85.h"
#include "beep.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>

//BOOL GeekDo=FALSE;

// Definitions for Series80.ini file
BYTE	*iniSTR[] = {
	(BYTE*)"",
	(BYTE*)"INI",
	(BYTE*)"rev",
	(BYTE*)"EMULATOR",
	(BYTE*)"window",
	(BYTE*)"model",
	(BYTE*)"HP85A",
	(BYTE*)"HP85B",
	(BYTE*)"HP87",
	(BYTE*)"flags",
	(BYTE*)"help",
	(BYTE*)"",
	(BYTE*)"",
	(BYTE*)"",
	(BYTE*)"ROMs",
	(BYTE*)"RAM",
	(BYTE*)"extRAM",
	(BYTE*)"base",
	(BYTE*)"marks",
	(BYTE*)"breakpoints",
	(BYTE*)"brightness",
	(BYTE*)"colors",
	(BYTE*)"IOcards",
	/// NOTE: ALL disk drives MUST be together here and in DEVTYPE_ order
	/// so that the 'name' can be indexed by iniSTR[INIS_DISK525+DiskParms[pD->devType]
	(BYTE*)"Diskette 5.25",
	(BYTE*)"Diskette 3.5",
	(BYTE*)"Diskette 8",
	(BYTE*)"Hard disk 5MB",
	(BYTE*)"",

	(BYTE*)"unit0",
	(BYTE*)"unit1",
	(BYTE*)"tape",
	(BYTE*)"FilePRT",
	(BYTE*)"HardPRT",
	(BYTE*)"HPIB",
	(BYTE*)"dev"
};
long	iniSTRcnt=sizeof(iniSTR)/sizeof(iniSTR[0]);
long	iniREV;
int		MenuDiskSC=8, MenuDiskTLA=8, MenuDiskDrive;

#define	CURINIREV	5

void StoreDis(char *fname, DISASMREC *pF, DISASMREC *pL, DWORD discnt);

char	ininame[]="Series80.ini";
char	CfgName[]="Series80.cfg";
char	*SysDir;	// location of ROM and .DIS files

DWORD	MainWndClr=CLR_MAINBACK;
DWORD	MainWndTextClr=CLR_MAINTEXT;
DWORD	MenuSelBackClr=CLR_MENUSELBACK;
DWORD	MenuSelTextClr=CLR_MENUSELTEXT;
DWORD	MenuBackClr=CLR_MENUBACK;
DWORD	MenuTextClr=CLR_MENUTEXT;
DWORD	MenuHiClr=CLR_MENUHI;
DWORD	MenuLoClr=CLR_MENULO;
DWORD	PrintClr=CLR_PRINT;

DWORD	Cycles, StartTime, HaltTime, SecondsCycles;

BOOL	InExec85=FALSE, CtlDown=FALSE, ShiftDown=FALSE;
int		SkipInts=0;

int		MENUx, MENUy, MENUw, MENUh, VW, VH;
int		REGx, REGy, REGw, REGh;
// REGw/REGh is width/height of ONE register cell
//****************************************************************************************
// FOR HP-85A and HP-85B
int		CRTx, CRTy, KEYx, KEYy, TAPEx, TAPEy;
int		PRTx, PRTy, PRTw, PRTh, PRTlen, PRTscroll;
// PRTlen is the length of the printout in "unstretched" (by CFG_BIGCRT) pixels
// Each line of TEXT is PTH tall (and each char is 7 pixels wide, giving a max
// print width of text of 7*32=224 pixels).
// Each "line" of GRAPHICS is 8 pixels tall, with 32 lines comprising the entire
// screen dump (with the graphics rotated so that it's 192 wide by 256 tall).
int		BRTx, BRTy, BRTw, BRTh;
int		RLx, RLy, RLw, RLh;

char	FilePath[PATHLEN];

PBMB	keyBM=NULL, tapeBM=NULL, disk525BM=NULL, disk35BM=NULL, disk8BM=NULL, disk5MBBM=NULL, prtBM=NULL, rlBM=NULL, hp85BM=NULL;

DWORD	CfgFlags, CurROMnum;
DWORD	TargetAddress, JumpAddress=0xFFFFFFFF;
DWORD	BackAddress[512], BackAddressCnt=0;

//***********************************************************
// The following MouseXXX is for repeating a key that's been
// clicked on by the left mouse button and had it held down.
#define	REPEAT_SLOW	250
#define	REPEAT_FAST	40
DWORD	MouseTime, MouseRepeatTime, MouseIndex;
BYTE	MouseKey;

//*********************************************************

BOOL	Halt85;
WORD	Int85;

DWORD	base=8;
BYTE	HexDig[] = "0123456789ABCDEF";

DWORD	Ptr1, Ptr2, Ptr1Cnt, Ptr2Cnt;
BYTE	Reg[64], LastReg[64];
BYTE	E, LastE;
BYTE	Flags, LastFlags;
#define	LSB		0x01
#define	RDZ		0x02
#define	Z		0x04
#define	OVF		0x08
#define	CY		0x10
#define	DCM		0x20
#define	LDZ		0x40
#define	MSB		0x80
WORD	Arp, Drp, LastArp, LastDrp;

DWORD	ROMlen;		// # of bytes allocated for *ROM
BYTE	*ROM=NULL;	// ptr to malloc'd memory for all of ROM, RAM, Extended RAM

// ROMnums[] contains the ROM numbers of the loaded option-ROMs.
// pROMS[] contains pointers to the 8K memory blocks into which the ROM codes get loaded
// numROMS, of course, keeps track of how many option-ROMs (including ROM0) have been loaded.
BYTE	ROMnums[MAXROMS];
BYTE	*pROMS[MAXROMS];
int		numROMS=0;
// ptrs to 8 blocks DISASMRECs for 8K blocks of memory: 0, 8K, 16K, 24K, 32K, 40K, 48K, 56K
// ptrs 0, 1, 2 contain records for the System ROMs, and 3 contains ROM0.
// ptrs 4,5,6,7 are all set the same, and contain records for all of RAM and the I/O addresses.
// (the last 4 are duplicated and all the same, so that it's easier to code the lookup routine,
// it just has to take an address, divide by 8K, and use that as the index into pDis[].
// For each of the 8 pDis entries, there are two values, the first ([0]) is the pointer to the
// FIRST record for that block, and the second ([1]) is the pointer to the LAST record for that block.
DISASMREC	*pDis[8][2];
// The following table contains first/last record pointer entries for each of the plug-in ROMs,
// including ROM0.  They get copied into pDis[3] when a ROM gets mapped into memory.  The 0th entry
// is ALWAYS ROM0, since it always gets loaded first before any option-ROMs.  The entries in this
// table match (index-wise) one-to-one with the entries in the pROMS[] table above.
DISASMREC	*pDisROM[MAXROMS][2];
BOOL	DisAsmNULL=FALSE;	// if TRUE, StoreDis must abort

DWORD	RomList[] = {001, 010, 016, 030, 047, 050, 070, 0250, 0260, 0261, 0300, 0317, 0320, 0321, 0340, 0347, 0350, 0360};
DWORD	RomListCnt=sizeof(RomList)/sizeof(DWORD);

DWORD	Model=0;

//*************************************
//*************************************
//*************************************
//*************************************
//*************************************
//** MACHINE SPECIFIC CONFIGURATIONS **
//*************************************
//*************************************
//*************************************
//*************************************
//*************************************
#define	NUMBRKPTS	10
WORD	BrkPts[3][NUMBRKPTS+1];
BYTE	Marks[3][8192];	// for CTL-F2 "mark line" and F2 "goto marked line"

// RomLoad is modified by the RomDlg in DLG.C.  So, to add more option
// ROMs to the emulator, add them to RomList[] above (making
// sure that you don't exceed MAXROMS or 32 total, whichever
// is less), then edit RomDlg in DLG.C to add your option ROM
// to the list of available ones, and CreateRomDlg() to init
// the checkboxes, and ROMDokProc() to read the checkboxes
// and set/clear the bits in RomLoad.  ALSO: you need to modify
// the ViewAddressDlg in DLG.C, and VADviewProc().
// Also, create a new "RADIO VADxxxRADIO" where 'xxx' is the name
// of your ROM (or some abbreviation thereof) in DLG.C (search for
// instanaces of VAD0RADIO to see where it's declared and used),
// and add the reference to it to VAD_CTLs[] in DLG.C, and then
// add it's OCTAL ROM# to the list above, inserting it in proper
// increasing numerical order.  Then increase the HEIGHT of the
// ViewAddressDlg dialog structure in DLG.C by 12 (to make room for
// the additional ROM entry in the dialog).  Then add a new
// "CHECK ROMDxxxCHECK" structure for your ROM (for the dialog that
// specifies which ROMs to load) and add the reference to it to
// ROMD_CTLs[], then increase the HEIGHT of the RomDlg dialog structure
// by 12.  For both of these dialogs, as you insert the new checkbox
// or radio button for the new ROM, you'll need to adjust the Y coordinate
// for all of the dialog box items that are located further down (higher
// Y coordinate) from where you insert your new ROM, adding 12 to all of
// those 'higher' Y coordinates.
// That SHOULD be all there is to it. :-)
DWORD	RomLoad, RomLoads[3]={0x00021020, 0x00023420, 0x00021021}, RomMUST[3]={0x00000000, 0x00003400, 0x00001001}, RomCANT[3]={0x0000A25D, 0x0000825D, 0x00004002};
DWORD	RamSize, RamSizes[3]={32, 32, 32}, RamSizeExt[3]={0, 1, 3};
// RamSize is 16 or 32, RamSizeExt=0 for none, 1 for 32K, 2 for 64K, 3 for 128K, 4 for 256K, 5 for 512K, 6 for 1024K

///************************************
///** END OF MODEL SPECIFIC SETTINGS **
///************************************

// First 0-7 IOcards are Select Codes 3-10 with corresponding CCR/PSR and OB/IB register I/O addresses
// IOcards 8-15 are for non-SelectCode cards (such as the System Monitor, etc).
// Most of these will probably never be used, but it's easier to structure it right than to hack it later.

/*
Part#    Description                                  ID=SR0  Select Code (default)
82937A   HP-IB Interface Module                        ID=1    SC=7
82939A   Serial Interface Module (Opt 001, Opt 002)    ID=2    SC=10
82950A   Modem Module                                  ID=2    SC=10
82966A   Data Link Interface Module (up to 19200 baud) ID=2?   SC=10
82941A   BCD Interface Module                          ID=3    SC=3
82940A   GPIO Module Module                            ID=4    SC=4
82949A   Printer Interface Module                      ID=4    SC=8
82938A   HP-IL Interface Module                        ID=5    SC=9
internal HP-86A "parallel port" floppy disk interface  ID=6    SC=7
82967A   Speech Synthesis Module                       ID=7    SC=10

#define	IOTYPE_NONE		0
#define	IOTYPE_HPIB		1
#define	IOTYPE_SERIAL	2
#define	IOTYPE_MODEM	3
#define IOTYPE_DATALINK	4
#define	IOTYPE_BCD		5
#define	IOTYPE_GPIO		6
#define	IOTYPE_PRINTER	7
#define	IOTYPE_HPIL		8
#define	IOTYPE_SPEECH	9
#define	IOTYPE_SYSMON	10

#define	IOTYPE_NUM		11	// number of IOTYPEs

typedef struct {
	WORD	address;	// base I/O address (like 177530, etc)
	WORD	numaddr;	// number of sequential I/O addresses used (usually 2)
	long	type;		// see IOTYPE_ #define list
	int		(*initProc)(long reason, long model, long sc, DWORD arg);	// functional to call for setup/de-setup/etc
	void	*data;		// S.C.-specific pointer to the card's data structure (up to the card's emulator code to set up during iR_STARTUP call to *initProc()
} IOCARD;

// NOTE: SelectCodes (sc) map to IO addresses thusly (numbers are
// IOcard_index/real_select_code and CCR_PSR/OB_IB):
//	0/3		177520/177521
//	1/4		177522/177523
//	2/5		177524/177525
//	3/6		177526/177527
//	4/7		177530/177531
//	5/8		177532/177533
//	6/9		177534/177535
//	7/10	177536/177537
*/

IOCARD	IOcards[16];

// add initFUNC() to this table if you implement any other IO card types!
int (*initProcs[IOTYPE_NUM])(long reason, long model, long sc, DWORD arg)={
	NULL,
	&initHPIB,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

// add initFUNC() to this table if you implement any other IO devices!
int (*devProcs[])(long reason, int sc, int tla, int d, int val)={
	&devDISK,	// 0 DEVTYPE_5_25
	&devDISK,	// 1 DEVTYPE_3_5
	&devDISK,	// DEVTYPE_8
	&devDISK,	// DEVTYPE_HARD5
	NULL,	// 4
	NULL,	// 5
	NULL,	// 6
	NULL,	// 7
	NULL,	// 8
	NULL,	// 9
	&devFilePRT,	// 10 DEVTYPE_FILEPRT
	&devHardPRT,	// 11 DEVTYPE_HARDPRT
	NULL,	// 12
};

DWORD	IO_PendingInt;	// one bit per SC if wants to interrupt, checked during Exec85() loop
BOOL	IOcardsIntEnabled=FALSE;
BYTE	IO_SC;			// interrupting SC

/*
typedef struct {
	int		devID;		// unique device number
	int		devType;	// DEVTYPE_ (DEVTYPE_DISKETTE, DEVTYPE_FILEPRT, DEVTYPE_HARDPRT, etc)
	int		ioType;		// IOTYPE_ (always IOTYPE_HPIB for now)
	int		sc, wsc;	// Select Code of IO card this device is on, and SC to WRITE to the INI file (for purposes of moving devices around)
	int		ca, wca;	// Controller Address (for HPIB) at which this device exists, and CA to WRITE to the INI file (for purposes of moving it)
	int		state;		// DEVSTATE_ whether device at boot-time, or added or deleted in DEVICES dialog
	int		(*initProc)(long reason, int sc, int tla, int d, int val);
} DEV;
*/
DEV		Devices[MAX_DEVICES];	// all the external devices (disks, printers, etc), set up by ReadIni()
/// NOTE: The DEVSTATE_NEW in the following table is because this is only used when there is NO .ini file
/// so the initProc(iR_READ_INI) needs to know not to look for its entry.
DEV		DefDevices[]={			// the default devices if no .ini file
	{0, DEVTYPE_5_25,	 IOTYPE_HPIB, 7, 7,  0,  0, DEVSTATE_BOOT, &devDISK},
	{1, DEVTYPE_FILEPRT, IOTYPE_HPIB, 7, 7, 10, 10, DEVSTATE_BOOT, &devFilePRT},
	{2, DEVTYPE_HARDPRT, IOTYPE_HPIB, 7, 7, 20, 20, DEVSTATE_BOOT, &devHardPRT},
};
int		SCwini;

#if 0
//*****************************************************************
// 177
BYTE io(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
	}
	return ;	// READ
}

#endif
//*****************************************************************
// 'address' is the IO address being read or written
// 'val' is 0-255 if READING, <0 || >255 if WRITING
// when READING, return value is the read byte value, when WRITING the return value is ignored
BYTE (*ioProc[256])(WORD address, long val)={
	&ioGINTEN,&ioGINTDS,&ioKEYSTS,&ioKEYCOD,&ioCRTSAD85,&ioCRTBAD85,&ioCRTSTS85,&ioCRTDAT85,	// 177400
	&ioTAPSTS,&ioTAPDAT,&ioCLKSTS,&ioCLKDAT,&ioPRTLEN,&ioPRCHAR,&ioPRTSTS,&ioPRTDAT,			// 177410
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177420
	&ioRSELEC,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177430
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177440
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177450
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177460
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177470
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177500
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177510
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177520
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177530
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177540
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177550
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177560
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177570
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177600
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177610
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177620
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177630
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177640
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177650
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177660
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177670
	&ioCRTSAD87,&ioCRTBAD87,&ioCRTSTS87,&ioCRTDAT87,&ioRULITE,&ioNULL,&ioNULL,&ioNULL,	// 177700
	&ioPTR1,&ioPTR1,&ioPTR1,&ioPTR1,&ioPTR2,&ioPTR2,&ioPTR2,&ioPTR2,	// 177710
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177720
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177730
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177740
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177750
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,	// 177760
	&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL,&ioNULL	// 177770
};


//*********************************************************

WORD	TC_BCD[256] = {
	0x00,0x99,0x98,0x97,0x96,0x95,0x94,0x93,0x92,0x91,0x90,0x97,0x96,0x95,0x94,0x93,
	0x90,0x89,0x88,0x87,0x86,0x85,0x84,0x83,0x82,0x81,0x80,0x87,0x86,0x85,0x84,0x83,
	0x80,0x79,0x78,0x77,0x76,0x75,0x74,0x73,0x72,0x71,0x70,0x77,0x76,0x75,0x74,0x73,
	0x70,0x69,0x68,0x67,0x66,0x65,0x64,0x63,0x62,0x61,0x60,0x67,0x66,0x65,0x64,0x63,
	0x60,0x59,0x58,0x57,0x56,0x55,0x54,0x53,0x52,0x51,0x50,0x57,0x56,0x55,0x54,0x53,
	0x50,0x49,0x48,0x47,0x46,0x45,0x44,0x43,0x42,0x41,0x40,0x47,0x46,0x45,0x44,0x43,
	0x40,0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,0x37,0x36,0x35,0x34,0x33,
	0x30,0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x27,0x26,0x25,0x24,0x23,
	0x20,0x19,0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11,0x10,0x17,0x16,0x15,0x14,0x13,
	0x10,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,0x07,0x06,0x05,0x04,0x03,
	0x00,0x79,0x78,0x77,0x76,0x75,0x74,0x73,0x72,0x71,0x70,0x77,0x76,0x75,0x74,0x73,
	0x70,0x69,0x68,0x67,0x66,0x65,0x64,0x63,0x62,0x61,0x60,0x67,0x66,0x65,0x64,0x63,
	0x60,0x59,0x58,0x57,0x56,0x55,0x54,0x53,0x52,0x51,0x50,0x57,0x56,0x55,0x54,0x53,
	0x50,0x49,0x48,0x47,0x46,0x45,0x44,0x43,0x42,0x41,0x40,0x47,0x46,0x45,0x44,0x43,
	0x40,0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,0x37,0x36,0x35,0x34,0x33,
	0x30,0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x27,0x26,0x25,0x24,0x23
};
WORD	NC_BCD[256] = {
	0x99,0x98,0x97,0x96,0x95,0x94,0x93,0x92,0x91,0x90,0x97,0x96,0x95,0x94,0x93,0x92,
	0x89,0x88,0x87,0x86,0x85,0x84,0x83,0x82,0x81,0x80,0x87,0x86,0x85,0x84,0x83,0x82,
	0x79,0x78,0x77,0x76,0x75,0x74,0x73,0x72,0x71,0x70,0x77,0x76,0x75,0x74,0x73,0x72,
	0x69,0x68,0x67,0x66,0x65,0x64,0x63,0x62,0x61,0x60,0x67,0x66,0x65,0x64,0x63,0x62,
	0x59,0x58,0x57,0x56,0x55,0x54,0x53,0x52,0x51,0x50,0x57,0x56,0x55,0x54,0x53,0x52,
	0x49,0x48,0x47,0x46,0x45,0x44,0x43,0x42,0x41,0x40,0x47,0x46,0x45,0x44,0x43,0x42,
	0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,0x37,0x36,0x35,0x34,0x33,0x32,
	0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x27,0x26,0x25,0x24,0x23,0x22,
	0x19,0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11,0x10,0x17,0x16,0x15,0x14,0x13,0x12,
	0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,0x07,0x06,0x05,0x04,0x03,0x02,
	0x79,0x78,0x77,0x76,0x75,0x74,0x73,0x72,0x71,0x70,0x77,0x76,0x75,0x74,0x73,0x72,
	0x69,0x68,0x67,0x66,0x65,0x64,0x63,0x62,0x61,0x60,0x67,0x66,0x65,0x64,0x63,0x62,
	0x59,0x58,0x57,0x56,0x55,0x54,0x53,0x52,0x51,0x50,0x57,0x56,0x55,0x54,0x53,0x52,
	0x49,0x48,0x47,0x46,0x45,0x44,0x43,0x42,0x41,0x40,0x47,0x46,0x45,0x44,0x43,0x42,
	0x39,0x38,0x37,0x36,0x35,0x34,0x33,0x32,0x31,0x30,0x37,0x36,0x35,0x34,0x33,0x32,
	0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,0x27,0x26,0x25,0x24,0x23,0x22
};

// This table gives the result of A+B, where A and B are any HEX digits (one each).
// In other words, even if the inputs are illegal BCD digits, this table gives the
// results as the Series 80 CPU would have calculated them.
// AD_BCD[A*16+B] gives the result, which if from 0x0 to 0x9 CY should NOT be set (ie, no carry out),
// and if from 0x10 to 0x1E if CY should be set (ie, carry out).
WORD AD_BCD[256] = {
	0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,
	0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,
	0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
	0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,
	0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,
	0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,
	0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,
	0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,0x001C,
	0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,
	0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,
	0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
	0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,
	0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,
	0x0005,0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,
	0x0006,0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,
	0x0007,0x0008,0x0009,0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,0x001C
};

// This table gives the result of A-B, where A and B are any HEX digits (one each).
// In other words, even if the inputs are illegal BCD digits, this table gives the
// results as the Series 80 CPU would have calculated them.
// SB_BCD[A*16+B] gives the result, which is from 0x0 to 0xF if A>B and CY should be set (ie, no borrow),
// and it's from 0x90 to 0x99 if A<B and CY should be cleared (ie, borrowed).
WORD	SB_BCD[256] = {
	0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,0x0090,0x0097,0x0096,0x0095,0x0094,0x0093,
	0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,0x0090,0x0097,0x0096,0x0095,0x0094,
	0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,0x0090,0x0097,0x0096,0x0095,
	0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,0x0090,0x0097,0x0096,
	0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,0x0090,0x0097,
	0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,0x0090,
	0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,0x0091,
	0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,0x0092,
	0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,0x0093,
	0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,0x0094,
	0x000A,0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,0x0095,
	0x000B,0x000A,0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,0x0096,
	0x000C,0x000B,0x000A,0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,0x0097,
	0x000D,0x000C,0x000B,0x000A,0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,0x0098,
	0x000E,0x000D,0x000C,0x000B,0x000A,0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,0x0099,
	0x000F,0x000E,0x000D,0x000C,0x000B,0x000A,0x0009,0x0008,0x0007,0x0006,0x0005,0x0004,0x0003,0x0002,0x0001,0x0000,
};

char		TraceOutbuf[512];
WORD		TraceX, TraceY, TraceW, TraceH;
DWORD		LineIndex[1024], DisStart=0, DisEnd;
DISASMREC	*LineRecs[1024], *CurRec=NULL;
DWORD		TraceLines, CursLine=0xFFFFFFFF;
BYTE		DisArp=0xFF, DisDrp=0xFF, DisTyp=REC_COD;
WORD		DisDataOverride;
DWORD		ScrollIndex[2048];
#define	NUMSI	(sizeof(ScrollIndex)/sizeof(DWORD))

DWORD		NxtByt, NxtRom, saveNxtByt;	// "pointer" to next byte to disassemble
DISASMREC	*NextRec, *pDefLabel=NULL, ScratchDISASM;


char	*opc[256] = {
	"ARP ",	// 000
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"ARP ",
	"DRP ",	// 100
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"DRP ",
	"ELB ",	// 200
	"ELM ",
	"ERB ",
	"ERM ",
	"LLB ",
	"LLM ",
	"LRB ",
	"LRM ",
	"ICB ",	// 210
	"ICM ",
	"DCB ",
	"DCM ",
	"TCB ",
	"TCM ",
	"NCB ",
	"NCM ",
	"TSB ",	// 220
	"TSM ",
	"CLB ",
	"CLM ",
	"ORB ",
	"ORM ",
	"XRB ",
	"XRM ",
	"BIN ",	// 230
	"BCD ",
	"SAD",
	"DCE ",
	"ICE ",
	"CLE ",
	"RTN ",
	"PAD ",
	"LDB ",	// 240
	"LDM ",
	"STB ",
	"STM ",
	"LDBD",
	"LDMD",
	"STBD",
	"STMD",
	"LDB ",	// 250
	"LDM ",
	"STB ",
	"STM ",
	"LDBI",
	"LDMI",
	"STBI",
	"STMI",
	"LDBD",	// 260
	"LDMD",
	"STBD",
	"STMD",
	"LDBD",
	"LDMD",
	"STBD",
	"STMD",
	"LDBI",	// 270
	"LDMI",
	"STBI",
	"STMI",
	"LDBI",
	"LDMI",
	"STBI",
	"STMI",
	"CMB ",	// 300
	"CMM ",
	"ADB ",
	"ADM ",
	"SBB ",
	"SBM ",
	"JSB ",
	"ANM ",
	"CMB ",	// 310
	"CMM ",
	"ADB ",
	"ADM ",
	"SBB ",
	"SBM ",
	"JSB ",
	"ANM ",
	"CMBD",	// 320
	"CMMD",
	"ADBD",
	"ADMD",
	"SBBD",
	"SBMD",
// The following NOP3 (opcode 326) is not a real opcode and was
// never used (so far as I know) by any Series 80 code.  It is
// an officially unsupported opcode in the HP-85 CPU.
// The hardware engineers investigated it for me in the early 1980's
// and found that, the way the CPU was designed, it resulted in a
// 3-byte "no operation".  What it does is fetch the following two
// bytes from memory, but does nothing with them.  So, this code:
//	BYT	326
//	BYT skip1
//	BYT skip2
//	BYT nextopcode
// will skip the skip1 and skip2 bytes and then resume execution
// with the nextopcode byte, with no modifications to the CPU registers,
// status, or memory, other than the incrementing of the PC (reg 4).
// I include it here just for the sake of completeness.
	"NOP3",

	"ANMD",
	"CMBD",	// 330
	"CMMD",
	"ADBD",
	"ADMD",
	"SBBD",
	"SBMD",
// The following NOP1 (opcode 336) is not a real opcode and was
// never used (so far as I know) by any Series 80 code.  It is
// an officially unsupported opcode in the HP-85 CPU.
// The hardware engineers investigated it for me in the early 1980's
// and found that, the way the CPU was designed, it resulted in a
// 1-byte "no operation".  So, this code:
//	BYT	336
//	BYT nextopcode
// fetches the 326 opcode, but does nothing, and then resumes execution
// with the nextopcode byte, with no modifications to the CPU registers,
// status, or memory, other than the incrementing of the PC (reg 4).
	"NOP1",

	"ANMD",
	"POBD",	// 340
	"POMD",
	"POBD",
	"POMD",
	"PUBD",
	"PUMD",
	"PUBD",
	"PUMD",
	"POBI",	// 350
	"POMI",
	"POBI",
	"POMI",
	"PUBI",
	"PUMI",
	"PUBI",
	"PUMI",
	"JMP ",	// 360
	"JNO ",
	"JOD ",
	"JEV ",
	"JNG ",
	"JPS ",
	"JNZ ",
	"JZR ",
	"JEN ",	// 370
	"JEZ ",
	"JNC ",
	"JCY ",
	"JLZ ",
	"JLN ",
	"JRZ ",
	"JRN ",
};

// This is the font used in the HP-85 CRT and Printer Controllers.
// The font (pixel bits) could not be read from the CRT controller,
// but was made readable from the Printer Controller so that the
// BASIC "LABEL" statement could use them for labelling text on the
// graphics CRT screen.  The HP85font[][] data below is used by this
// emulator for alpha AND graphics output to the emulated CRT and Printer.
// The font is only 128 characters.  If the most significant bit was set
// on a character, then that caused the character to be underlined.  This
// is how the "cursor" underscore was implemented, as well as normal
// underlining, which caused problems, because moving the cursor over
// a quoted string with underlined characters would erase the underline.
// That was fixed on the HP-86/87 by the addition of a hardware alpha cursor.

// An additional 7 chars are added at the end for the HP-87 chars that are different.
// Some chars are also re-arranged on the HP-87, so for it, we access these fonts
// indirectly through the HP87font[] index table.
BYTE	HP85font[135][8]={
	{0x00,0x10,0x38,0x7C,0xFE,0x00,0x00,0x00},{0x0C,0x12,0xA2,0x02,0x0C,0x00,0x00,0x00},
	{0xA2,0x94,0x88,0x94,0xA2,0x00,0x00,0x00},{0xBE,0x90,0x88,0x84,0xBE,0x00,0x00,0x00},
	{0x1C,0x22,0x22,0x1C,0x22,0x00,0x00,0x00},{0x3E,0x54,0x52,0x52,0x2C,0x00,0x00,0x00},
	{0xFE,0x80,0x80,0x80,0xC0,0x00,0x00,0x00},{0xBE,0x90,0xA0,0xA0,0x9E,0x00,0x00,0x00},
	{0x06,0x1A,0x62,0x1A,0x06,0x00,0x00,0x00},{0x1C,0x22,0x22,0x3C,0x20,0x00,0x00,0x00},
	{0x20,0x40,0xFE,0x40,0x20,0x00,0x00,0x00},{0x8E,0x50,0x20,0x10,0x0E,0x00,0x00,0x00},
	{0x02,0x3C,0x04,0x04,0x38,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x10,0x20,0x3E,0x20,0x40,0x00,0x00,0x00},{0x10,0xAA,0xFE,0xAA,0x10,0x00,0x00,0x00},
	{0x7C,0x92,0x92,0x92,0x7C,0x00,0x00,0x00},{0x3A,0x46,0x40,0x46,0x3A,0x00,0x00,0x00},
	{0x0C,0x52,0xB2,0x92,0x0C,0x00,0x00,0x00},{0x3E,0x48,0xC8,0x48,0x3E,0x00,0x00,0x00},
	{0x1C,0x22,0xA2,0x3C,0x02,0x00,0x00,0x00},{0x3E,0xC8,0x48,0xC8,0x3E,0x00,0x00,0x00},
	{0x1C,0xA2,0x22,0xBC,0x02,0x00,0x00,0x00},{0x3C,0xC2,0x42,0xC2,0x3C,0x00,0x00,0x00},
	{0x1C,0xA2,0x22,0xA2,0x1C,0x00,0x00,0x00},{0x7C,0x82,0x02,0x82,0x7C,0x00,0x00,0x00},
	{0x3C,0x82,0x02,0x82,0x3E,0x00,0x00,0x00},{0x7E,0x90,0xFE,0x92,0x82,0x00,0x00,0x00},
	{0x1C,0x22,0x1C,0x2A,0x12,0x00,0x00,0x00},{0x48,0x98,0xA8,0x48,0x00,0x00,0x00,0x00},
	{0x12,0x7E,0x92,0x82,0x42,0x00,0x00,0x00},{0xAA,0x54,0xAA,0x54,0xAA,0x00,0x00,0x00},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0xFA,0x00,0x00,0x00,0x00,0x00},
	{0x00,0xE0,0x00,0xE0,0x00,0x00,0x00,0x00},{0x28,0xFE,0x28,0xFE,0x28,0x00,0x00,0x00},
	{0x24,0x54,0xFE,0x54,0x48,0x00,0x00,0x00},{0xC4,0xC8,0x10,0x26,0x46,0x00,0x00,0x00},
	{0x6C,0x92,0x6A,0x04,0x0A,0x00,0x00,0x00},{0x00,0x00,0xE0,0x00,0x00,0x00,0x00,0x00},
	{0x00,0x38,0x44,0x82,0x00,0x00,0x00,0x00},{0x00,0x82,0x44,0x38,0x00,0x00,0x00,0x00},
	{0x44,0x28,0xFE,0x28,0x44,0x00,0x00,0x00},{0x10,0x10,0x7C,0x10,0x10,0x00,0x00,0x00},
	{0x00,0x02,0x0C,0x00,0x00,0x00,0x00,0x00},{0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00},
	{0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00},{0x04,0x08,0x10,0x20,0x40,0x00,0x00,0x00},
	{0x7C,0x8A,0x92,0xA2,0x7C,0x00,0x00,0x00},{0x00,0x42,0xFE,0x02,0x00,0x00,0x00,0x00},
	{0x46,0x8A,0x92,0x92,0x62,0x00,0x00,0x00},{0x84,0x82,0x92,0xB2,0xCC,0x00,0x00,0x00},
	{0x18,0x28,0x48,0xFE,0x08,0x00,0x00,0x00},{0xE4,0xA2,0xA2,0xA2,0x9C,0x00,0x00,0x00},
	{0x3C,0x52,0x92,0x92,0x8C,0x00,0x00,0x00},{0x80,0x8E,0x90,0xA0,0xC0,0x00,0x00,0x00},
	{0x6C,0x92,0x92,0x92,0x6C,0x00,0x00,0x00},{0x62,0x92,0x92,0x94,0x78,0x00,0x00,0x00},
	{0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x00},{0x00,0x02,0x2C,0x00,0x00,0x00,0x00,0x00},
	{0x00,0x10,0x28,0x44,0x82,0x00,0x00,0x00},{0x28,0x28,0x28,0x28,0x28,0x00,0x00,0x00},
	{0x82,0x44,0x28,0x10,0x00,0x00,0x00,0x00},{0x60,0x80,0x8A,0x90,0x60,0x00,0x00,0x00},
	{0x7C,0x82,0xBA,0x9A,0x72,0x00,0x00,0x00},{0x7E,0x90,0x90,0x90,0x7E,0x00,0x00,0x00},
	{0xFE,0x92,0x92,0x92,0x6C,0x00,0x00,0x00},{0x7C,0x82,0x82,0x82,0x44,0x00,0x00,0x00},
	{0xFE,0x82,0x82,0x82,0x7C,0x00,0x00,0x00},{0xFE,0x92,0x92,0x92,0x82,0x00,0x00,0x00},
	{0xFE,0x90,0x90,0x90,0x80,0x00,0x00,0x00},{0x7C,0x82,0x82,0x8A,0x8E,0x00,0x00,0x00},
	{0xFE,0x10,0x10,0x10,0xFE,0x00,0x00,0x00},{0x00,0x82,0xFE,0x82,0x00,0x00,0x00,0x00},
	{0x04,0x02,0x02,0x02,0xFC,0x00,0x00,0x00},{0xFE,0x10,0x28,0x44,0x82,0x00,0x00,0x00},
	{0xFE,0x02,0x02,0x02,0x02,0x00,0x00,0x00},{0xFE,0x40,0x30,0x40,0xFE,0x00,0x00,0x00},
	{0xFE,0x20,0x10,0x08,0xFE,0x00,0x00,0x00},{0x7C,0x82,0x82,0x82,0x7C,0x00,0x00,0x00},
	{0xFE,0x90,0x90,0x90,0x60,0x00,0x00,0x00},{0x7C,0x82,0x8A,0x84,0x7A,0x00,0x00,0x00},
	{0xFE,0x90,0x98,0x94,0x62,0x00,0x00,0x00},{0x64,0x92,0x92,0x92,0x4C,0x00,0x00,0x00},
	{0x80,0x80,0xFE,0x80,0x80,0x00,0x00,0x00},{0xFC,0x02,0x02,0x02,0xFC,0x00,0x00,0x00},
	{0xF0,0x0C,0x02,0x0C,0xF0,0x00,0x00,0x00},{0xFE,0x04,0x18,0x04,0xFE,0x00,0x00,0x00},
	{0xC6,0x28,0x10,0x28,0xC6,0x00,0x00,0x00},{0xC0,0x20,0x1E,0x20,0xC0,0x00,0x00,0x00},
	{0x86,0x8A,0x92,0xA2,0xC2,0x00,0x00,0x00},{0xFE,0xFE,0x82,0x82,0x82,0x00,0x00,0x00},
	{0x40,0x20,0x10,0x08,0x04,0x00,0x00,0x00},{0x82,0x82,0x82,0xFE,0xFE,0x00,0x00,0x00},
	{0x10,0x20,0x40,0x20,0x10,0x00,0x00,0x00},{0x02,0x02,0x02,0x02,0x02,0x00,0x00,0x00},
	{0x00,0x80,0x40,0x20,0x00,0x00,0x00,0x00},{0x04,0x2A,0x2A,0x2A,0x1E,0x00,0x00,0x00},
	{0xFE,0x12,0x22,0x22,0x1C,0x00,0x00,0x00},{0x1C,0x22,0x22,0x22,0x22,0x00,0x00,0x00},
	{0x1C,0x22,0x22,0x12,0xFE,0x00,0x00,0x00},{0x1C,0x2A,0x2A,0x2A,0x18,0x00,0x00,0x00},
	{0x00,0x10,0x7E,0x90,0x00,0x00,0x00,0x00},{0x10,0x28,0x2A,0x2A,0x3C,0x00,0x00,0x00},
	{0xFE,0x10,0x20,0x20,0x1E,0x00,0x00,0x00},{0x00,0x22,0xBE,0x02,0x00,0x00,0x00,0x00},
	{0x04,0x02,0x22,0xBC,0x00,0x00,0x00,0x00},{0x00,0xFE,0x08,0x14,0x22,0x00,0x00,0x00},
	{0x00,0x82,0xFE,0x02,0x00,0x00,0x00,0x00},{0x3E,0x20,0x1E,0x20,0x1E,0x00,0x00,0x00},
	{0x3E,0x10,0x20,0x20,0x1E,0x00,0x00,0x00},{0x1C,0x22,0x22,0x22,0x1C,0x00,0x00,0x00},
	{0x3E,0x28,0x28,0x28,0x10,0x00,0x00,0x00},{0x10,0x28,0x28,0x3E,0x02,0x00,0x00,0x00},
	{0x00,0x3E,0x10,0x20,0x20,0x00,0x00,0x00},{0x12,0x2A,0x2A,0x2A,0x24,0x00,0x00,0x00},
	{0x00,0x20,0x7C,0x22,0x00,0x00,0x00,0x00},{0x3C,0x02,0x02,0x04,0x3E,0x00,0x00,0x00},
	{0x30,0x0C,0x02,0x0C,0x30,0x00,0x00,0x00},{0x3C,0x02,0x0C,0x02,0x3C,0x00,0x00,0x00},
	{0x22,0x14,0x08,0x14,0x22,0x00,0x00,0x00},{0x22,0x14,0x08,0x10,0x20,0x00,0x00,0x00},
	{0x22,0x26,0x2A,0x32,0x22,0x00,0x00,0x00},{0x10,0x3E,0x20,0x3E,0x40,0x00,0x00,0x00},
	{0x00,0x00,0xEE,0x00,0x00,0x00,0x00,0x00},{0x10,0x10,0x54,0x38,0x10,0x00,0x00,0x00},
	{0x82,0xC6,0xAA,0x92,0xC6,0x00,0x00,0x00},{0xFE,0x10,0x10,0x10,0x10,0x00,0x00,0x00},
	// 87-unique chars start here
	{0x10,0x38,0x54,0x10,0x10,0x00,0x00,0x00},	// lf arrow (^H)			128
	{0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00},	// vertical line drawing	129
	{0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},	// horizontal line drawing	130
	{0x10,0x10,0xFF,0x10,0x10,0x10,0x10,0x10},	// intersection line drawing131
	{0x10,0x20,0x10,0x08,0x10,0x00,0x00,0x00},	// tilde					132
	{0x00,0x10,0x6C,0x82,0x00,0x00,0x00,0x00},	// open brace {				133
	{0x00,0x82,0x6C,0x10,0x00,0x00,0x00,0x00}	// close brace }			134
};
int		HP87font[128]={
	0,1,2,3,4,5,126,8,128,9,10,11,12,13,14,123,16,27,28,19,
	20,21,22,23,24,25,26,127,129,130,30,131,32,33,34,35,36,37,38,39,
	40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,
	60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
	80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,
	100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,
	120,121,122,133,124,134,132,31
};

//******************************************************
// Relative locations of HP-85 keys on emulator screen (for small size, double for large size)
typedef struct {
	long	x, y, w, h;
	WORD	keycod, shiftkeycod;
} HP85KEY;

HP85KEY	Keys85[23] = {
	{407, 154, 63, 42, 138, 138},	// paper advance
	{344, 196, 42, 42,  40, 139},	// reset
	{386, 196, 42, 42,  41, 140},	// init
	{428, 196, 42, 42,  94, 166},	// result
	{  0, 154, 42, 42, 148, 149},	// list/plist
	{  0, 196, 42, 42, 150, 150},	// key label
	{ 44, 196, 64, 42, 128, 132},	// k1/k5
	{108, 196, 64, 42, 129, 133},	// k2/k6
	{172, 196, 64, 42, 130, 134},	// k3/k7
	{236, 196, 64, 42, 131, 135},	// k4/k8
	{  3, 238, 42, 42, 161, 165},	// up cursor/home cursor
	{ 45, 238, 42, 42, 162, 172},	// dn cursor/auto
	{ 87, 238, 42, 42, 156, 147},	// lf cursor/graph
	{129, 238, 42, 42, 157, 137},	// rt cursor/copy
	{171, 238, 42, 42, 163, 163},	// ins/rpl
	{213, 238, 42, 42, 164, 168},	// -char/delete
	{255, 238, 42, 42, 159, 158},	// roll dn/roll up
	{428, 238, 42, 42, 141, 141},	// run
	{365, 238, 63, 42, 142, 144},	// pause/step
	{302, 238, 63, 42, 143, 173},	// cont/scratch
	{302, 196, 42, 42, 160, 146},	// -line/clear
	{302, 154, 42, 42, 170, 136},	// load/rew
	{344, 154, 42, 42, 169, 145}	 // store/test
};

HP85KEY	Keys87[22] = {
	{163, 302, 58, 58,  40, 139},	// reset
	{221, 302, 58, 58,  41, 140},	// init
	{279, 302, 58, 58,  94, 166},	// result
	{  0,  70, 58, 58, 148, 149},	// list/plist
	{  0, 244, 87, 58, 150, 150},	// key label
	{105, 244, 87, 58, 128, 132},	// k1/k8
	{192, 244, 87, 58, 129, 133},	// k2/k9
	{279, 244, 87, 58, 130, 134},	// k3/k10
	{366, 244, 87, 58, 131, 135},	// k4/k11
	{453, 244, 87, 58, 161, 165},	// k5/k12
	{540, 244, 87, 58, 162, 172},	// k6/k13
	{627, 244, 87, 58, 156, 147},	// k7/k14
	{424, 302, 58, 58, 163, 163},	// up cursor/home cursor
	{482, 302, 58, 58, 164, 168},	// dn cursor/AG
	{540, 302, 58, 58, 159, 158},	// lf cursor/IR
	{598, 302, 58, 58, 170, 136},	// rt cursor/-char
	{656, 302, 58, 58, 169, 145},	// roll dn/roll up
	{  0, 128, 58, 58, 141, 141},	// run
	{  0, 186, 87, 58, 142, 144},	// pause/step
	{  0, 302, 87, 58, 143, 173},	// cont/TrNormal
	{366, 302, 58, 58, 157, 137},	// -line/clear
	{105, 302, 58, 58, 160, 146}	 // E/test
};

//********************************************************
char *DbgMenuStr[] = {
	"    F1  HELP",
	"    R   SERIES 80 OPTIONS",
	"    O   EMULATOR OPTIONS",
	"    D   DISASSEMBLE AT ADDRESS",
	"    E   EDIT CURRENT LINE",
	"    C   INSERT FULL-LINE COMMENT",
	"    X   EXPORT ROM DISASSEMBLY",
	"    F8  RUN (EMULATE)",
	"CTL-F12 RESET TO POWER-ON",
	"        EXIT"
};
#define	DBGMENUCNT	(sizeof(DbgMenuStr)/sizeof(char *))

char *RunMenuStr[] = {
	"        HELP",
	"        SERIES 80 OPTIONS",
	"        EMULATOR OPTIONS",
	"        TYPE ASCII FILE AS KEYS",
	"        IMPORT LIF FILE TO :D700",
	"        EXPORT LIF FILE FROM :D700",
	"",
	"CTL-F8  BREAK INTO DEBUGGER",
	"CTL-F12 RESET TO POWER-ON",
	"        EXIT"
};
#define	RUNMENUCNT	(sizeof(RunMenuStr)/sizeof(char *))

char	**MenuStr=DbgMenuStr;
int		CurMenuItem=-1, MenuCnt=DBGMENUCNT;

#define	MENU_HELP	0
#define	MENU_ROMS	1
#define	MENU_OPTS	2
#define	MENU_ADDR	3
#define	MENU_TYPE	3
#define	MENU_EDIT	4
#define	MENU_IMPORT	4
#define	MENU_COMM	5
#define	MENU_EXPORT	5
#define	MENU_EXP	6
#define	MENU_RUNBRK	7
#define	MENU_RESET	8
#define	MENU_EXIT	9

long		KeyMemLen;
BYTE		*pKeyMem=NULL, *pKeyPtr, pKeyLastKEYCOD=255;
DWORD		pKeyLastTime;

//*******************************************************
// This is a REAL hack!  Designed for importing LIF file formats
// directly into memory, so they can be stored to tape.  What a hack. <ugh>
// This was done BEFORE HP-IB disk drives and Import/Export functions
// were implemented.  The code is left here and in the HP85OnMButtUp() function
// purely for archival purposes in case I ever want to do something similar
// for any reason.
/*
void Import(int t) {

	int		f;
	DWORD	dw;
	WORD	FWUSER, w, s, d, len, p;
	BYTE	*pMem, *pB, *pS;

	if( -1==(f=Kopen("import.bin", O_RDBIN)) ) return;
	dw = Kseek(f, 0, SEEK_END);
	Kseek(f, 0, SEEK_SET);
	if( NULL==(pMem=(BYTE *)malloc(dw)) ) {
		Kclose(f);
		return;
	}
	Kread(f, pMem, dw);
	Kclose(f);

	if( t==0 ) {	// BASIC program
		if( *(pMem+0x0206) & 0x80 ) SystemMessage("Sorry, COMMON variables");
		else {
			w = *((WORD *)(pMem+0x011C));	// get file length from LIF dir
			FWUSER = GETROMW(0100000);
			memcpy(ROM + FWUSER, pMem+0x0200, w);	// copy the bytes into HP-85 RAM
			SETROMW(0100002, FWUSER);		// set FWPRGM
			SETROMW(0100004, FWUSER);		// set FWCURR
			FWUSER += w;
			SETROMW(0100006, FWUSER);		// set NXTMEM
			SETROMW(0101132, FWUSER);		// set TOS
			SETROMW(0101130, FWUSER);		// set STSIZE
			SETREGW(012, FWUSER);			// set R12 stack ptr
		}
	} else if( t==1 ) {	// ASSEMBLER source
#if 1
		w = *((WORD *)(pMem+0x011C));	// get file length from LIF dir
		FWUSER = GETROMW(0100000);
		memcpy(ROM + FWUSER, pMem+0x0200, w);	// copy the bytes into HP-85 RAM
#else
		w = *((WORD *)(pMem+7));	// get file length from PCB
		FWUSER = *((WORD *)(ROM+0100000));
		memcpy(ROM + FWUSER, pMem, w);			// copy the bytes into HP-85 RAM
#endif
		SETROMW(0100002, FWUSER);		// set FWPRGM
		SETROMW(0100004, FWUSER);		// set FWCURR
		FWUSER += w;
		SETROMW(0100006, FWUSER);	// set NXTMEM
		SETROMW(0101132, FWUSER);	// set TOS
		SETROMW(0101130, FWUSER);	// set STSIZE
		SETREGW(012, FWUSER);			// set R12 stack ptr
	} else if( t==2 ) {	// BINARY PROGRAM
		// LAVAIL 100010
		// CALVRB 100012
		// RTNSTK 100014
		// NXTRTN 100016
		// FWBIN  100020
		// LWAMEM 100022
		// BINTAB 101233

		len = *((WORD *)(pMem+0x011C));	// get file length from LIF dir
		if( GETROMW(0101233)==0 ) {	// if no BPGM loaded
			w = GETROMW(0100022);
			if( w==GETROMW(0100016) ) {	// if LWAMEM==NXTRTN
				s = GETROMW(0100010);	// LAVAIL
				d = s-len;
				memmove(ROM+d, ROM+s, w-s+1);	// move up, open hole for BPGM
				SETROMW(0100010, GETROMW(0100010)-len);	// adjust pointers
				SETROMW(0100012, GETROMW(0100012)-len);
				SETROMW(0100014, GETROMW(0100014)-len);
				SETROMW(0100016, GETROMW(0100016)-len);
				SETROMW(0100020, GETROMW(0100020)-len);
				w = GETROMW(0100020)+1;
				SETROMW(0101233, w);
				memcpy(ROM+w, pMem+0x0200, len);
				d = w+030;
				s = GETROMW(d);	// old base address in file
				SETROMW(d, w);	// set new base address
				d += 2;
				while( 0177777 != (p=GETROMW(d)) ) {
					SETROMW(d, p+(w-s));	// adjust ptrs
					d += 2;
				}

				return;
			}
		}
		SystemMessage("Sorry, bad time to load BPGM");
	} else if( t==3 ) {	// ASCII TEXT file to GET as BASIC program
		pB = pMem;
		while( (DWORD)(pB-pMem) < dw ) {
			while( (DWORD)(pB-pMem) < dw  &&  (*pB==13 || *pB==10 || *pB=='\t' || *pB==' ') ) ++pB;
			pS = pB;
			s = 0;
			while( (DWORD)(pB-pMem) < dw  &&  *pB!=13  &&  *pB!=10 ) {
				IO_KEYCOD = *pB++;
				IO_KEYSTS |= 2;	// set 'key pressed' flag
				Int85 |= INT_KEY;
				do {
					Exec85(500);
					w = *((WORD *)(Reg+4));
				} while( Int85 || (w!=0102434 && (w<072 || w>=0111)) );
				++s;
			}
			if( s>95 ) { // if chars output is longer than possible line length
				IO_KEYCOD = 161; // up cursor
				IO_KEYSTS |= 2;	// set 'key pressed' flag
				Int85 |= INT_KEY;
				do {
					Exec85(500);
					w = *((WORD *)(Reg+4));
				} while( Int85 || (w!=0102434 && (w<072 || w>=0111)) );
			}
			if( s ) { // if something output
				IO_KEYCOD = 154;  // END LINE
				IO_KEYSTS |= 2;	// set 'key pressed' flag
				Int85 |= INT_KEY;
				do {
					Exec85(500);
					w = *((WORD *)(Reg+4));
				} while( Int85 || (w!=0102434 && (w<072 || w>=0111)) );
			}
		}
	}
	free(pMem);
}
*/

//*******************************************************
void RunHP85() {

	DWORD	t;

	t = KGetTicks()-HaltTime;
	Halt85 = FALSE;
	AdjustTime(t);
	StartTime += t;	// adjust machine "running time" (ie, advance what time we started) by elapsed time since we halted
}

//*******************************************************
void HaltHP85() {

	Halt85 = TRUE;
	HaltTime = KGetTicks();	// preserve at what time the machine was halted
}

//*******************************************************
void FormatReg(char *pBuf, BYTE r) {

	if( r==0xFF ) strcpy(pBuf, "r#");
	else if( r==0x01 ) strcpy(pBuf, "r*");
	else if( base==8 ) sprintf(pBuf, "r%o", r);
	else if( base==10) sprintf(pBuf, "r%d", r);
	else if( base==16) sprintf(pBuf, "r%X", r);
}

//*********************************************************
void FormatAddress(char *pBuf, WORD addr) {

	DISASMREC	*ptmp;

	TargetAddress = (DWORD)addr;
	if( NULL==(ptmp=FindRecAddress((addr<<16)|0x0000FFFF, ROM[060000])) || ptmp->label[0]==0 ) {
		if( base==8 )	   sprintf(pBuf, "0%6.6o", addr);
		else if( base==10 ) sprintf(pBuf, "%5.5d", addr);
		else if( base==16 ) sprintf(pBuf, "0x%4.4X", addr);
	} else strcpy(pBuf, (char *)ptmp->label);
}

//*********************************************************
// NxtByt is offset into ROM[] of what to disassemble
//	(actually, it's ((offset into ROM[])<<16) | c) where 'c' is
//	0x0000FFFF if we're pointing to actual ROM code, and some
//	value from 0x00000000 to 0x0000FFFE if we're pointing to a
//	comment DISASMREC that PRECEDES acutal ROM code.
// pBuf is pointer to output buffer
// maxlen is max # of bytes output to pBuf (including terminating 0)
void DisAsm(WORD maxlen) {

	BYTE	cod1, cod2, cod3, ARPDRP1, ARPDRP2, AD1, AD2;
//	BOOL	isDRP, isARP;
	char	arpstr[6], drpstr[6], addstr[16], *savpBuf, *pForm, *pBuf;
	DWORD	i, k, s, m, w;

	pBuf = TraceOutbuf;

	TargetAddress = 0xFFFFFFFF;

	if( base==8 ) pForm = "%6.6o  %3.3o";
	else if( base==10 ) pForm = " %5.5d  %3.3d";
	else pForm = " %4.4X   %2.2X";

	k = HP85ADDR(NxtByt);
	sprintf(pBuf, pForm, k, ROM[k]);
	k = strlen(pBuf);
	pBuf += k;
	while( k++<24 ) *pBuf++ = ' ';
	s = NxtRom = HP85ADDR(NxtByt);	// just 0-0xFFFF address in ROM (disregarding full-line comments)

	savpBuf = pBuf;
	// try to find DISASMREC for this "line" (NxtByt address) of code

	if( NextRec==NULL || (HP85ADDR(NextRec->address)/8192) != NxtRom/8192 ) NextRec = pDis[NxtRom/8192][0];
	while( NextRec != NULL  &&  NextRec->address < NxtByt ) {
		DisTyp = NextRec->type;
		NextRec = NextRec->pNext;
	}

	if( NextRec!=NULL							// if found a record
	 && NextRec->address > NxtByt				// but it's HIGHER than the one we're looking for
	 && (NxtByt & 0x0000FFFF)!=0x0000FFFF ) {	// and the one we're looking for is a full-line comment
		if( NextRec->address <= (NxtByt | 0x0000FFFF) ) {	// if record is for next full-line comment OR NxtByt's "real" code
			NxtByt = NextRec->address;							// then jump to there
		} else NxtByt |= 0x0000FFFF;						// else jump NxtByt to "real" code
	}
	saveNxtByt = NxtByt;

	// check for one byte in between tables of DEFs, and force COD type so next table comes out
	// (this is rare, but when it occurs, it can be ugly in the disassembly)
	if( NextRec!=NULL								// if found a DISASMREC after NxtByt
	 && (NextRec->address & 0x0000FFFF)==0x0000FFFF	// and it points to "real" code
	 && (NxtByt & 0x0000FFFF)==0x0000FFFF			// and NxtByt points to "real" code
	 && NextRec->address==(NxtByt+0x00010000)		// and the following DISASMREC "real" code is 1 byte after the NxtByt "real" code
	 && DisTyp==REC_DEF								// and the previous code was a DEF address
	 && NextRec->type!=REC_DEF )					// and the next code is a DEF address
		DisTyp = REC_COD;					// then use REC_COD for this one byte, not REC_DEF, to keep the following table aligned

	if( NextRec!=NULL && NextRec->address==NxtByt ) {
// we got a DISASMREC for this line of code
		CurRec = NextRec;
		if( (NxtByt & 0x0000FFFF)!=0x0000FFFF ) {	// not "real" code, just a full-line comment
			++NxtByt;		// so we move to next line next time
			// erase the address/object code that was output above
			for(pBuf=TraceOutbuf, k=0; k<24; k++) *pBuf++ = ' ';

			*pBuf++ = '!';	// output full-line comment
			*pBuf++ = ' ';
			if( NextRec->Comment ) strcpy(pBuf, NextRec->Comment);
			else *pBuf = 0;
			TraceOutbuf[maxlen] = 0;
			return;			// all done with this line
		}
		strcpy(pBuf, (char *)NextRec->label);
		for(cod1=0; cod1<9; cod1++, pBuf++) if( *pBuf==0 ) break;
		for(; cod1<9; cod1++) *pBuf++ = ' ';
		*pBuf = 0;
		DisTyp = NextRec->type;
		// if label (not blanks), set DRP/ARP to undefined
		if( NextRec->label[0]!=0 ) DisDrp = DisArp = 0xFF;
		DisDataOverride = NextRec->datover;
	} else {
// no DISASMREC found, output blanks for label
		CurRec = NULL;
		strcpy(pBuf, "         ");
		pBuf += 9;
		DisDataOverride = 0;
	}
	// output "code", based upon disassembly type
	switch( DisTyp ) {
	 case REC_ASC:	// ASCII string of "datover" len (max 8 chars)
		if( (i=DisDataOverride)==0 ) i = 1;
		strcpy(pBuf, "ASC  \"");
		pBuf += strlen(pBuf);
		while( i-- && pBuf<(savpBuf+maxlen-3) ) *pBuf++ = ROM[NxtRom++];
		*pBuf++ = '"';
		*pBuf = 0;
		break;
	 case REC_ASP:	// ASCII string with parity (most-significant) bit set on last character
		strcpy(pBuf, "ASP  \"");
		pBuf += strlen(pBuf);
		while( !(ROM[NxtRom] & 128) && pBuf<(savpBuf+maxlen-3) ) {
			*pBuf++ = ROM[NxtRom++];
		}
		if( (ROM[NxtRom] & 0x7F)<32 ) *pBuf++ = ' ';
		else *pBuf++ = ROM[NxtRom] & 0x7F;
		++NxtRom;
		*pBuf++ = '"';
		*pBuf = 0;
		break;
	 case REC_DEF:	// DEF statement to define an address (as in a table)
		FormatAddress(addstr, (WORD)(((WORD)ROM[NxtRom]) + (((WORD)ROM[NxtRom+1])<<8)));
		sprintf(pBuf, "DEF  %s", addstr);
		++NxtRom;
		++NxtRom;
		break;
	 case REC_DRP:	// stand-alone DRP or ARP
//		isARP = FALSE;
//		isDRP = FALSE;
		cod1 = ROM[NxtRom];
		AD1 = cod1 & 0xC0;
		ARPDRP1 = cod1 & 0x3F;
		goto disarpdrp;
	 case REC_COD:	// "normal" assembly code (CPU instructions)
//		isARP = FALSE;
//		isDRP = FALSE;
		cod1 = ROM[NxtRom];
		AD1 = cod1 & 0xC0;
		ARPDRP1 = cod1 & 0x3F;
		cod2 = ROM[NxtRom+1];
		AD2 = cod2 & 0xC0;
		ARPDRP2 = cod2 & 0x3F;
		cod3 = ROM[NxtRom+2];
		if( AD1 & 0x80 ) {	// no ARP or DRP
			if( cod1==0xC6 ) goto jsbinx;
			else if( cod1==0xCE ) {	// JSB =
				FormatAddress(addstr, (WORD)((WORD)cod2+(((WORD)cod3)<<8)));
				sprintf(pBuf, "%s =%s", opc[cod1], addstr);
				++NxtRom;
				++NxtRom;
				++NxtRom;
				DisArp = DisDrp = 0xFF;
			} else if( ((cod1 & 0xFC)==0x94)
			 || ((cod1 & 0xF8)==0xA0)
			 || ((cod1 & 0xFC)==0xAC)
			 || ((cod1 & 0xFC)==0xB4)
			 || ((cod1 & 0xFC)==0xBC)
			 || ((cod1 & 0xF8)==0xC0)
			 || ((cod1 & 0xF8)==0xD8)
			 || ((cod1 & 0xF0)==0xE0) ) goto regreg;
			else if( ((cod1 & 0xF8)==0x80)
				  || ((cod1 & 0xFC)==0x88)
				  || ((cod1 & 0xFC)==0x8C)
				  || ((cod1 & 0xFC)==0x90) ) goto reg;
			else if( (cod1 & 0xF0)==0xF0 ) {	// jmp opcodes (JNZ, JNC, etc)
				FormatAddress(addstr, (WORD)(NxtRom+2+(WORD)((int)((char)cod2))));
				sprintf(pBuf, "%s %s", opc[cod1], addstr);
				++NxtRom;
				++NxtRom;
			} else if( (cod1 & 0xF8)==0x98 ) {	// single byte opcodes (BIN,BCD,SAD,DCE,ICE,CLE,RTN,PAD)
				sprintf(pBuf, "%s", opc[cod1]);
				++NxtRom;
				if( cod1==0x9F ) DisArp = DisDrp = 0xFF;	// if PAD, ARP/DRP are unknown
			} else if( ((cod1 & 0xFC)==0xA8) || ((cod1 & 0xF8)==0xC8) ) goto regimm;
			else if( ((cod1 & 0xFC)==0xB0)
				  || ((cod1 & 0xFC)==0xB8)
				  || ((cod1 & 0xF8)==0xD0) ) goto regdir;
			else goto unknown;
		} else if( !(AD1 & 0x80) && !(AD2 & 0x80)	// first 2 bytes are ARP or DRP
		 && ((AD1==0x40 && AD2==0x00) || (AD1==0x00 && AD2==0x40))
		 && (((cod3 & 0xFC)==0x94)
		  || ((cod3 & 0xF8)==0xA0)
		  || ((cod3 & 0xFC)==0xAC)
		  || ((cod3 & 0xFC)==0xB4)
		  || ((cod3 & 0xFC)==0xBC)
		  || ((cod3 & 0xF8)==0xC0)
		  || ((cod3 & 0xF8)==0xD8)
		  || ((cod3 & 0xF0)==0xE0)) ) {
			if( AD1==0x40 ) {
				DisDrp = ARPDRP1;
				DisArp = ARPDRP2;
			} else {
				DisDrp = ARPDRP2;
				DisArp = ARPDRP1;
			}
			++NxtRom;
			++NxtRom;
			cod1 = cod3;
regreg:
			strcpy(pBuf, opc[cod1]);
			FormatReg(drpstr, DisDrp);
			FormatReg(arpstr, DisArp);
			++NxtRom;
			if( (cod1 & 0xF0)==0xE0 ) {
				if( cod1 & 0x02 ) sprintf(pBuf, "%s %s,-%s", opc[cod1], drpstr, arpstr);
				else sprintf(pBuf, "%s %s,+%s", opc[cod1], drpstr, arpstr);
			} else if( (cod1 & 0xFC)==0xB4 || (cod1 & 0xFC)==0xBC ) {
				arpstr[0] = 'X';
				FormatAddress(addstr, (WORD)((WORD)ROM[NxtRom]+(((WORD)ROM[NxtRom+1])<<8)));
				++NxtRom;
				++NxtRom;
				sprintf(pBuf, "%s %s,%s,%s", opc[cod1], drpstr, arpstr, addstr);
			} else sprintf(pBuf, "%s %s,%s", opc[cod1], drpstr, arpstr);
		} else if( (!(AD1 & 0x80) && !(AD2 & 0x80)) || (AD1==0x40 && cod2==0xCE) ) {	// stand-alone ARP or DRP
disarpdrp:
			if( AD1==0x40 ) DisDrp = ARPDRP1;
			else DisArp = ARPDRP1;
			FormatReg(drpstr, ARPDRP1);
			sprintf(pBuf, "%s %s", opc[cod1], drpstr);
			++NxtRom;
		} else if( AD1==0x00 && cod2==0xC6 ) {	// jsb xREG,ADDR
			DisArp = ARPDRP1;
			cod1 = cod2;
			cod2 = cod3;
			++NxtRom;
jsbinx:
			++NxtRom;
			++NxtRom;
			FormatReg(arpstr, DisArp);
			arpstr[0] = 'X';
			FormatAddress(addstr, (WORD)(((WORD)cod2+(((WORD)ROM[NxtRom])<<8))));
			sprintf(pBuf, "%s %s,%s", opc[cod1], arpstr, addstr);
			++NxtRom;
			DisArp = DisDrp = 0xFF;
		} else if( AD1==0x00
		 && (((cod2 & 0xFC)==0x94)
		  || ((cod2 & 0xF8)==0xA0)
		  || ((cod2 & 0xFC)==0xAC)
		  || ((cod2 & 0xFC)==0xB4)
		  || ((cod2 & 0xFC)==0xBC)
		  || ((cod2 & 0xF8)==0xC0)
		  || ((cod2 & 0xF8)==0xD8)
		  || ((cod2 & 0xF0)==0xE0)) ) {
			DisArp = ARPDRP1;
			++NxtRom;
			cod1 = cod2;
			goto regreg;
		} else if( AD1==0x40
		 && (((cod2 & 0xFC)==0x94)
		  || ((cod2 & 0xF8)==0xA0)
		  || ((cod2 & 0xFC)==0xAC)
		  || ((cod2 & 0xFC)==0xB4)
		  || ((cod2 & 0xFC)==0xBC)
		  || ((cod2 & 0xF8)==0xC0)
		  || ((cod2 & 0xF8)==0xD8)
		  || ((cod2 & 0xF0)==0xE0)) ) {
			DisDrp = ARPDRP1;
			++NxtRom;
			cod1 = cod2;
			goto regreg;
		} else if( AD1==0x40
		 && (((cod2 & 0xF8)==0x80)
		  || ((cod2 & 0xFC)==0x88)
		  || ((cod2 & 0xFC)==0x8C)
		  || ((cod2 & 0xFC)==0x90)) ) {		// single register opcodes (ORB, TSB, etc)
			DisDrp = ARPDRP1;
			++NxtRom;
			cod1 = cod2;
reg:
			FormatReg(drpstr, DisDrp);
			sprintf(pBuf, "%s %s", opc[cod1], drpstr);
			++NxtRom;
		} else if( AD1==0x40
		 && (((cod2 & 0xFC)==0xA8)
		  || ((cod2 & 0xF8)==0xC8)) ) {	// literal immediate (LDB=, CMB=, etc)
			DisDrp = ARPDRP1;
			++NxtRom;
			cod1 = cod2;
			cod2 = cod3;
regimm:
			++NxtRom;
			FormatReg(drpstr, DisDrp);
			if( (cod1 & 1) && ((DisDataOverride>1) || (DisDrp<32 && !(DisDrp & 1)) || (DisDrp>=32 && (DisDrp & 7)<7)) ) {	// multibyte
				if( DisDataOverride==0 && (DisDrp<32 || (DisDrp>=32 && (DisDrp & 7)==6)) ) {
					FormatAddress(addstr, (WORD)((WORD)cod2+(((WORD)ROM[NxtRom+1])<<8)));
					sprintf(pBuf, "%s %s,=%s", opc[cod1], drpstr, addstr);
					++NxtRom;
					++NxtRom;
				} else {
					sprintf(pBuf, "%s %s,=", opc[cod1], drpstr);
					if( DisDataOverride==0 ) cod2 = DisDrp & 7;
					else cod2 = 8-DisDataOverride;
					if( base==8 ) pForm = "0%o,";
					else if( base==10 ) pForm = "%d,";
					else if( base==16 ) pForm = "0x%X,";
					while( cod2<7 ) {
						pBuf += strlen(pBuf);
						sprintf(pBuf, pForm, (WORD)ROM[NxtRom]);
						++NxtRom;
						++cod2;
					}
					pBuf += strlen(pBuf);
					if( base==8 ) pForm = "0%o";
					else if( base==10 ) pForm = "%d";
					else if( base==16 ) pForm = "0x%X";
					sprintf(pBuf, pForm, (WORD)ROM[NxtRom]);
					++NxtRom;
				}
			} else {			// single byte
				if( base==8 ) pForm = "%s %s,=0%o";
				else if( base==10 ) pForm = "%s %s,=%d";
				else if( base==16 ) pForm = "%s %s,=0x%X";
				sprintf(pBuf, pForm, opc[cod1], drpstr, (WORD)cod2);
				++NxtRom;
			}
		} else if( AD1==0x40
		 && (((cod2 & 0xFC)==0xB0)
		  || ((cod2 & 0xFC)==0xB8)
		  || ((cod2 & 0xF8)==0xD0)) ) {
			DisDrp = ARPDRP1;
			cod1 = cod2;
			cod2 = cod3;
			++NxtRom;
regdir:
			++NxtRom;
			++NxtRom;
			FormatReg(drpstr, DisDrp);
			FormatAddress(addstr, (WORD)((WORD)cod2+(((WORD)ROM[NxtRom])<<8)));
			sprintf(pBuf, "%s %s,=%s", opc[cod1], drpstr, addstr);
			++NxtRom;
		} else {
unknown:
			if( !(cod1 & 0x80) ) goto disarpdrp;
			if( base==8 ) pForm = "BYT  0%o";
			else if( base==10 ) pForm = "BYT  %d";
			else if( base==16 ) pForm = "BYT  0x%X";
			sprintf(pBuf, pForm, cod1);
			++NxtRom;
		}
		break;
	 case REC_RAM:
		if( base==8 ) pForm = "RAM  0%o";
		else if( base==10 ) pForm = "RAM  %d";
		else if( base==16 ) pForm = "RAM  0x%X";
		i = (DisDataOverride==0)?1:DisDataOverride;
		sprintf(pBuf, pForm, i);
		NxtRom += i;
		break;
	 case REC_I_O:
		if( base==8 ) pForm = "I/O  0%o";
		else if( base==10 ) pForm = "I/O  %d";
		else if( base==16 ) pForm = "I/O  0x%X";
		i = (DisDataOverride==0)?1:DisDataOverride;
		sprintf(pBuf, pForm, i);
		NxtRom += i;
		break;
	 case REC_BSZ:
		if( base==8 ) pForm = "BSZ  0%o";
		else if( base==10 ) pForm = "BSZ  %d";
		else if( base==16 ) pForm = "BSZ  0x%X";
		i = (DisDataOverride==0)?1:DisDataOverride;
		sprintf(pBuf, pForm, i);
		NxtRom += i;
		break;
	 case REC_BYT:
	 default:
		if( base==8 ) pForm = "0%o";
		else if( base==10 ) pForm = "%d";
		else if( base==16 ) pForm = "0x%X";
		strcpy(pBuf, "BYT  ");
		pBuf += strlen(pBuf);
		if( (i=DisDataOverride)==0 ) i = 1;
		while( i-- && pBuf<TraceOutbuf+sizeof(TraceOutbuf)-5 ) {
			sprintf(pBuf, pForm, ROM[NxtRom++]);
			pBuf += strlen(pBuf);
			if( i ) *pBuf++ = ',';
		}
		*pBuf = 0;
		break;
	}
	NxtByt = NxtRom<<16;	// leave lower word == 0 in case of full-line comments in front of next "real" code
	// if DISASMREC and comment, output the comment
	if( NextRec!=NULL && NextRec->address==saveNxtByt && NextRec->Comment!=NULL ) {
		k = strlen(TraceOutbuf);
		pBuf = TraceOutbuf + k;
		do {
			*pBuf++ = ' ';
		} while( k++ < 50 );
		*pBuf++ = '!';
		*pBuf++ = ' ';
		strcpy(pBuf, NextRec->Comment);
	}

	m = 4;
	w = 4;
	if( base==8 ) pForm = "%3.3o";
	else if( base==10 ) pForm = "%3.3d";
	else {
		pForm = "%2.2X";
		m = 5;
		w = 3;
	}
	for(k=1; k<min(m, NxtRom-s); k++) {
		sprintf(TraceOutbuf+8+k*w, pForm, ROM[s+k]);
		TraceOutbuf[8+k*w+w-1] = ' ';
	}

	TraceOutbuf[maxlen] = 0;
}

//************************************************************
void Label85(PBMB hBM, int x, int y, int rowH, char *str, DWORD clr) {

	int		cx, cy;
	BYTE	c;
	DWORD	fclr;

	while( *str ) {
		fclr = clr;
		c = *str++;
		if( c & 128 ) {
			if( Model<2 ) {
				LineBMB(hBM, x, y+8, x+7, y+8, clr);
				LineBMB(hBM, x, y+9, x+7, y+9, clr);
			} else {
				fclr = clr ^ 0x00FFFFFF;
				RectBMB(hBM, x, y-2, x+8, y-2+rowH, clr, CLR_NADA);
			}
			c &= 0x7F;
		}
		if( Model<2 ) {
			for(cy=0; cy<8; cy++) {
				for(cx=0; cx<7; cx++) {
					if( HP85font[c][cx] & (0x80>>cy) ) PointBMB(hBM, x+cx+1, y+cy, fclr);
				}
			}
		} else {
			for(cy=0; cy<8; cy++) {
				for(cx=0; cx<7; cx++) {
					if( HP85font[HP87font[c]][cx] & (0x80>>cy) ) {
						PointBMB(hBM, x+cx+1, y+cy, fclr);
						if( cx==0 && (c==29 || c==31) ) PointBMB(hBM, x, y+cy, fclr);
						if( cy==0 && (c==28 || c==31) ) {
							PointBMB(hBM, x+cx+1, y-1, fclr);
							PointBMB(hBM, x+cx+1, y-2, fclr);
						}
					}
				}
			}
		}
		x += 8;
	}
}

//**************************
void DrawDisAsm(PBMB hBM) {

	int			y, x, opsw, opi, opx;//, nops;
	DWORD		i, j, k, maxlen, pc, curblk;
	BYTE		savdistyp=0;

	if( Halt85==FALSE ) return;

	maxlen = 128;

	ClipBMB(hBM, TraceX, TraceY, TraceW, TraceH);

	// erase DisAsm background to BLACK
	RectBMB(hBM, TraceX, TraceY, TraceX+TraceW, TraceY+TraceH, CLR_BLACK, CLR_NADA);
	// draw DisAsm header line
	RectBMB(hBM, TraceX, TraceY, TraceX+TraceW, TraceY+10, CLR_BLUE, CLR_NADA);
	sprintf(TraceOutbuf, "Address Opcodes         Label    Assembly-language");
	k = strlen(TraceOutbuf);
	while( k<maxlen ) TraceOutbuf[k++] = ' ';
	TraceOutbuf[k] = 0;
	Label85(hBM, TraceX+8, TraceY+1, 12, TraceOutbuf, CLR_WHITE);

	x = TraceX+8;
	y = TraceY+12;

	NxtByt = DisStart;

	curblk = -1;
	pc = (DWORD)GETREGW(4);
	TraceLines = TraceH/12-1;
	for(i=0; (int)i<TraceLines; i++) {
		if( curblk != HP85ADDR(NxtByt)/8192 ) {
			curblk = HP85ADDR(NxtByt)/8192;
			NextRec = pDis[curblk][0];
		}

		LineIndex[i] = NxtByt;		// so CurRec gets set correctly and bkps and other highlighting is done right
		DisAsm((WORD)maxlen);
		LineIndex[i] = saveNxtByt;	// ptr to the actual address that was disassembled (in case of full-line comment irregularities)
		LineRecs[i]  = CurRec;

		k = strlen(TraceOutbuf);
		while( k<maxlen ) TraceOutbuf[k++] = ' ';
		TraceOutbuf[k] = 0;

		if( i==0 ) savdistyp = DisTyp;
		j = HP85ADDR(LineIndex[i]);
		j = (Marks[Model][j/8] & (1<<(j&7))) && (LineIndex[i] & 0x0000FFFF)==0x0000FFFF;
		// output this one disassembled line
		Label85BMB(hBM, x-1, y, 12, (BYTE*)TraceOutbuf, maxlen, j?CLR_MARK:CLR_WHITE);

		// if this line is where the PC is pointing, highlight the opcode
		if( (opi = pc - HP85ADDR(LineIndex[i])) >= 0  &&  HP85ADDR(NxtByt) > pc ) {
			if( base==16 ) {
				opsw = 3;
//				nops = 5;
			} else {
				opsw = 4;
//				nops = 4;
			}
			opx = x+8*8+opi*opsw*8-2;
			RectBMB(hBM, opx, y-2, opx+8*opsw-7, y+9, CLR_NADA, CLR_RED);
			if( CursLine==0xFFFFFFFF ) CursLine = i;
		}
		// if this line is where the "cursor" is pointing...
		if( (int)i==CursLine ) {
			// highlight the line
			RectBMB(hBM, x-8, y-3, x+TraceW-8, y+10, CLR_NADA, CLR_YELLOW);
			// save TargetAddress for J-key jumping
			JumpAddress = TargetAddress;
		}
		// if this line has a breakpoint, highlight the address
		if( (LineIndex[i] & 0x0000FFFF)==0x0000FFFF ) {	// if "real" code
			for(j=0; j<NUMBRKPTS; j++) {
				if( BrkPts[Model][j] && BrkPts[Model][j]==HP85ADDR(LineIndex[i]) ) {
					RectBMB(hBM, x-4, y-2, x+6*8+4, y+9, CLR_NADA, CLR_ORANGE);
				}
			}
		}
		y += 12;
	}
	DisEnd = LineIndex[i] = NxtByt;
	DisTyp = savdistyp;
	ClipBMB(hBM, -1, -1, -1, -1);
}

//*********************************************************************
void DrawFlag(PBMB hBM, int x, int y, char *str, int f, DWORD c) {

	char	buf[2];

	Label85(hBM, x+4*(3-strlen(str)), y, 12, str, CLR_REGNAMES);
	buf[0] = f?'1':'0';
	buf[1] = 0;
	Label85(hBM, x+27, y, 12, buf, c);
}

//*************************************************************************
void DrawADE(PBMB hBM, int x, int y, int h, char *str, BYTE f, DWORD c) {

	BYTE	buf[4];

	Label85(hBM, x+4*(3-strlen(str)), y, 12, str, CLR_REGNAMES);
	buf[2] = 0;
	if( base==8 ) {
		buf[1] = '0' + (f%8);
		buf[0] = '0' + (f/8);
	} else if( base==10 ) {
		if( f<10 ) {
			buf[1] = 0;
			buf[0] = '0' + f;
			x += 4;
		} else {
			buf[1] = '0' + (f%10);
			buf[0] = '0' + (f/10);
		}
	} else if( base==16 ) {
		if( f<16 ) {
			buf[1] = 0;
			buf[0] = HexDig[f];
			x += 4;
		} else {
			buf[1] = HexDig[f & 0x0F];
			buf[0] = HexDig[f >> 4];
		}
	}
	Label85(hBM, x+4, y+h, 12, (char*)buf, c);
}

//********************************************************
void DrawWReg(PBMB hBM, int x, int y, char *str, WORD f, DWORD c) {

	char	buf[8];

	Label85(hBM, x, y, 12, str, CLR_REGNAMES);
	x += 8*strlen(str);
	if( base==8 ) sprintf(buf, "0%6.6o", f);
	else if( base==10 ) sprintf(buf, "%d", f);
	else if( base==16 ) sprintf(buf, "0x%4.4X", f);
	Label85(hBM, x-1, y, 12, buf, c);
}

//********************************************************
void DrawCPU(PBMB hBM) {

	int		x, y, regnum, i, RX, RY;
	BYTE	buf[4], rbuf[4];

	if( Halt85==FALSE ) return;

	RX = REGx+77;
	RY = REGy+REGh;
	// Draw black background and white framework of registers
	RectBMB(hBM, RX-REGw, REGy, RX+8*REGw, RY+8*REGh, CLR_REGBACK, CLR_NADA);
	for(i=0; i<10; i++) {
		x = RX-REGw+i*REGw;
		LineBMB(hBM, x, RY-REGh, x, RY+8*REGh, CLR_REGFRAME);
		y = RY-REGh+i*REGh;
		LineBMB(hBM, RX-REGw, y, RX+8*REGw, y, CLR_REGFRAME);
	}
	// Draw column labels 0-7
	buf[1] = 0;
	for(regnum=0; regnum<8; regnum++) {
		x = RX+regnum*REGw+4+8;
		y = RY-REGh+4;
		buf[0] = '0' + regnum;
		Label85(hBM, x, y, 12, (char*)buf, CLR_REGNAMES);
	}

	// Draw register contents and row labels R00-R70
	buf[3] = 0;
	for(regnum=0; regnum<64; regnum++) {
		if( 0 && (Flags & DCM) && regnum>017 && (Reg[regnum]&0x0F)<=0x09 && (Reg[regnum]&0xF0)<=0x90 ) {
/* BCD display

		One thought is to check the state of the BIN/BCD flag and draw registers
		020-077 as BCD digits when in BCD mode.  That is not implemented here.
		The '0' at the above if() causes this branch to never be taken.  If this
		is ever implemented, the "0 && " will be removed and code will be added
		to save 'base' before entering the for(;;;) loop, set base=16 in this if()
		statement, then restore 'base' at exit from the for(;;;) loop.

*/
		}
		x = RX+(regnum % 8)*REGw+4;
		y = RY+(regnum / 8)*REGh+4;
		if( base==8 ) {
			buf[2] = '0' + (Reg[regnum] & 7);
			buf[1] = '0' + ((Reg[regnum]>>3) & 7);
			buf[0] = '0' + ((Reg[regnum]>>6) & 3);
			if( !(regnum & 7) ) {
				rbuf[0] = 'r';
				rbuf[1] = '0' + (regnum/8);
				rbuf[2] = '0';
				rbuf[3] = 0;
				Label85(hBM, RX-REGw+4, y, 12, (char*)rbuf, CLR_REGNAMES);
			}
		} else if( base==10 ) {
			sprintf((char*)buf, "%d", Reg[regnum]);
			if( Reg[regnum]<100 ) x += 4;
			if( Reg[regnum]<10 ) x += 4;
			if( !(regnum & 7) ) {
				sprintf((char*)rbuf, "r%d", regnum);
				Label85(hBM, RX-REGw+4, y, 12, (char*)rbuf, CLR_REGNAMES);
			}
		} else if( base==16 ) {
			buf[2] = 0;
			buf[1] = HexDig[Reg[regnum] & 0x0F];
			buf[0] = HexDig[Reg[regnum]>>4];
			x += 4;
			if( !(regnum & 7) ) {
				rbuf[0] = 'r';
				rbuf[1] = HexDig[regnum/16];
				rbuf[2] = '0';
				rbuf[3] = 0;
				Label85(hBM, RX-REGw+4, y, 12, (char*)rbuf, CLR_REGNAMES);
			}
		}
		Label85(hBM, x, y, 12, (char*)buf, (Reg[regnum]==LastReg[regnum])?CLR_REGNOCHANGE:CLR_REGCHANGE);
	}
	// Draw background and framework for status bits
	x = REGx;
	y = RY;
	RectBMB(hBM, x, y, x+8*5, y+8*REGh, CLR_REGBACK, CLR_REGFRAME);
	LineBMB(hBM, x+REGw, y, x+REGw, y+8*REGh, CLR_REGFRAME);
	for(i=1; i<8; i++) LineBMB(hBM, x, y+i*REGh, x+8*5, y+i*REGh, CLR_REGFRAME);
	// Draw status bits
	x += 4;
	y += 4;
	DrawFlag(hBM, x, y, "MSB", Flags & MSB, ((Flags ^ LastFlags) & MSB)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "LDZ", Flags & LDZ, ((Flags ^ LastFlags) & LDZ)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "DCM", Flags & DCM, ((Flags ^ LastFlags) & DCM)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "CY", Flags & CY, ((Flags ^ LastFlags) & CY)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "OVF", Flags & OVF, ((Flags ^ LastFlags) & OVF)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "Z", Flags & Z, ((Flags ^ LastFlags) & Z)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "RDZ", Flags & RDZ, ((Flags ^ LastFlags) & RDZ)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	y += REGh;
	DrawFlag(hBM, x, y, "LSB", Flags & LSB, ((Flags ^ LastFlags) & LSB)?CLR_REGCHANGE:CLR_REGNOCHANGE);

	// Draw background and framework of DRP,ARP,E,PC,R6 status
	x = REGx;
	y = RY + 17*REGh/2;
	RectBMB(hBM, x, y, x+7*REGw, y+2*REGh, CLR_REGBACK, CLR_REGFRAME);
	LineBMB(hBM, x, y+REGh, x+7*REGw, y+REGh, CLR_REGFRAME);
	for(i=1; i<4; i++) LineBMB(hBM, x+i*REGw, y, x+i*REGw, y+2*REGh, CLR_REGFRAME);
	// Draw DRP, ARP, E, PC, R6 status
	x += 4;
	y += 4;
	DrawADE(hBM, x, y, REGh, "DRP", (BYTE)Drp, (Drp!=LastDrp)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	x += REGw;
	DrawADE(hBM, x, y, REGh, "ARP", (BYTE)Arp, (Arp!=LastArp)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	x += REGw;
	DrawADE(hBM, x, y, REGh, "E", E, (E!=LastE)?CLR_REGCHANGE:CLR_REGNOCHANGE);
	x += REGw;
	DrawWReg(hBM, x, y, "PC=", (WORD)GETREGW(4), GETREGW(4)!=GETLASTREGW(4) ? CLR_REGCHANGE : CLR_REGNOCHANGE);
	DrawWReg(hBM, x, y+REGh, "R6=", (WORD)GETREGW(6), GETREGW(6)!=GETLASTREGW(6)?CLR_REGCHANGE:CLR_REGNOCHANGE);

	// set StepOver for single-stepping over instructions and JSBs
/*
	clr = (DWORD)(*((WORD *)(Reg+4)));
	NxtByt = ((clr)<<16) | 0x0000FFFF;
	NextRec = pDis[clr/8192][0];
	DisDrp = (BYTE)Drp;
	DisArp = (BYTE)Arp;
	DisTyp = REC_COD;

	DisAsm(128);

	StepOver = NxtByt | 0x0000FFFF;	// do this disassembly to get address of next instruction for stepping over a JSB
*/
}

//***************************************************
void ScrollUp(DWORD n) {

	DWORD	i, curblk;

	if( HP85ADDR(LineIndex[0])/4 > TraceLines ) NxtByt = ((HP85ADDR(LineIndex[0]) - 4*TraceLines)<<16) | 0x0000FFFF;
	else NxtByt = 0x0000FFFF;

	curblk = -1;
	for(i=0; i<NUMSI && NxtByt<LineIndex[0]; i++) {
		ScrollIndex[i] = NxtByt;
		if( curblk != HP85ADDR(NxtByt)/8192 ) {
			curblk = HP85ADDR(NxtByt)/8192;
			NextRec = pDis[curblk][0];
		}
		DisAsm(128);
	}
	if( i>=NUMSI ) DisStart = LineIndex[0]-(n<<16);	// VERY rare boundary case fall-back
	else if( n>=i ) DisStart = ScrollIndex[0];
	else DisStart = ScrollIndex[i-n-1];
	DrawDisAsm(KWnds[0].pBM);	// reset all disasm pointers for screen
}

//***************************************************
void ScrollDn(DWORD n) {

	n = min(n, TraceLines);	// don't overscroll

	DisStart = LineIndex[n];
	DrawDisAsm(KWnds[0].pBM);	// reset all disasm pointers for screen
}

//***************************************************
void ForceDisStart() {

	if( Halt85 ) {
		DWORD	pc;

		pc = (((DWORD)(GETREGW(4)))<<16) | 0x0000FFFF;
		if( pc<DisStart || pc>=DisEnd ) DisStart = pc;
		CursLine = 0xFFFFFFFF;
		if( HP85ADDR(DisStart) > (0x00010000-TraceLines) ) DisStart = (0x00010000 - TraceLines)<<16;
	}
}

//********************************************************
void DrawMenu(PBMB hBM) {

	long	i, y;
	DWORD	clr;

	if( Halt85 ) {
		MenuStr = DbgMenuStr;
		MenuCnt = DBGMENUCNT;
	} else {
		MenuStr = RunMenuStr;
		MenuCnt = RUNMENUCNT;
	}

	for(i=0; i<MenuCnt; i++) {
		y = MENUy + i*MENUh;
		clr = (i==CurMenuItem) ? MenuSelBackClr : MenuBackClr;
		RectBMB(hBM, MENUx, y, MENUx+MENUw, y+MENUh, clr, CLR_NADA);

		LineBMB(hBM, MENUx, y+MENUh-1, MENUx, y, MenuHiClr);
		LineBMB(hBM, MENUx, y, MENUx+MENUw-1, y, MenuHiClr);
		LineBMB(hBM, MENUx+MENUw-1, y+1, MENUx+MENUw-1, y+MENUh-1, MenuLoClr);
		LineBMB(hBM, MENUx+MENUw-1, y+MENUh-1, MENUx+1, y+MENUh-1, MenuLoClr);
		clr = (i==CurMenuItem) ? MenuSelTextClr : MenuTextClr;
		Label85BMB(hBM, MENUx+8, y+(MENUh-8)/2, 12, (BYTE*)MenuStr[i], strlen(MenuStr[i]), clr);
	}
}

//********************************************************
void DrawAll() {

	int		c;

	// draw whole window's background
	RectBMB(KWnds[0].pBM, 0, 0, KWnds[0].pBM->physW, KWnds[0].pBM->physH, MainWndClr, CLR_NADA);

	// Draw KEYS
	BltBMB(KWnds[0].pBM, KEYx, KEYy, keyBM, 0, 0, keyBM->physW, keyBM->physH, TRUE);

	// Draw CRT
	DrawCRT(KWnds[0].pBM, CRTx, CRTy);

	// Draw PRINTER
	DrawPrinter();

	// Draw TAPE
	DrawTape(KWnds[0].pBM, TAPEx, TAPEy);

	// Draw RUNLITE
	DrawRunLite();

	// Draw any IO card stuff (peripherals)
	for(c=0; c<16; c++) {
		if( IOcards[c].initProc != NULL ) IOcards[c].initProc(iR_DRAW, Model, c, 0);
	}

	// Draw BRIGHTNESS control
	DrawBrightness(KWnds[0].pBM);

	// Draw CPU state (if Halt85==TRUE)
	DrawCPU(KWnds[0].pBM);

	// Draw trace listing (if Halt85==TRUE)
	DrawDisAsm(KWnds[0].pBM);

	// Draw menu or dialog
	if( ActiveDlg ) SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
	else DrawMenu(KWnds[0].pBM);

	KInvalidateAll();
}

//********************************************
void DrawDebug() {

	// Draw CRT
	DrawCRT(KWnds[0].pBM, CRTx, CRTy);
//	if( CfgFlags & CFG_BIGCRT ) StretchBMB(KWnds[0].pBM, CRTx, CRTy, 512, 384, hp85BM, 0, (IO_CRTCTL & 128) ? 192 : 0, 256, 192);
//	else BltBMB(KWnds[0].pBM, CRTx, CRTy, hp85BM, 0, (IO_CRTCTL & 128) ? 192 : 0, 256, 192, FALSE);

	DrawCPU(KWnds[0].pBM);
	DrawDisAsm(KWnds[0].pBM);
	DrawMenu(KWnds[0].pBM);
	DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
}

//************************************************
BOOL LoadDis(char *fn, DISASMREC **pDR) {

	int		f;
	WORD	cfgrev, tmp;
	DWORD	reccnt;
	DISASMREC	*ptmp1, *ptmp2;
	char	fname[128];

	strcpy(fname, fn);
	strcat(fname, ".dis");
	if( -1==(f=Kopen(fname, O_RDBIN)) ) {
		*pDR++ = NULL;
		*pDR++ = NULL;
		return FALSE;
	}
	Kread(f, (BYTE *)&cfgrev, 2);
	Kread(f, (BYTE *)&reccnt, 4);

	if( reccnt ) {
		if( NULL!=(ptmp1 = (DISASMREC *)malloc(sizeof(DISASMREC))) ) {
			*pDR++ = ptmp1;
			ptmp1->pPrev = NULL;
			ptmp1->pNext = NULL;
			Kread(f, &ptmp1->type, 1);
			Kread(f, ptmp1->label, (cfgrev<3)?7:9);
			Kread(f, (BYTE *)&ptmp1->address, 4);
			Kread(f, (BYTE *)&tmp, 2);
			if( tmp>1 ) {
				ptmp1->Comment = (char *)malloc(tmp);
				Kread(f, (BYTE*)ptmp1->Comment, tmp);
			} else ptmp1->Comment = NULL;

			Kread(f, (BYTE *)&ptmp1->datover, 2);
			ptmp2 = ptmp1;
			for(reccnt--; reccnt; reccnt--) {
				if( NULL != (ptmp1 = (DISASMREC *)malloc(sizeof(DISASMREC))) ) {
					ptmp1->pNext = NULL;
					Kread(f, &ptmp1->type, 1);
					Kread(f, ptmp1->label, (cfgrev<3)?7:9);
					Kread(f, (BYTE *)&ptmp1->address, 4);

					Kread(f, (BYTE *)&tmp, 2);
					if( tmp>1 ) {
						ptmp1->Comment = (char *)malloc(tmp);
						Kread(f, (BYTE*)ptmp1->Comment, tmp);
					} else ptmp1->Comment = NULL;
					Kread(f, (BYTE *)&ptmp1->datover, 2);
					ptmp2->pNext = ptmp1;
					ptmp1->pPrev = ptmp2;

					ptmp2 = ptmp1;
				}
			}
		} else *pDR++ = NULL;
		*pDR = ptmp1;
	} else {
		*pDR++ = NULL;
		*pDR++ = NULL;
	}
	Kclose(f);

	return TRUE;
}

//************************************************
BOOL LoadSYSROM(int sysnum) {

	int		f;
	char	temp[128], fname[32], *suffix;
	BYTE	*pBuf;

	suffix = (Model==1) ? "B" : "";
	sprintf(fname, "%s\\romsys%d%s", SysDir, sysnum, suffix);
	pBuf = ROM + (sysnum-1)*8192;

	if( -1==(f=Kopen(fname, O_RDBIN)) ) {
		sprintf(temp, "Error opening %s.", fname);
        MessageBox(NULL, temp, "Oops!", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}
	Kread(f, pBuf, 8192);
	Kclose(f);

	strcpy(FilePath, fname);
	if( !LoadDis(FilePath, pDis[sysnum-1]) ) {
		strcpy(temp, (char*)FilePath);
		strcat(temp, ".dis");
		StoreDis(temp, NULL, NULL, 0);
		if( !LoadDis(FilePath, pDis[sysnum-1]) ) {
			sprintf(temp, "Error opening %s.", FilePath);
			MessageBox(NULL, temp, "Oops!", MB_OK | MB_ICONEXCLAMATION);
			return FALSE;
		}
	}

	return TRUE;
}

//************************************************
BOOL LoadROM(DWORD romnum) {

	int		f;
	char	temp[128];

	sprintf(FilePath, "%s\\rom%3.3o%s", SysDir, (unsigned int)romnum, Model==0?"":"B");
	if( -1==(f=Kopen((char*)FilePath, O_RDBIN)) ) {
		sprintf(FilePath, "%s\\rom%3.3o", SysDir, (unsigned int)romnum);
		if( -1==(f=Kopen(FilePath, O_RDBIN)) ) {
			sprintf(temp, "Error opening %s.", FilePath);
			MessageBox(NULL, temp, "Oops!", MB_OK | MB_ICONEXCLAMATION);
			return FALSE;
		}
	}
	pROMS[numROMS] = (BYTE *)malloc(8192);
	if( pROMS[numROMS]==NULL ) {
		Kclose(f);
		sprintf(temp, "Error malloc'ing space for ROM# %o", (unsigned int)romnum);
		MessageBox(NULL, temp, "Oops!", MB_OK | MB_ICONEXCLAMATION);
	} else {
		Kread(f, pROMS[numROMS], 8192);
		ROMnums[numROMS] = *pROMS[numROMS];
		Kclose(f);
		LoadDis(FilePath, pDisROM[numROMS]);
		++numROMS;
	}

	return TRUE;
}


//*********************************************************
void SelectROM(DWORD romnum) {

	int		i, j;
	DWORD	curromnum;

	// if already selected in, do nothing
	curromnum = ROM[060000];
	if( curromnum == romnum ) return;

	// UNmap current ROM
	for(j=0; j<numROMS; j++) if( *pROMS[j]==curromnum ) break;
	if( j<numROMS ) {
		// save DISASM ptrs in case of any changes to current ROM
		pDisROM[j][0] = pDis[3][0];
		pDisROM[j][1] = pDis[3][1];
	}
	// try to find requested ROM
	for(i=0; i<numROMS; i++) if( *pROMS[i]==romnum ) break;
	if( i>=numROMS ) {
		// if not found, fill option-ROM space in ROM[] with FF
		memset(ROM+24576, 0xFF, 8192);
		// and unlink any DISASMRECs
		pDis[3][0] = pDis[3][1] = NULL;
	} else {
		// if found, copy requested ROM code into option-ROM space in ROM[]
		memcpy(ROM+24576, pROMS[i], 8192);
		// and link in its DISASMRECs
		pDis[3][0] = pDisROM[i][0];
		pDis[3][1] = pDisROM[i][1];
	}
}

//*********************************************************
BOOL LoadROMS() {

	long	i;

	numROMS = 0;
	DisStart = 0x00000000;

	if( !LoadSYSROM(1) ) return FALSE;
	if( !LoadSYSROM(2) ) return FALSE;
	if( !LoadSYSROM(3) ) return FALSE;

	if( !LoadDis((Model==2)?"roms87\\ram_io" : "roms85\\ram_io", pDis[4]) ) {
		pDis[4][0] = pDis[4][1] = NULL;
//		SystemMessage("Failed to load RAM_IO.DIS");
//		return FALSE;
	}
	pDis[5][0] = pDis[6][0] = pDis[7][0] = pDis[4][0];
	pDis[5][1] = pDis[6][1] = pDis[7][1] = pDis[4][1];

	if( !LoadROM(0) ) {
		SystemMessage("Failed to load ROM000");
		return FALSE;
	}

	for(i=0; i<sizeof(RomList)/sizeof(DWORD); i++) {
		if( RomLoad & (1<<i) ) {
			if( !LoadROM(RomList[i]) ) {
				sprintf(FilePath, "Failed to load ROM%3.3o", (unsigned int)RomList[i]);
				SystemMessage(FilePath);
				return FALSE;
			}
		}
	}
	DisAsmNULL = FALSE;

	memset(&ROM[060000], 0377, 8192);	// set memory to all 1's for OPTION-ROM, since none are mapped in yet
	CurROMnum = 0377;

	memset(Reg, 0, 64);
	E = 0;
	Flags = 0;
	Arp = Drp = 0;

	SETREGW(4, GETROMW(0));	// load pwo/reset vector into PC

	return TRUE;
}

//************************************************
void StoreDis(char *fname, DISASMREC *pF, DISASMREC *pL, DWORD discnt) {

	int			f;
	WORD		cfgrev, len;
	DISASMREC	*pT;
	char		tmpstr[128];

	if( -1==(f=Kopen(fname, O_WRBINNEW)) ) {
		sprintf(tmpstr, "Error updating %s.", fname);
        MessageBox(NULL, tmpstr, "Dang!", MB_OK | MB_ICONEXCLAMATION);
	} else {
		cfgrev = 3;
		Kwrite(f, (BYTE *)&cfgrev, 2);
		Kwrite(f, (BYTE *)&discnt, 4);

		for(pT=pF; pT!=NULL && discnt--; pT=pT->pNext) {
			Kwrite(f, &pT->type, 1);
			Kwrite(f, pT->label, 9);
			Kwrite(f, (BYTE *)&pT->address, 4);
			if( pT->Comment != NULL ) len = strlen(pT->Comment)+1;
			else len = 0;
			Kwrite(f, (BYTE *)&len, 2);
			if( len ) Kwrite(f, (BYTE*)pT->Comment, len);
			Kwrite(f, (BYTE *)&pT->datover, 2);
		}
		Kclose(f);
	}
}

//************************************************
void StoreDisAsmRecs() {

	DISASMREC	*pT;
	DWORD		discnt;
	int			i, j;
	char		fname[32], *suffix;

	if( DisAsmNULL ) return;
	// swap out and back in the current OPTION-ROM to make sure
	// the pDisROM[][] ptrs are up to date for the currently
	// selected ROM.
	discnt = ROM[060000];
	SelectROM(discnt ^ 0x00FF);
	SelectROM(discnt);
	// now write the lower three system ROMs' DISASMRECs to their files

	suffix = (Model==1) ? "B" : "";

	for(j=0; j<3; j++) {
		for(discnt=0, pT=pDis[j][0]; pT!=NULL; pT=pT->pNext, ++discnt) ;
		if( discnt ) {
			sprintf(fname, "%s\\romsys%d%s.dis", SysDir, j+1, suffix);
			StoreDis(fname, pDis[j][0], pDis[j][1], discnt);
		}
	}
	// now write the OPTION-ROMs' DISASMRECs to their files
	for(i=0; i<numROMS; i++) {
		for(discnt=0, pT=pDisROM[i][0]; pT!=NULL; pT=pT->pNext, ++discnt) ;
		if( discnt ) {
			sprintf(fname, "%s\\rom%3.3o%s.dis", SysDir, *pROMS[i], suffix);
			StoreDis(fname, pDisROM[i][0], pDisROM[i][1], discnt);
		}
	}
	// now write the RAM and IO DISASMRECs to their file
	for(discnt=0, pT=pDis[4][0]; pT!=NULL; pT=pT->pNext, ++discnt) ;
	if( discnt ) {
		sprintf(fname, "%s\\ram_io.dis", SysDir);
		StoreDis(fname, pDis[4][0], pDis[4][1], discnt);
	}
}

//************************************************
DISASMREC *AddRec(BYTE typ, char *lbl, DWORD addr, char *rem, WORD dostate) {

	DISASMREC	*ptmp, *psrch;
	int			disblk;
	DWORD		addr16;

	addr16 = HP85ADDR(addr);
	if( NULL!=(ptmp=(DISASMREC *)malloc(sizeof(DISASMREC))) ) {
		disblk = addr16/8192;
		if( pDis[disblk][0]==NULL ) {
			pDis[disblk][0] = pDis[disblk][1] = ptmp;
			ptmp->pNext = ptmp->pPrev = NULL;
			// if RAM_IO disblk, set all FOUR RAM_IO disblks to this first (and last) record
			if( disblk==4 ) pDis[5][0] = pDis[5][1] = pDis[6][0] = pDis[6][1] = pDis[7][0] = pDis[7][1] = pDis[4][0];
		} else {
			for(psrch=pDis[disblk][0]; psrch!=NULL && psrch->address <= addr; psrch = psrch->pNext) ;
			if( psrch==NULL ) {
				pDis[disblk][1]->pNext = ptmp;
				ptmp->pPrev = pDis[disblk][1];
				ptmp->pNext = NULL;
				pDis[disblk][1] = ptmp;
			} else {
				ptmp->pNext = psrch;
				if( psrch->pPrev==NULL ) {
					pDis[disblk][0] = ptmp;
					ptmp->pPrev = NULL;
				} else {
					ptmp->pPrev = psrch->pPrev;
					psrch->pPrev->pNext = ptmp;
				}
				psrch->pPrev = ptmp;
			}
		}
		ptmp->type = typ;
		strcpy((char *)ptmp->label, lbl);
		ptmp->address = addr;
		if( rem != NULL && rem[0]!=0 ) {
			ptmp->Comment = (char *)malloc(strlen(rem)+1);
			if( ptmp->Comment ) strcpy(ptmp->Comment, rem);
		} else ptmp->Comment = NULL;
		ptmp->datover = dostate;
	}
	return ptmp;
}

//************************************************
DISASMREC	*DeleteRec(DISASMREC *pRec) {

	DISASMREC	*ptmp;
	int			i;

	ptmp = pRec->pNext;
	if( pRec->pPrev==NULL ) {
		for(i=0; i<8; i++) if( pDis[i][0]==pRec ) pDis[i][0] = pRec->pNext;
		for(i=0; i<numROMS; i++) if( pDisROM[i][0]==pRec ) pDisROM[i][0] = pRec->pNext;
	} else {
		pRec->pPrev->pNext = pRec->pNext;
	}
	if( pRec->pNext==NULL ) {
		for(i=0; i<8; i++) if( pDis[i][1]==pRec ) pDis[i][1] = pRec->pPrev;
		for(i=0; i<numROMS; i++) if( pDisROM[i][1]==pRec ) pDisROM[i][1] = pRec->pPrev;
	} else {
		pRec->pNext->pPrev = pRec->pPrev;
	}
	if( pRec->Comment != NULL ) free(pRec->Comment);
	free(pRec);
	return ptmp;
}

//************************************************
DISASMREC	*FindRecAddress(DWORD addr, DWORD optrom) {

	DISASMREC	*psrch;
	int			i;
	DWORD		addr16;

	addr16 = HP85ADDR(addr);
	if( addr16>060000 && addr16<0100000 ) {
		for(i=0; i<numROMS; i++) if( *pROMS[i]==optrom )  break;
		if( i>=numROMS ) i = 0;	// use ROM 0 as default if no ROM is mapped in
		else {	// if it's a ROM mapped in and we found it in the table of ROMs
			if( ROM[060000]==optrom ) {	// if it's the current ROM, make sure pDisROM[] is up to date in case edits have occured
				pDisROM[i][0] = pDis[3][0];
				pDisROM[i][1] = pDis[3][1];
			}
		}
		// now load the proper
		psrch = pDisROM[i][0];
	} else psrch = pDis[addr16/8192][0];

	for(; psrch && psrch->address < addr; psrch = psrch->pNext) ;
	if( psrch && psrch->address != addr ) psrch = NULL;
	return psrch;
}

//*********************************************************
BOOL AddFullLineComment() {

	DWORD		CLaddr, NLaddr, romnum;
	DISASMREC	*psrch;

	romnum = ROM[060000];
	CLaddr = LineIndex[CursLine];

	if( (psrch=FindRecAddress(CLaddr, romnum))==NULL || (psrch->address & 0x0000FFFF)==0x0000FFFF ) {
		// CursLine must be "real" code
		NLaddr = CLaddr & 0xFFFF0000;	// "address" of first full-line comment before this "real" code
		if( (psrch=FindRecAddress(NLaddr, romnum)) ) {
			// there is at least one full-line comment in front of this "real" code, find LAST one
			while( psrch!=NULL												// stop if reach end of DISASMRECs
				&& (psrch->pNext->address & 0x0000FFFF) != 0x0000FFFF		// stop if last full-line comment before this "real" code
				&& HP85ADDR(psrch->pNext->address)==HP85ADDR(CLaddr) ) {	// stop if last full-line comment before other different address (shouldn't happen, but...)
					psrch = psrch->pNext;
			}
			NLaddr = psrch->address + 1;
			if( HP85ADDR(NLaddr) != HP85ADDR(CLaddr) ) return FALSE;		// not likely, but just in case some idiot puts 64K full-line comments all together... :-)
		}
	} else { // DISASMREC points to full-line comment, insert new one in front of it
		// renumber full-line comments from here down to "real" code, open "numbering hole"
		NLaddr = psrch->address;	// address/full-line-comment-number for new full-line comment
		while( (psrch->address & 0x0000FFFF) != 0x0000FFFF  &&  HP85ADDR(psrch->address) == HP85ADDR(CLaddr) ) {
			++psrch->address;	// open hole for new full-line comment
			psrch = psrch->pNext;	// move up until out of full-line comments in this block
		}
	}
	AddRec(REC_REM, "", NLaddr, NULL, 0);
	DrawDisAsm(KWnds[0].pBM);
	return TRUE;
}

//**************************************************************
// EVERY write to HP-85 address space, whether RAM, ROM, or I/O,
// goes through this routine, byte by byte.
//
void StoreMem(WORD addr, BYTE val) {

	FixCRTbytes(addr);

	if( addr>=0177400 ) {	// I/O
		if( addr!=0177500 ) {//addr<0177500 || addr>0177537 ) {//|| addr==0177530 || addr==0177531 ) {
			ioProc[addr-(WORD)0177400](addr, val);
		} else if( addr==0177500 && GINTen() ) {	// ********************** INTRSC ***********************
			// if INTRSC and GLOBAL INTERRUPTS are ENABLED
			IOcardsIntEnabled = TRUE;
		}
	} else if( addr>=0100000 ) {	// *********************** RAM **********************
		ROM[addr] = val;
	} else {						// *********************** ROM ***********************
// There SHOULDN'T be any writes to ROM.  However, in reality, there
// are a few under some circumstances.  To help track down bugs in the
// emulator, this code catches any writes to ROM and throws up a warning
// message, then breaks into debug mode (halts the HP-85 emulation right
// after the instruction that wrote to ROM).  The couple of "valid" cases
// where writes to ROM occur are documented below, checked for, and ignored.

//		char	*pForm;

		if( ROM[060000]==0340 && addr==077777 ) {
			// Service ROM writes to this location during a RAM test.  Ignore it.

		} else if( ROM[060000]==0 && GETREGW(4)==070752 && (addr==0 || addr==041202) ) {
			// LABEL "xxx" without having first done a DISP or PRINT statement
			// causes the LABEL code to do a STBI r22,=P.FLAG at 70747 in ROM 0,
			// without the P.BUFF, P.PTR, P.FLAG, and LINELN ptrs getting set.
			// Thus, the indirect address is 000000, which tries to write to ROM.
			// We ignore it.
			// A similar problem happens if the Service ROM is running, in which
			// case P.BUFF is left pointing at 041202.
			// We ignore it, too.

		} else {
/*
			if( base==8 ) pForm = "Write to ROM! PC=0%6.6o ROM=0%6.6o VAL=0%o";
			else if( base==10 ) pForm = "Write to ROM! PC=%5.5d ROM=%5.5d VAL=%d";
			else if( base==16 ) pForm = "Write to ROM! PC=%4.4X ROM=%4.4X VAL=0x%4.4X";
			sprintf(temp, pForm, GETREGW(4)-1, addr, val);
			MessageBox(NULL, temp, "Say WHAT?", MB_OK);
			HaltHP85();
*/
		}
	}
}

//**************************************************************
// ALL reads from RAM, ROM, and I/O by the HP-85 CPU go through this routine.
//
BYTE LoadMem(WORD addr) {

	BYTE	retval;

	FixCRTbytes(addr);

	if( addr>=0177400 ) {	// I/O
		if( addr>=0177400 && addr!=0177500 ) {//addr<0177500 || addr>0177537 ) {//|| addr==0177530 || addr==0177531 ) {
			retval = (BYTE)ioProc[addr-(WORD)0177400](addr, -1);	// val<0 || val>255 means READ



		} else if( addr==0177500 ) {	// ********************** INTRSC ***********************
			retval = IO_SC;
//			IOcardsIntEnabled = TRUE;
#if DEBUG
	sprintf((char*)ScratBuf, "Reading IO_SC = %d", (int)retval);
	WriteDebug(ScratBuf);
#endif

		} else retval = 0xFF;	// return all 1's for unused I/O space data
	} else {	// ROM/RAM
		// when the HP-85 had only 16 Kbyts of RAM, reads from
		// the remaining RAM address space usually returned all 1's,
		// so that's what we do here.
		if( RamSize<32 && addr>=48*1024 ) retval = 0xFF;	// if only 16 KB RAM
		else retval = ROM[addr];
	}
	return retval;
}

//**************************************************************
void SetZFlags(WORD regnum, BOOL first) {

	if( Reg[regnum] ) Flags &= ~Z;
	if( first ) {
		Flags |= (Reg[regnum] & LSB);
		if( !(Reg[regnum] & 0x0F) ) Flags |= RDZ;
	}
	Flags = (Flags & ~MSB) | (Reg[regnum] & 0x80);
	if( (Reg[regnum] & 0xF0) ) Flags &= ~LDZ;
	else Flags |= LDZ;
}

//**************************************************************
void SetZFlagsR(WORD regnum, BOOL first) {

	if( Reg[regnum] ) Flags &= ~Z;
	if( first ) {
		Flags |= (Reg[regnum] & 0x80);
		if( !(Reg[regnum] & 0xF0) ) Flags |= LDZ;
	}
	Flags = (Flags & ~LSB) | (Reg[regnum] & LSB);
	if( (Reg[regnum] & 0x0F) ) Flags &= ~RDZ;
	else Flags |= RDZ;
}

//**************************************************************
// This adds two bytes together which contain BCD digits, resulting
// in the same sum (hopefully) that a real HP-85 would.
BYTE AddBCD(WORD first, WORD second, BYTE *carry) {

	WORD	sumlfdig, sumrtdig, locCY;

	sumrtdig = AD_BCD[((first<<4) & 0xF0) | (second & 0x0F)];
	if( sumrtdig & 0x10 ) {
		locCY = 1;
		sumrtdig &= 0x0F;
	} else locCY = 0;

	sumrtdig = AD_BCD[(sumrtdig<<4) | (WORD)(*carry)];
	if( sumrtdig & 0x10 ) {
		locCY = 1;
		sumrtdig &= 0x0F;
	}

	sumlfdig = AD_BCD[(first & 0xF0) | ((second>>4) & 0x0F)];
	if( sumlfdig & 0x10 ) {
		*carry = 1;
		sumlfdig &= 0x0F;
	} else *carry = 0;

	sumlfdig = AD_BCD[(sumlfdig<<4) | locCY];
	if( sumlfdig & 0x10 ) {
		*carry = 1;
		sumlfdig &= 0x0F;
	}
	return (BYTE)((sumlfdig<<4) | sumrtdig);
}

//**************************************************************
// This subtracts two bytes which contain BCD digits, resulting
// in the same difference (hopefully) that a real HP-85 would.
// "carry" is really "borrow", and should be 1 if no borrow occured, 0 if a borrow occured.
BYTE SubBCD(WORD first, WORD second, BYTE *carry) {

	WORD	sumlfdig, sumrtdig, locCY;

	sumrtdig = SB_BCD[((first<<4) & 0xF0) | (second & 0x0F)];
	if( sumrtdig & 0xF0 ) {
		locCY = 0;
		sumrtdig &= 0x0F;
	} else locCY = 1;

	sumrtdig = SB_BCD[(sumrtdig<<4) | (1-(WORD)(*carry))];
	if( sumrtdig & 0xF0 ) {
		locCY = 0;
		sumrtdig &= 0x0F;
	}

	sumlfdig = SB_BCD[(first & 0xF0) | ((second>>4) & 0x0F)];
	if( sumlfdig & 0xF0 ) {
		*carry = 0;
		sumlfdig &= 0x0F;
	} else *carry = 1;

	sumlfdig = SB_BCD[(sumlfdig<<4) | (1-locCY)];
	if( sumlfdig & 0xF0 ) {
		*carry = 0;
		sumlfdig &= 0x0F;
	}
	return (BYTE)((sumlfdig<<4) | sumrtdig);
}

//**************************************************************
//**************************************************************
//**************************************************************

// This is the main HP-85 emulation code that executes CPU
// instructions one by one.  'num' is the number of instructions
// to execute before returning to the caller.

//**************************************************************
//**************************************************************
//**************************************************************
void Exec85(WORD num) {

	BYTE	opc, msb2, msb3, msb6, savreg, membyt, cy;
	WORD	wtmp, xtmp, top, bot, arpreg, pc, pR;
	BOOL	first;
	DWORD	dw, ptr, numbytes;

	if( InExec85 ) return;	// avoid being called recursively somehow
	InExec85 = TRUE;

	// The currently mapped ROM may get changed during a debug breakpoint
	// so we check here before resuming execution (or single-stepping) to
	// make sure that the ROM that was selected when execution was halted
	// is mapped back in before resuming execution.
	if( ROM[060000] != CurROMnum ) SelectROM(CurROMnum);

	while( num-- ) {
		Ptr1Cnt = Ptr2Cnt = 0;	// how many bytes have passed when reading or setting PTR1 and PTR2 values
		dw = KGetTicks();
/*		if( IO_KEYDOWN && IO_KEYTIM+1 < dw ) {
			IO_KEYDOWN = FALSE;
			IO_KEYSTS &= ~2;
		}
*/
		CheckTimerStuff(dw);
		if( pKeyMem!=NULL && (Reg[017] & 0300)==0300 ) {
//char ebuf[32];
//sprintf(ebuf, "Reg17=0x%2.2X", Reg[017]);
//SystemMessage(ebuf);
//*(pKeyPtr+50) = 0;
//sprintf(ScratBuf, "R16=%X R17=%X ERNUM#=%d ERRROM=%X ERROM#=%X %s", Reg[016], Reg[017], LoadMem(0100064), LoadMem(0100065), LoadMem(0100066), pKeyPtr);
//MessageBox(NULL, ScratBuf, "Debug", MB_OK);
			pKeyPtr = NULL;	// stop processing keys on ERROR
			free(pKeyMem);
			pKeyMem = NULL;
		}
// check for interrupts
		if( GINTen() && Int85 && !SkipInts ) {	// INTERRUPT!!!!
			if( Int85 & INT_KEY ) {			// keyboard
				wtmp = GETREGW(6);
				StoreMem(wtmp++, Reg[4]);
				StoreMem(wtmp++, Reg[5]);
				SETREGW(6, wtmp);
				SETREGW(4, GETROMW(4));
				KEYINTgranted();
				goto ExitExec85;
			} else if( Int85 & INT_CL0 ) {	// clock 0
				wtmp = GETREGW(6);
				StoreMem(wtmp++, Reg[4]);
				StoreMem(wtmp++, Reg[5]);
				SETREGW(6, wtmp);
				SETREGW(4, GETROMW(8));
				SetClockBase(0, KGetTicks());
				Int85 &= ~INT_CL0;
				goto ExitExec85;
			} else if( Int85 & INT_CL1 ) {	// clock 1
				wtmp = GETREGW(6);
				StoreMem(wtmp++, Reg[4]);
				StoreMem(wtmp++, Reg[5]);
				SETREGW(6, wtmp);
				SETREGW(4, GETROMW(10));
				SetClockBase(1, KGetTicks());
				Int85 &= ~INT_CL1;
				goto ExitExec85;
			} else if( Int85 & INT_CL2 ) {	// clock 2
				wtmp = GETREGW(6);
				StoreMem(wtmp++, Reg[4]);
				StoreMem(wtmp++, Reg[5]);
				SETREGW(6, wtmp);
				SETREGW(4, GETROMW(12));
				Int85 &= ~INT_CL2;
				SetClockBase(2, KGetTicks());
				goto ExitExec85;
			} else if( Int85 & INT_CL3 ) {	// clock 3
				wtmp = GETREGW(6);
				StoreMem(wtmp++, Reg[4]);
				StoreMem(wtmp++, Reg[5]);
				SETREGW(6, wtmp);
				SETREGW(4, GETROMW(14));
				Int85 &= ~INT_CL3;
				SetClockBase(3, KGetTicks());
				goto ExitExec85;
			} else if( (Int85 & INT_IO) && IOcardsIntEnabled ) {	// IO card
#if DEBUG
	sprintf((char*)ScratBuf, "Exec85() Int85/INT_IO IO_SC=%d interrupting", IO_SC);
	WriteDebug(ScratBuf);
#endif
				IOcardsIntEnabled = FALSE;
				SkipInts = 1;	// make sure at least one OPCODE gets executed after each IO interrupt (to allow proper Burst termination)
				wtmp = GETREGW(6);			// get R6 ptr
				StoreMem(wtmp++, Reg[4]);	// push R4-5 Program Counter on R6 stack
				StoreMem(wtmp++, Reg[5]);
				SETREGW(6, wtmp);			// update R6 ptr
				SETREGW(4, GETROMW(16));	// get IRQ20 vector address from the vector table in ROM and place it in R4-5 Program Counter
				Int85 &= ~INT_IO;			// clear the "needs IO interrupt" flag
//				ioGINTDS(0,0);	// global disable interrupts for now
				goto ExitExec85;
			} else Int85 = 0;
		}
		if( SkipInts ) --SkipInts;
		if( GINTen() && !(Int85 & INT_IO) && IO_PendingInt && IOcardsIntEnabled ) {	// if global interrupts enabled and no IO interrupt pending yet and an IO interrupt is pending, slip it into Int85
			int		c;

			// Enable IO cards to interrupt
			for(c=0; c<16; c++) {
				if( IOcards[c].initProc != NULL && (IO_PendingInt & (1<<c)) )  {	// if card is logged in and has a pending interrupt
#if DEBUG
	sprintf((char*)ScratBuf, "Exec85() Enabling SC to interrupt: %d", (int)c);
	WriteDebug(ScratBuf);
#endif
					IOcards[c].initProc(iR_ENINT, Model, c, 0);	// give it a chance
					break;
				}
			}
		}
// else execute next opcode
		opc = LoadMem(*((WORD *)(Reg+4)));	// fetch next opcode
		pR = GETREGW(4)+1;
		SETREGW(4, pR);						// advance the PC
		msb2 = opc & 0xC0;
		if( msb2==0x00 ) {
			if( (opc & 077)==001 ) {
				Arp = Reg[0] & 077;
				Cycles += 3;
			} else {
				Arp = opc & 077;
				Cycles += 2;
			}
		} else if( msb2==0x40 ) {
			if( (opc & 077)==001 ) {
				Drp = Reg[0] & 077;
				Cycles += 3;
			} else {
				Drp = opc & 077;
				Cycles += 2;
			}
		} else {
			msb3 = opc & 0xE0;
			if( msb3<0xA0 ) {
				msb6 = opc & 0xFC;
				if( msb6==0x80 ) {	// EL,ER
shift:
					Cycles += 4;
					if( Flags & DCM ) Flags = DCM | Z;
					first = TRUE;
					if( opc & 0x01 ) {	// MULTI
						bot = (Drp==1)?(Reg[0]&0x3F):Drp;
						if( bot<040 ) top = bot | 1;
						else top = bot | 7;
						if( opc & 0x02 ) {
							top = bot;
							bot = bot & ((bot<040)?0x3E:0x38);
						}
					} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					do {
						++Cycles;
						if( Flags & DCM ) {	// BCDmode
							if( opc & 0x02 ) {	// ER
								wtmp = (((WORD)E)<<8) | ((WORD)Reg[top]);
								E = (BYTE)(wtmp & 0x0F);
								Reg[top] = (BYTE)(wtmp>>4);
								SetZFlagsR(top, first);
								first = FALSE;
								if( top ) --top;
								else bot = top+1;
							} else {	// EL
								wtmp = ((WORD)E) | (((WORD)Reg[bot])<<4);
								E = (BYTE)((wtmp>>8)&0x0F);
								Reg[bot] = (BYTE)wtmp;
								SetZFlags(bot, first);
								first = FALSE;
								++bot;
							}
						} else {	// BINmode
							if( opc & 0x02 ) {	// ER
								wtmp = ((((WORD)Flags) & CY)<<4) | (WORD)Reg[top];
								Flags = (Flags & DCM) | Z | ((wtmp & 1)?CY:0);
								Reg[top] = (BYTE)(wtmp>>1);
								SetZFlagsR(top, first);
								first = FALSE;
								if( top ) --top;
							} else {	// EL
								wtmp = ((((WORD)Flags) & CY)>>4) | (((WORD)Reg[bot])<<1);
								Flags = (Flags & DCM) | Z | ((wtmp & 0x0100)?CY:0);
								if( ((Flags & CY)?1:0) ^ ((wtmp & 0x0080)?1:0) ) Flags |= OVF;
								Reg[bot] = (BYTE)wtmp;
								SetZFlags(bot, first);
								first = FALSE;
								++bot;
							}
						}
					} while( top>=bot );
				} else if( msb6==0x84 ) { // LL,LR
					if( Flags & DCM ) E = 0;
					else Flags &= ~CY;
					goto shift;
				} else if( msb6==0x88 ) { // IC,DC
					if( opc & 0x02 ) {
						msb2 = 1;
						msb6 = 0x04;	// sb
					} else {
						msb2 = 0;	// carry
						msb6 = 0x02;	// ad
					}
					Flags = (Flags & DCM) | Z;
					first = TRUE;
					if( opc & 0x01 ) {	// MULTI
						bot = (Drp==1)?(Reg[0]&0x3F):Drp;
						if( bot<040 ) top = bot | 1;
						else top = bot | 7;
					} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					goto dcic;
				} else if( msb6==0x8C ) { // TC,NC
					Flags = (Flags & DCM) | Z | CY | OVF;
					msb2 = 0;	// carry
					first = TRUE;	// first byte
					if( opc & 0x01 ) {	// MULTI
						bot = (Drp==1)?(Reg[0]&0x3F):Drp;
						if( bot<040 ) top = bot | 1;
						else top = bot | 7;
					} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					Cycles += 4;
					do {
						++Cycles;
						if( Flags & DCM ) {	// BCDmode
							if( opc & 0x02 ) {	// NC
								Flags &= ~(CY | OVF);
								Reg[bot] = (BYTE)NC_BCD[Reg[bot]];
							} else {	// TC
								if( Reg[bot]!=0 ) Flags &= ~CY;
								Flags &= ~OVF;
								if( msb2==0 ) {
									if( Reg[bot]!=0 ) msb2 = 1;
									Reg[bot] = (BYTE)TC_BCD[Reg[bot]];
								} else {
									Reg[bot] = (BYTE)NC_BCD[Reg[bot]];
								}
							}
						} else {	// BINmode
							if( opc & 0x02 ) {	// NC
								Flags &= ~(CY | OVF);
								Reg[bot] = ~Reg[bot];
							} else {	// TC
								if( Reg[bot]!=0 ) Flags &= ~CY;
								if( (top==bot && Reg[bot]!=0x80) || (top!=bot && Reg[bot]!=0) ) Flags &= ~OVF;
								if( msb2==0 ) {
									if( Reg[bot]!=0 ) msb2 = 1;
									Reg[bot] = (~Reg[bot])+1;
								} else {
									Reg[bot] = ~Reg[bot];
								}
							}
						}
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
					} while( top>=bot );
				} else if( msb6==0x90 ) { // TS,CL
					Flags = (Flags & DCM) | Z;
					first = TRUE;	// first byte flag
					if( opc & 0x01 ) {	// MULTI
						bot = (Drp==1)?(Reg[0]&0x3F):Drp;
						if( bot<040 ) top = bot | 1;
						else top = bot | 7;
					} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					Cycles += 4;
					do {
						++Cycles;
						if( opc & 0x02 ) {	// CL
							Reg[bot] = 0;
						} else {	// TS
						}
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
					} while( top>=bot );
				} else if( msb6==0x94 ) { // OR,XR
					Flags = (Flags & DCM) | Z;
					first = TRUE;
					if( opc & 0x01 ) {	// MULTI
						bot = (Drp==1)?(Reg[0]&0x3F):Drp;
						if( bot<040 ) top = bot | 1;
						else top = bot | 7;
					} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					arpreg = (Arp==1)?(Reg[0]&0x3F):Arp;
					Cycles += 4;
					do {
						++Cycles;
						if( opc & 0x02 ) Reg[bot] = Reg[bot] ^ Reg[arpreg];	// XR
						else Reg[bot] = Reg[bot] | Reg[arpreg];	// OR
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
						++arpreg;
					} while( top>=bot );
				} else if( msb6==0x98 ) { // BIN,BCD,SAD,DCE
					if( opc==0x98 ) {	// BIN
						Flags &= ~DCM;
						Cycles += 4;
					} else if( opc==0x99 ) {	// BCD
						Flags |= DCM;
						Cycles += 4;
					} else if( opc==0x9A ) {	// SAD
						msb2 = (BYTE)Arp | ((Flags & OVF)<<4) | ((Flags & CY)<<2);
						msb3 = (BYTE)Drp | ((Flags & OVF)<<4) | ((Flags & DCM)<<1);
						msb6 = (Flags & (MSB | LDZ | Z | RDZ | LSB)) ^ (LDZ | Z | RDZ);
						pR = GETREGW(6);
						StoreMem(pR++, msb2);
						StoreMem(pR++, msb3);
						StoreMem(pR++, msb6);
						SETREGW(6, pR);
						Cycles += 8;
					} else {	// DCE
						E = (E-1) & 0x0F;
						Cycles += 2;
					}
				} else { // ICE,CLE,RTN,PAD
					if( opc==0x9C ) {			// ICE
						E = (E+1) & 0x0F;
						Cycles += 2;
					} else if( opc==0x9D ) {	// CLE
						E = 0;
						Cycles += 2;
					} else if( opc==0x9E ) {	// RTN
						pR = GETREGW(6);
						Reg[5] = LoadMem(--pR);
						Reg[4] = LoadMem(--pR);
						SETREGW(6, pR);
						Cycles += 5;
					} else {					// PAD
						pR = GETREGW(6);
						msb6 = LoadMem(--pR);
						msb3 = LoadMem(--pR);
						msb2 = LoadMem(--pR);
						SETREGW(6, pR);
						Arp = msb2 & 0x3F;
						Drp = msb3 & 0x3F;
						Flags = (msb6 ^ (LDZ | Z | RDZ)) | ((msb2 >>4) & OVF) | ((msb2 >> 2) & CY) | ((msb3 >> 1) & DCM);
						Cycles += 8;
					}
				}
			} else if( msb3==0xA0 ) {	// LD,ST
				Flags = (Flags & DCM) | Z;
				first = TRUE;
				if( opc & 0x01 ) {	// MULTI
					bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					if( bot<040 ) top = bot | 1;
					else top = bot | 7;
				} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
				arpreg = (Arp==1)?(Reg[0]&0x3F):Arp;

				msb3 = opc & 0x1C;
				if( msb3==0x00 ) {	// REG_REG
					Cycles += 4;
					do {
						++Cycles;
						if( opc & 0x02 ) Reg[arpreg] = Reg[bot];	// ST
						else Reg[bot] = Reg[arpreg];	// LD
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
						++arpreg;
					} while( top>=bot );
					// In binary programs, sometimes they did a GTO by loading
					// an address into a register, adding BINTAB to it, decrementing
					// it, then doing a STM of the register into R4.  Other times
					// they did a LDM R4 of the register.  So this code checks
					// both ways and does the appropriate INC R4 after the LD/ST.
					if( (top==5 && !(opc & 0x02)) || (arpreg==6 && (opc & 0x02)) ) {
						pR = GETREGW(4)+1;
						SETREGW(4, pR);
					}
				} else if( msb3==0x04 ) {	// REG_REG_DIR
						wtmp = GETREGW(arpreg);
stldmem:
					Cycles += 5;
					do {
						++Cycles;
						if( opc & 0x02 ) StoreMem(wtmp, Reg[bot]);	// ST
						else Reg[bot] = LoadMem(wtmp);	// LD
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
						if( wtmp<0177400 ) ++wtmp;	// if not IO, bump memory address
					} while( top>=bot );
					// sometimes the 80CPU code did "GTO label", a macro that
					// really did "LDM R4,=label-1".  The '-1' (resulting in
					// an address one byte BEFORE the desired target) was done
					// because the 80CPU would do the final increment of R4 (the PC)
					// AFTER loading the value INTO R4.  The following line of
					// code checks for having done "LDM R4" (top==5), and increments
					// R4 in that case, so that the "GTO label" instructions will work.
					if( top==5 ) {
						pR = GETREGW(4)+1;
						SETREGW(4, pR);
					}
				} else if( msb3==0x08 ) {	// REG_LIT
					wtmp = GETREGW(4);
					pR = GETREGW(4) + (top-bot+1);
					SETREGW(4, pR);
					Cycles += 4;
					do {
						++Cycles;
						if( opc & 0x02 ) StoreMem(wtmp, Reg[bot]);	// ST
						else Reg[bot] = LoadMem(wtmp);	// LD
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
						++wtmp;
					} while( top>=bot );
					// sometimes the 80CPU code did "GTO label", a macro that
					// really did "LDM R4,=label-1".  The '-1' (resulting in
					// an address one byte BEFORE the desired target) was done
					// because the 80CPU would do the final increment of R4 (the PC)
					// AFTER loading the value INTO R4.  The following line of
					// code checks for having done "LDM R4" (top==5), and increments
					// R4 in that case, so that the "GTO label" instructions will work.
					if( top==5 ) {
						pR = GETREGW(4)+1;
						SETREGW(4, pR);
					}
				} else if( msb3==0x0C ) {	// REG_REG_INDIR
					wtmp = GETREGW(arpreg);
					Cycles += 2;
rrindir:
					if( wtmp>=0177710 && wtmp<=0177717 ) {	// PTR1/PTR2 INDIRECT
						Cycles += 5;
//0177710  PTR1
//0177711  PTR1-
//0177712  PTR1+
//0177713  PTR1-+
//0177714  PTR2
//0177715  PTR2-
//0177716  PTR2+
//0177717  PTR2-+
						numbytes = top-bot+1;
						if( wtmp < 0177714 ) {
							if( wtmp & 1 ) Ptr1 -= numbytes; // either PTR1- or PTR1-+
							ptr = Ptr1;
						} else {
							if( wtmp & 1 ) Ptr2 -= numbytes;	// either PTR2- or PTR2-+
							ptr = Ptr2;
						}
						do {
							++Cycles;
							if( opc & 0x02 ) {
								if( ptr > 077777 && ptr<ROMlen ) ROM[ptr] = Reg[bot];	// ST (only if NOT ROM)
							} else if( ptr<ROMlen ) Reg[bot] = ROM[ptr];	// LD
							else Reg[bot] = 0xFF;	// past end of memory, return 1's
							++ptr;
							SetZFlags(bot, first);
							first = FALSE;
							++bot;
						} while( top>=bot );
						if( wtmp < 0177714 ) {
							if( (wtmp & 3) > 1 ) Ptr1 = ptr; // either PTR1+ or PTR1-+
						} else {
							if( (wtmp & 3) > 1 ) Ptr2 = ptr;	// either PTR2+ or PTR2-+
						}
					} else {	// normal CPU memory space
normdir:
						wtmp = GETROMW(wtmp);
						goto stldmem;
					}
				} else if( msb3==0x10 ) {	// REG_ADR_DIR
					wtmp = GETREGW(4)+2;
					SETREGW(4, wtmp);
					wtmp -= 2;
					goto normdir;	// direct no PTR1/2 indirect stuff
				} else if( msb3==0x14 ) {	// REG_REG_ADDR_DIR
					wtmp = GETREGW(4)+2;
					SETREGW(4, wtmp);
					wtmp -= 2;
					wtmp = GETROMW(wtmp) + GETREGW(arpreg);
					SETREGW(2, wtmp);
					Cycles += 2;
					goto stldmem;
				} else if( msb3==0x18 ) {	// REG_ADDR_INDIR
					wtmp = GETREGW(4)+2;
					SETREGW(4, wtmp);
					wtmp -= 2;
					wtmp = GETROMW(wtmp);
					Cycles += 2;
					goto rrindir;
				} else {	// REG_REG_ADDR_INDIR
					wtmp = GETREGW(4)+2;
					SETREGW(4, wtmp);
					wtmp -= 2;
					wtmp = GETROMW(wtmp) + GETREGW(arpreg);
					SETREGW(2, wtmp);
					Cycles += 4;
					goto rrindir;
				}
			} else if( msb3==0xC0 ) {	// CM,AD,SB,ANM,JSB
				if( opc==0xC6 ) {	// JSBx
					wtmp = GETREGW(4);
					wtmp = GETROMW(wtmp);
					arpreg = (Arp==1)?(Reg[0]&0x3F):Arp;
					wtmp += GETREGW(arpreg);
//					SETREGW(2, wtmp);	NO!!!!! This instruction does NOT modify R2-3
					Cycles += 11;
					goto jsbcom;
				} else if( opc==0xCE ) {	// JSB=
					wtmp = GETREGW(4);
					wtmp = GETROMW(wtmp);
					Cycles += 9;
jsbcom:
					pR = GETREGW(4)+2;
					SETREGW(4, pR);
					pR = GETREGW(6);
					StoreMem(pR++, Reg[4]);
					StoreMem(pR++, Reg[5]);
					SETREGW(6, pR);
					SETREGW(4, wtmp);
				} else if( opc==0xD6 ) {	// opcode 326 - 3 byte NOP
					// this is an "unused" and unsupported opcode in the HP-85 CPU
					// the hardware engineers investigated it for me in the early 1980's
					// and found that, the way the CPU was designed, it resulted in a
					// 3-byte "no operation".  What it does is fetch the following two
					// bytes from memory, but does nothing with them.  So, this code:
					//	BYT	326
					//	BYT skip1
					//	BYT skip2
					//	BYT nextopcode
					// will skip the skip1 and skip2 bytes and then resume execution
					// with the nextopcode byte, with no modifications to the CPU registers,
					// status, or memory, other than the incrementing of the PC (reg 4).
					// Even though there is no need to actually do the memory fetches in
					// the emulator, we do them here for completeness and illustration.
					pR = GETREGW(4);
					wtmp = GETROMW(pR);
					pR += 2;
					SETREGW(4, pR);
					Cycles += 6;
				} else if( opc==0xDE ) {	// opcode 336 - 1 byte NOP
					// this is an "unused" and unsupported opcode in the HP-85 CPU
					// the hardware engineers investigated it for me in the early 1980's
					// and found that, the way the CPU was designed, it resulted in a
					// 1-byte "no operation".  So, this code:
					//	BYT	336
					//	BYT nextopcode
					// fetches the 336 opcode, but does nothing, and then resumes execution
					// with the nextopcode byte, with no modifications to the CPU registers,
					// status, or memory, other than the incrementing of the PC (reg 4).
					Cycles += 6;
					BrkPts[Model][NUMBRKPTS] = *((WORD *)(Reg+4));	// set breakpoint after this dummy
				} else {	// CM, AD, SB, ANM
					msb6 = opc & 0x06;
					msb2 = (msb6==0x00 || msb6==0x04)?1:0;	// carry
					Flags = (Flags & DCM) | Z;
					first = TRUE;
					if( opc & 0x01 ) {	// MULTI
						bot = (Drp==1)?(Reg[0]&0x3F):Drp;
						if( bot<040 ) top = bot | 1;
						else top = bot | 7;
					} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					arpreg = (Arp==1)?(Reg[0]&0x3F):Arp;

					msb3 = opc & 0x18;
					if( msb3==0x10 ) {	// xxxD Rx,=ADDR
						arpreg = GETREGW(4)+2;
						SETREGW(4, arpreg);
						arpreg -= 2;
						arpreg = GETROMW(arpreg);
						++Cycles;	// one extra cycle
					} else if( msb3==0x18 ) {	// xxxD Rx,Rx
						arpreg = GETREGW(arpreg);
						++Cycles;	// one extra cycle for R/R-direct
					}
dcic:
					Cycles += 4;
					if( (opc & 0x1E)==0x0A && bot==4 && top==5 ) {	// if ADM R4,=immediate
						arpreg = GETREGW(4);
						xtmp = LoadMem(arpreg++);
						xtmp = (LoadMem(arpreg++)<<8) | xtmp;
						arpreg += xtmp;
						SETREGW(4, arpreg);
						goto mod4;
					}
					do {
						++Cycles;
						if( (opc & 0xFC)==0x88 ) {	// DC/IC
							membyt = first?1:0;
						} else {
							msb3 = opc & 0x18;
							if( msb3==0x00 ) {	// REG_REG
								membyt = Reg[arpreg];
								++arpreg;
							} else if( msb3==0x08 ) {	// REG_LIT
								arpreg = GETREGW(4);
								membyt = LoadMem(arpreg++);
								SETREGW(4, arpreg);
								--arpreg;
							} else {	// REG_ADR_DIR && REG_REG_DIR
								membyt = LoadMem(arpreg);
								if( arpreg<0177400 ) ++arpreg;
							}
						}
						if( Flags & DCM ) {	// BCDmode
							if( msb6==0x00 ) {	// CM
								savreg = Reg[bot];
								Reg[bot] = SubBCD((WORD)Reg[bot], (WORD)membyt, &msb2);
								SetZFlags(bot, first);
								Reg[bot] = savreg;
								goto skipflags;
							} else if( msb6==0x02 ) {	// AD
								Reg[bot] = AddBCD((WORD)Reg[bot], (WORD)membyt, &msb2);
							} else if( msb6==0x04 ) {	// SB
								Reg[bot] = SubBCD((WORD)Reg[bot], (WORD)membyt, &msb2);
								SetZFlags(bot, first);
							} else {	// ANM
								Reg[bot] &= membyt;
							}
						} else {	// BINmode
							if( msb6==0x00 ) {	// CM

// NOTE: See the large comment block in front of the SB instruction below, as it applies
// equally well to CM (since CM does a SUBTRACT to set the flags, it just doesn't store the result).

								msb3 = Reg[bot];
#if 0
// OLD, BROKEN version
								wtmp = (WORD)msb3+(WORD)(BYTE)((~membyt))+(WORD)msb2;
								msb2 = (wtmp<256) ? 0 : 1;	// set resulting CY (borrow)
								if( (msb3 & 0x80) && !(wtmp & 0x80) ) Flags |= OVF;
								else Flags &= ~OVF;
#else
// NEW, IMPROVED version
								wtmp = (WORD)msb3-(WORD)(BYTE)((membyt))-1+(WORD)msb2;
								msb2 = (wtmp<256) ? 1 : 0;
								if( bot==top) {	// if final byte of possibly multi-byte operation, check for OVF
									if( ((msb3 ^ membyt)&0x80) && ((wtmp ^ (msb2<<7))&0x80) ) Flags |= OVF;
									else Flags &= ~OVF;
								}
#endif
								savreg = Reg[bot];
								Reg[bot] = (BYTE)wtmp;
								SetZFlags(bot, first);
								Reg[bot] = savreg;
								goto skipflags;
							} else if( msb6==0x02 ) {	// AD
/*
 This whole bloody mess (below) is just to make the OVF flag get set appropriately
 in all the special circumstances.  I would THINK this could be simplified
 somehow, but I tried a LOT of different "methods", and none of them but
 this one came up with the right OVF setting in ALL cases.  Since we're
 really adding THREE things together (the two arguments and a possible carry)
 we have to check for OVF twice, since it involves the signs of both arguments
 to an add, the CY out, and the sign of the sum.

	On a REAL HP-85, I wrote an assembly language function that would
	take two 1-byte values, add them together, and return the resulting
	state of the CY and OVF flags.  I then tested all 16 possible combinations
	of adding 2 values from the list of {-128, -1, 1, 127}, and got these
	results (where MSA=Most Significant bit of A, etc):
	VAL A    VAL B	  SUM      SUMBYT   MSA MSB MSS  CY OVF
	-------  -------  -------  -------  --- --- --- --- ---
	DEC	OCT  DEC OCT  DEC OCT  DEC OCT
	127	177	 127 177  254 376   -2 376   0   0   1   0   1
	127	177	-128 200   -1 377   -1 377   0   1   1   0   0
	127	177	   1 001  128 200 -128 200   0   0   1   0   1
	127	177	  -1 377  126 576  126 176   0   1   0   1   0
   -128	200	 127 177   -1 377   -1 377   1   0   1   0   0
   -128	200	-128 200 -256 400    0 000   1   1   0   1   1
   -128	200	   1 001 -127 201 -127 201   1   0   1   0   0
   -128	200	  -1 377 -129 577  127 177   1   1   0   1   1
      1	001	 127 177  128 200 -128 200   0   0   1   0   1
	  1	001	-128 200 -127 201 -127 201   0   1   1   0   0
	  1	001	   1 001	2 002    2 002   0   0   0   0   0
	  1	001	  -1 377	0 400    0 000   0   1   0   0   0
	 -1	377	 127 177  126 576  126 176   1   0   0   1   0
	 -1	377	-128 200 -129 577  127 177   1   1   0   1   1
	 -1	377	   1 001	0 400    0 000   1   0   0   0   0
	 -1	377	  -1 377   -2 776   -2 376   1   1   1   1   0

	MSS is the MSbit of the BYTE sum, ie, SUM MOD 256.  Note that the SUM columns
	are what the sum SHOULD be, were the numbers added as "real" numbers (ie, letting
	the sum expand to WORD size).  The ACTUAL resulting BYTE SUM, done by adding two BYTE
	values, is shown in the SUMBYT column.

	From this table, we can construct a "truth table" for OVF outcome given the
	inputs of MSA, MSB, MSS, and CY (the 'X' cases never occur, so are "don't cares"):

		   \ MSS| 0  0  1  1
			\ CY| 0  1  0  1
			 \  |
			  \ |
			   \|
		MSA MSB +------------
		 0   0  | 0  X  1  X
		 0   1  | 0  0  0  X
		 1   0  | 0  0  0  X
		 1   1  | 0  1  1  0

	From this, we arrive at:
	   OVF = (not(MSA xor MSB)) and (MSS xor CY)
	or in C language parlance:
	   OVF = !(MSA ^ MSB) && (MSS ^ CY)

 According to the available Series 80 CPU documention, OVF gets set
 ONLY when the CPU is in binary mode (DCM=0), and:
	1) the addition of two positive numbers results in a negative number.
	2) the addition of two negative numbers results in a positive number.
	3) when a left-shift changes the sign (MSbit) of the data register.

 In the above OVF equation, the !(MSA ^ MSB) checks the "addition of
 two positive numbers" and the "addition of two negative numbers" portion,
 (ie, it's true when A and B have the same sign), and the (MSS ^ CY) checks
 for the "results in a negative/positive number" portion.

 So, while the following code is extremely complex, it seems to work.
 The code in EL and LL instructions seems to work and is PROBABLY right.
 The code in binary mode SB below is simple, and SEEMS to work, but is
 probably NOT entirely right.  I just haven't found any test cases yet
 for which it fails, nor have I yet done the extensive analysis I did
 for ADB above.
*/

//***** start of code to check for OVF in binary ADB/ADM
// note that we only set OVF on the final byte of a multi-byte operation.
// on all earlier bytes, we simply add together and set/clear CY.

								msb3 = Reg[bot];
								if( bot==top) {	// if final byte of possibly multi-byte operation, check for OVF
									wtmp = (WORD)msb3+(WORD)membyt+(WORD)msb2;
									cy = (wtmp>255) ? 1 : 0;
									if( !((msb3 ^ membyt)&0x80) && ((wtmp ^ (cy<<7))&0x80) ) Flags |= OVF;
									else Flags &= ~OVF;
/*
									msb3 = (BYTE)wtmp;
									wtmp += msb2;

									xtmp = (WORD)msb3+(WORD)msb2;
									cy = (xtmp>255) ? 1 : 0;
									if( !((msb3 ^ msb2)&0x80) && ((xtmp ^ (cy<<7))&0x80) ) Flags |= OVF;
*/
//GEEKDO: QUESTION: should we be CLEARING OVF up above the beginning of the DO loop????  Or maybe here with an ELSE?

								} else wtmp = (WORD)msb3+(WORD)membyt+msb2;
//***** end of OVF code

								msb2 = (wtmp>255) ? 1 : 0;	// set resulting CY

								Reg[bot] = (BYTE)wtmp;
							} else if( msb6==0x04 ) {	// SB
/*
	On a REAL HP-85, I wrote an assembly language function that would
	take two 1-byte values, subtract the 2nd from the 1st, and return the resulting
	state of the CY, OVF, and MSbit (of result) flags.  I then tested all 16 possible combinations
	of adding 2 values from the list of {-128, -1, 1, 127}, and got these
	results (where MSA=Most Significant bit of A, MS=Most Significant bit of B,
	MSD=Most Significant bit of the Difference):
	VAL A    VAL B	  DIF         DIFBYT   MSA MSB MSD  CY OVF
	-------  -------  ----------  -------  --- --- --- --- ---
	DEC	OCT  DEC OCT  DEC    OCT  DEC OCT
	127	177	 127 177    0      0    0   0   0   0   0   1   0
	127	177	-128 200  255    377   -1 377   0   1   1   0   1
	127	177	   1 001  126    176  126 176   0   0   0   1   0
	127	177	  -1 377  128    200 -128 200   0   1   1   0   1
   -128	200	 127 177 -255 177401    1   1   1   0   0   1   1
   -128	200	-128 200    0      0    0   0   1   1   0   1   0
   -128	200	   1 001 -129 177577  127 177   1   0   0   1   1
   -128	200	  -1 377 -127 177601 -127 201   1   1   1   0   0
      1	001	 127 177 -126 177602 -126 202   0   0   1   0   0
	  1	001	-128 200  129    201 -127 201   0   1   1   0   1
	  1	001	   1 001	0    000    0   0   0   0   0   1   0
	  1	001	  -1 377	2      2    2   2   0   1   0   0   0
	 -1	377	 127 177 -128 177600 -128 200   1   0   1   1   0
	 -1	377	-128 200  127    177  127 177   1   1   0   1   0
	 -1	377	   1 001   -2 177776   -2 376   1   0   1   1   0
	 -1	377	  -1 377    0      0    0   0   1   1   0   1   0

	MSD is the MSbit of the BYTE diff, DIFF MOD 256.  Note that the DIF columns
	are what the diff SHOULD be, were the numbers added as "real" numbers (ie, letting
	the diff expand to WORD size).  The ACTUAL resulting BYTE DIFF, done by subtracting two BYTE
	values, is shown in the DIFBYT column.

	From this table, we can construct a "truth table" for OVF outcome given the
	inputs of MSA, MSB, MSS, and CY (the 'X' cases never occur, so are "don't cares"):

		   \ MSD| 0  0  1  1
			\ CY| 0  1  0  1
			 \  |
			  \ |
			   \|
		MSA MSB +------------
		 0   0  | X  0  0  X
		 0   1  | 0  X  1  X
		 1   0  | X  1  X  0
		 1   1  | X  0  0  X

	From this, we arrive at:
	   OVF = ((MSA xor MSB)) and (MSD xor CY)
	or in C language parlance:
	   OVF = (MSA ^ MSB) && (MSS ^ CY)
*/
// OLD CODE
								msb3 = Reg[bot];
#if 0
//***** start of code to check for OVF in binary ADB/ADM
// note that we only set OVF on the final byte of a multi-byte operation.
// on all earlier bytes, we simply add together and set/clear CY.
								wtmp = (WORD)msb3+(WORD)(BYTE)((~membyt))+(WORD)msb2;
								msb2 = (wtmp<256) ? 0 : 1;	// set resulting CY (borrow)
// GEEKDO: is the following right for setting OVF on subtract???
// Run test on real HP-85 and compare results.
								if( (msb3 & 0x80) && !(wtmp & 0x80) ) Flags |= OVF;
								else Flags &= ~OVF;

#else
//***** start of code to check for OVF in binary SBB/SBM
// note that we only set OVF on the final byte of a multi-byte operation.
// on all earlier bytes, we simply subtract and set/clear CY.
								wtmp = (WORD)msb3-(WORD)(BYTE)((membyt))-1+(WORD)msb2;
								msb2 = (wtmp<256) ? 1 : 0;
								if( bot==top) {	// if final byte of possibly multi-byte operation, check for OVF
									if( ((msb3 ^ membyt)&0x80) && ((wtmp ^ (msb2<<7))&0x80) ) Flags |= OVF;
									else Flags &= ~OVF;
/*
									msb3 = (BYTE)wtmp;
									wtmp += msb2;

									xtmp = (WORD)msb3+(WORD)msb2;
									cy = (xtmp>255) ? 1 : 0;
									if( !((msb3 ^ msb2)&0x80) && ((xtmp ^ (cy<<7))&0x80) ) Flags |= OVF;
*/
								}
//								msb2 = (wtmp<256) ? 0 : 1;	// set resulting CY
#endif

								Reg[bot] = (BYTE)wtmp;
							} else {	// ANM
								Reg[bot] &= membyt;
							}
						}
						SetZFlags(bot, first);
skipflags:
						first = FALSE;
						++bot;
					} while( top>=bot );

					if( msb2 ) Flags |= CY;
					else Flags &= ~CY;
mod4:;
				}
			} else if( (opc & 0xF0)==0xE0 ) {	// PO,PU
				Flags = (Flags & DCM) | Z;
				first = TRUE;
				if( opc & 0x01 ) {	// MULTI
					bot = (Drp==1)?(Reg[0]&0x3F):Drp;
					if( bot<040 ) top = bot | 1;
					else top = bot | 7;
				} else top = bot = (Drp==1)?(Reg[0]&0x3F):Drp;
				arpreg = (Arp==1)?(Reg[0]&0x3F):Arp;

				if( opc & 0x08 ) {	// REG_REG_INDIRECT
					if( opc & 0x02 ) {	// -adr
						wtmp = GETREGW(arpreg)-2;
						SETREGW(arpreg, wtmp);	// move the address stack ptr down one entry first
					} else {			// +adr
						wtmp = GETREGW(arpreg);
						pR = wtmp+2;
						SETREGW(arpreg, pR);	// move the address stack ptr up one entry after
					}
					wtmp = GETROMW(wtmp);		// get the indirect address
					Cycles += 2;
					goto pupomem;
				} else {	// REG_REG_DIR
					wtmp = GETREGW(arpreg);
					if( opc & 0x02 ) {	// -adr
						if( wtmp<=0177400 ) {
							wtmp -= (WORD)(top-bot+1);
							SETREGW(arpreg, wtmp);
						} else {
//GEEKDO: remove these 2 lines
/*if( wtmp >= 0177710 && wtmp<=0177717 ) {
			sprintf((char*)ScratBuf, "PU/PO D R,R to I/O %o",wtmp);
	        MessageBox(NULL, (char*)ScratBuf, "Oops!", MB_OK | MB_ICONEXCLAMATION);
}*/
							pR = wtmp-(WORD)(top-bot+1);
							SETREGW(arpreg, pR);
						}
					} else {	// +adr
						pR = GETREGW(arpreg) + (WORD)(top-bot+1);
						SETREGW(arpreg, pR);
					}
pupomem:
					Cycles += 2;
					do {
						++Cycles;
//GEEKDO: remove these 4 lines
/*if( wtmp >= 0177710 && wtmp<=0177717 ) {
			sprintf((char*)ScratBuf, "PU/PO D R,R to I/O %o",wtmp);
	        MessageBox(NULL, (char*)ScratBuf, "Oops!", MB_OK | MB_ICONEXCLAMATION);
}*/
						if( opc & 0x04 ) StoreMem(wtmp, Reg[bot]);	// PU
						else Reg[bot] = LoadMem(wtmp);	// PO
						SetZFlags(bot, first);
						first = FALSE;
						++bot;
						if( wtmp<0177400 ) ++wtmp;
					} while( top>=bot );
				}
			} else {	// Jxx jump conditional
				switch( opc & 0x0F ) {
				 case 0:	// JMP
					msb2 = 1;
					break;
				 case 1:	// JNO (no overflow)
					msb2 = !(Flags & OVF);
					break;
				 case 2:	// JOD (LSB)
					msb2 = (Flags & LSB);
					break;
				 case 3:	// JEV (no LSB)
					msb2 = !(Flags & LSB);
					break;
				 case 4:	// JNG (MSB)
					// takes into consideration overflow (XOR of MSB and OVF)
					msb2 = ((Flags & MSB) ^ ((Flags<<4) & MSB));
					break;
				 case 5:	// JPS (no MSB)
					// takes into consideration overflow (XOR of MSB and OVF)
					msb2 = !((Flags & MSB) ^ ((Flags<<4) & MSB));
					break;
				 case 6:	// JNZ (no Z)
					msb2 = !(Flags & Z);
					break;
				 case 7:	// JZR (Z)
					msb2 = (Flags & Z);
					break;
				 case 8:	// JEN (E)
					msb2 = E;
					break;
				 case 9:	// JEZ !(E)
					msb2 = !E;
					break;
				 case 10:	// JNC (no CY)
					msb2 = !(Flags & CY);
					break;
				 case 11:	// JCY (CY)
					msb2 = (Flags & CY);
					break;
				 case 12:	// JLZ (LDZ)
					msb2 = (Flags & LDZ);
					break;
				 case 13:	// JLN (no LDZ)
					msb2 = !(Flags & LDZ);
					break;
				 case 14:	// JRZ (RDZ)
					msb2 = (Flags & RDZ);
					break;
				 case 15:	// JRN (no RDZ)
					msb2 = !(Flags & RDZ);
					break;
				}
				wtmp = GETREGW(4)+1;
				SETREGW(4, wtmp);
				--wtmp;
				wtmp = GETREGW(4) + (WORD)((int)(char)LoadMem(wtmp));
				if( msb2 ) {
					SETREGW(4, wtmp);
					Cycles += 5;
				} else Cycles += 4;
			}
		}
		pc = GETREGW(4);
		for(wtmp=0; wtmp<NUMBRKPTS+1; wtmp++) {
			if( BrkPts[Model][wtmp] && BrkPts[Model][wtmp]==pc ) {
				BrkPts[Model][NUMBRKPTS] = 0;	// clear step-over-jsb breakpoint
				HaltHP85();
				num = 0;
				if( pKeyMem!=NULL ) {	// if TYPING ASCII file as keys, stop it!
					pKeyPtr = NULL;	// stop processing keys on ERROR
					free(pKeyMem);
					pKeyMem = NULL;
				}
				ForceDisStart();
				DrawDebug();
				break;
			}
		}
		if( Halt85==TRUE && num ) {
			num = 0;
			ForceDisStart();
			DrawDebug();
		}
	}
ExitExec85:
	InExec85 = FALSE;
	CurROMnum = ROM[060000];	// save for restoring when execution resumes
}

//****************************************************
// quick and dirty for now, disassemble all 64K of ROM
// (including whatever option-ROM is currently mapped in)
// and write it to "ROM85.SRC".

// GEEKDO: maybe someday add a dialog to let the user specify
// the ROM# and address range that they want to disassemble
// and a file name they want it written to.
void ExportDisAsm() {

	DWORD	curblk;
	long	hFile;

	if( -1 != (hFile=Kopen("ROM85.SRC", O_WRBINNEW)) ) {
		NxtByt = 0;
		curblk = -1;

		do {
			if( curblk != HP85ADDR(NxtByt)/8192 ) {
				curblk = HP85ADDR(NxtByt)/8192;
				NextRec = pDis[curblk][0];
			}
			DisAsm(128);
			strcat(TraceOutbuf, "\r\n");
			Kwrite(hFile, (BYTE*)TraceOutbuf, strlen(TraceOutbuf));
		} while( NxtByt > 1 );	// disassemble/export full 64Kbyte address space (until wrap around to 0)
		Kclose(hFile);
	}
	DrawDisAsm(KWnds[0].pBM);	// reset all disasm pointers for screen
}

//****************************************
// This function sets up all the variables that control
// where things are located on the dispaly and how big
// they are.
BOOL SetupDispVars85() {

	long	tracebot;

	if( keyBM ) DeleteBMB(keyBM);
	if( tapeBM ) DeleteBMB(tapeBM);
	if( disk525BM ) DeleteBMB(disk525BM);
	if( disk35BM ) DeleteBMB(disk35BM);
	if( disk8BM ) DeleteBMB(disk8BM);
	if( disk5MBBM ) DeleteBMB(disk5MBBM);
	if( rlBM ) DeleteBMB(rlBM);
	keyBM = tapeBM = disk525BM = disk35BM = disk8BM = disk5MBBM = rlBM = NULL;
	KGetWndWH((long int *)&VW, (long int *)&VH);
	if( CfgFlags & CFG_BIGCRT ) {
		keyBM = LoadBMB("images\\85keys2.bmp", 0, 0, 0, 0);
		tapeBM= LoadBMB("images\\tape2.bmp", -1, -1, -1, -1);

		disk5MBBM= LoadBMB("images\\disks8.bmp", 0, 0, 0, 0);
		disk8BM  = LoadBMB("images\\disks6.bmp", 0, 0, 0, 0);
		disk35BM = LoadBMB("images\\disks4.bmp", 0, 0, 0, 0);
		disk525BM= LoadBMB("images\\disks2.bmp", 0, 0, 0, 0);
		rlBM  = LoadBMB("images\\runlite2.bmp", -1,-1,-1,-1);
		MENUx= 2*(2);
		MENUy= 2*(4);
		MENUh= 16;
		MENUw= 302;
		REGx = 2*(2);
		REGy = 1*(MENUy+MENUh*(DBGMENUCNT+1));
		REGw = 28;
		REGh = 14;
		KEYx = 2*(2);
		KEYy = VH - keyBM->physH - 2*(2);
		CRTx = KEYx + 2*(42+2);
		CRTy = KEYy + 2*(2);
		TAPEx= KEYx + keyBM->physW + 2*(6);//CRTx + 512 - tapeBM->physW;
		TAPEy= VH - tapeBM->physH/4 - 2*(2);

		RLw  = 2*40;
		RLh  = 2*16;
		RLx  = (CRTx-RLw)/2;
		RLy  = CRTy;

		BRTw = 2*24;
		BRTh = 2*99;
		BRTx = (CRTx - BRTw)/2;
		BRTy = RLy + RLh + 2*20;

		tracebot = ((KEYy+keyBM->physH)-2*(3*42+4));

		PRTx = CRTx + 2*(256+2+(7*42-272)/2);
		PRTy = tracebot;	// BOTTOM of the paper!!!
		PRTw = 272;	// like hp85BM (for the CRT), the printer bitmap is always the same width
		PRTh = (tracebot-2)/2;	// and it's 1/2 the available height, since it will get stretched to double height and double width

		tracebot -= 4*12;	// let printer show a little

		TraceX = CRTx + 2*(256+2+4);
		TraceW = 8*((VW-TraceX)/8);
		TraceH = 12*((tracebot-2*(2))/12);
		TraceY = 2*(2);
	} else {
		keyBM = LoadBMB("images\\85keys1.bmp", 0, 0, 0, 0);
		tapeBM= LoadBMB("images\\tape1.bmp", -1, -1, -1, -1);
		disk5MBBM= LoadBMB("images\\disks7.bmp", 0, 0, 0, 0);
		disk8BM  = LoadBMB("images\\disks5.bmp", 0, 0, 0, 0);
		disk35BM = LoadBMB("images\\disks3.bmp", 0, 0, 0, 0);
		disk525BM= LoadBMB("images\\disks1.bmp", 0, 0, 0, 0);
		rlBM  = LoadBMB("images\\runlite1.bmp", -1,-1,-1,-1);

		MENUx= 2;
		MENUy= 4;
		MENUh= 16;
		MENUw= 302;
		REGx = 2;
		REGy = MENUy+MENUh*(DBGMENUCNT+1);
		REGw = 28;
		REGh = 14;
		KEYx = 2;
		KEYy = VH - keyBM->physH - 2;
		CRTx = KEYx + (42+2);
		CRTy = KEYy + (2);
		TAPEx= KEYx + keyBM->physW + 6;//CRTx + 512 - tapeBM->physW;
		TAPEy= VH - tapeBM->physH/4 - 2;

		RLw  = 40;
		RLh  = 16;
		RLx  = (CRTx-RLw)/2;
		RLy  = CRTy;

		BRTw = 24;
		BRTh = 99;
		BRTx = (CRTx - BRTw)/2;
		BRTy = RLy + RLh + 20;

		tracebot = ((KEYy+keyBM->physH)-(3*42+4));

		PRTx = CRTx + (256+2+(7*42-272)/2);
		PRTy = tracebot;	// BOTTOM of the paper!!!
		PRTw = 272;
		PRTh = tracebot-2;

		tracebot -= 4*12;	// let printer show a little

		TraceX = CRTx + (256+2+4);
		TraceY = MENUy;
		TraceW = 8*((VW-TraceX)/8);
		TraceH = 12*((tracebot-TraceY)/12);
	}

	return keyBM!=NULL && tapeBM!=NULL && disk525BM!=NULL && disk35BM!=NULL && disk8BM!=NULL && disk5MBBM!=NULL && rlBM!=NULL;
}

//****************************************
// This function sets up all the variables that control
// where things are located on the dispaly and how big
// they are.
BOOL SetupDispVars87() {

	long	tracebot;

	if( keyBM ) DeleteBMB(keyBM);
	if( tapeBM ) DeleteBMB(tapeBM);
	if( disk525BM ) DeleteBMB(disk525BM);
	if( disk35BM ) DeleteBMB(disk35BM);
	if( disk8BM ) DeleteBMB(disk8BM);
	if( disk5MBBM ) DeleteBMB(disk5MBBM);
	if( rlBM ) DeleteBMB(rlBM);
	keyBM = tapeBM = disk525BM = disk35BM = disk8BM = disk5MBBM = rlBM = NULL;
	KGetWndWH((long int *)&VW, (long int *)&VH);
	if( CfgFlags & CFG_BIGCRT ) {
		keyBM = LoadBMB("images\\87keys2.bmp", 0, 0, 0, 0);
		disk5MBBM= LoadBMB("images\\disks8.bmp", 0, 0, 0, 0);
		disk8BM  = LoadBMB("images\\disks6.bmp", 0, 0, 0, 0);
		disk35BM = LoadBMB("images\\disks4.bmp", 0, 0, 0, 0);
		disk525BM= LoadBMB("images\\disks2.bmp", 0, 0, 0, 0);
		rlBM  = LoadBMB("images\\runlite2.bmp", -1,-1,-1,-1);
		MENUx= 2*(2);
		MENUy= 2*(4);
		MENUh= 16;
		MENUw= 302;
		REGx = 2*(2);
		REGy = (MENUy+MENUh*(DBGMENUCNT+1));
		REGw = 28;
		REGh = 14;
		KEYx = (2);
		KEYy = VH - keyBM->physH - 2*2;
		CRTx = KEYx + 2*(87+2);
		CRTy = KEYy + 2*(2);

		RLw  = 2*40;
		RLh  = 2*16;
		RLx  = (CRTx-RLw)/2;
		RLy  = CRTy;

		BRTw = 2*24;
		BRTh = 2*99;
		BRTx = CRTx - BRTw - 2*5;
		BRTy = CRTy + 2*181 - BRTh;

		tracebot = KEYy - 2*2;
		TraceX = MENUx + MENUw + 2*2;
		TraceW = 8*((VW-TraceX)/8);
		TraceH = 12*((tracebot-2*(2))/12);
		TraceY = 2*(2);
	} else {
		keyBM = LoadBMB("images\\87keys1.bmp", 0, 0, 0, 0);
		disk5MBBM= LoadBMB("images\\disks7.bmp", 0, 0, 0, 0);
		disk8BM  = LoadBMB("images\\disks5.bmp", 0, 0, 0, 0);
		disk35BM = LoadBMB("images\\disks3.bmp", 0, 0, 0, 0);
		disk525BM= LoadBMB("images\\disks1.bmp", 0, 0, 0, 0);
		rlBM  = LoadBMB("images\\runlite1.bmp", -1,-1,-1,-1);

		MENUx= 2;
		MENUy= 4;
		MENUh= 16;
		MENUw= 302;
		REGx = 2;
		REGy = MENUy+MENUh*(DBGMENUCNT+1);
		REGw = 28;
		REGh = 14;
		KEYx = 2;
		KEYy = VH - keyBM->physH - 2;
		CRTx = KEYx + (87+2);
		CRTy = KEYy + (2);

		RLw  = 40;
		RLh  = 16;
		RLx  = (CRTx-RLw)/2;
		RLy  = CRTy;

		BRTw = 24;
		BRTh = 99;
		BRTx = CRTx - BRTw - 5;
		BRTy = CRTy + 181 - BRTh;

		tracebot = KEYy - 2;
		TraceX = MENUx + MENUw + 2;
		TraceW = 8*((VW-TraceX)/8);
		TraceH = 12*((tracebot-(2))/12);
		TraceY = (2);
	}

	return keyBM!=NULL && disk525BM!=NULL && disk35BM!=NULL && disk8BM!=NULL && disk5MBBM!=NULL && rlBM!=NULL;
}


BOOL SetupDispVars() {

	if( Model<2 ) return SetupDispVars85();
	else return SetupDispVars87();
}

//****************************************
void HP85OnResize() {

	if( SetupDispVars() ) {
		if( hp85BM!=NULL && (Model==2 || prtBM!=NULL) ) DrawAll();
	}
}

//****************************************
void HP85PWO(BOOL first) {

	long	i, c;

	if( Halt85==FALSE ) HaltHP85();

	// Reset IO cards
	for(c=0; c<16; c++) {
		if( IOcards[c].initProc != NULL ) IOcards[c].initProc(iR_RESET, Model, c, 0);
	}

	SysDir = (Model==2) ? "roms87" : "roms85";

	PWONinternal(first);
	if( !first ) {
		StoreDisAsmRecs();
		for(i=0; i<numROMS; i++) free(pROMS[i]);
		if( ROM ) free(ROM);
		ROM = NULL;
	}

	// init RAM/ROM
	ROMlen = 65536 + (RamSizeExt[Model] ? (16384<<RamSizeExt[Model]) : 0);
//	RamSizeExt=0 for none, 1 for 32K, 2 for 64K, 3 for 128K, 4 for 256K, 5 for 512K, 6 for 1024K
	ROM = (BYTE*)malloc(ROMlen);
	memset(ROM, 0, ROMlen);
	if( !LoadROMS() ) {	// load ROMs, init CPU, etc
		KillProgram();
		return;
	}

	// force DisAsm cursor relocation
	CursLine = 0xFFFFFFFF;

	Cycles = SecondsCycles = 0;
	StartTime = HaltTime = KGetTicks();

	if( CfgFlags & CFG_AUTORUN ) RunHP85();

	// redraw everything
	DrawAll();
}

//******************************************
void ShowHelp() {

	if( helpW<0 ) {
		helpW = 600;
		helpH = KWnds[0].h+WINDOW_TITLEY+WINDOW_FRAMEY;
		helpX = min(KWnds[0].x+KWnds[0].w-WINDOW_FRAMEX, PhysicalScreenW-helpW);
		helpY = KWnds[0].y;
	}
	if( pHelpMem==NULL ) {
		iHelpWnd = KCreateWnd(helpX, helpY, helpW, helpH, "Series 80 Emulator HELP", &HelpWndProc, TRUE);
		ShowWindow(KWnds[iHelpWnd].hWnd, SW_SHOW);
	} else SendMessage(KWnds[iHelpWnd].hWnd, WM_CLOSE, 0, 0);
}

///****************************************
void WriteIni() {

	long	i, j, k, c;
	DWORD	dw;
	long int sc;
	long	x, y, w, h;

	///*********************************
	/// Write INI section
	iniSetSec(iniSTR[INIS_INI]);
	sprintf((char*)ScratBuf, "%d", CURINIREV);
	iniSetVar(iniSTR[INIS_REV], ScratBuf);

	///*********************************
	/// Write EMULATOR section
	iniSetSec(iniSTR[INIS_EMULATOR]);

	KGetWindowPos(NULL, &x, &y, &w, &h, &sc);
	sprintf((char*)ScratBuf, "%ld %ld %ld %ld %ld", x, y, w, h, sc);
	iniSetVar(iniSTR[INIS_WINDOW], ScratBuf);

	sprintf((char*)ScratBuf, "%d %ld %ld %ld %ld", pHelpMem!=NULL, helpX, helpY, helpW, helpH);
	iniSetVar(iniSTR[INIS_HELP], ScratBuf);

	sprintf((char*)ScratBuf, "0x%8.8lX", CfgFlags);
	iniSetVar(iniSTR[INIS_FLAGS], ScratBuf);

	sprintf((char*)ScratBuf, "%d", (int)base);
	iniSetVar(iniSTR[INIS_BASE], ScratBuf);

	sprintf((char*)ScratBuf, "0x%8.8lX", CRTbrightness);
	iniSetVar(iniSTR[INIS_BRIGHT], ScratBuf);

	sprintf((char*)ScratBuf, "0x%8.8lX", MainWndClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MainWndTextClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MenuSelBackClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MenuSelTextClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MenuBackClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MenuTextClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MenuHiClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", MenuLoClr);
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), " 0x%8.8lX", PrintClr);
	iniSetVar(iniSTR[INIS_COLORS], ScratBuf);

	sprintf((char*)ScratBuf, "\"%s\"", Model==0?iniSTR[INIS_HP85A]:(Model==1?iniSTR[INIS_HP85B]:iniSTR[INIS_HP87]));
	iniSetVar(iniSTR[INIS_MODEL], ScratBuf);

	///****************************************
	/// Write INI for IO cards devices
	/// first, delete all OLD "devXXX" ini vars
	for(i=0; i<MAX_DEVICES; i++) {
		sprintf((char*)ScratBuf, "%s%ld", iniSTR[INIS_DEV], i);
		iniDelVar(ScratBuf);
	}
	/// next, set all Devices[].iniDevNum's to sequential counts
	for(i=j=0; i<MAX_DEVICES; i++) {
		if( Devices[i].devType!=DEVTYPE_NONE ) Devices[i].devID = j++;
	}

	SCwini = 0;
	for(c=0; c<16; c++) {
		if( IOcards[c].initProc != NULL ) {
			IOcards[c].initProc(iR_WRITE_INI, Model, c, 0);
			SCwini |= (1<<c);
		}
	}
	/// now look for ADDED devices that are on new select codes that haven't been written yet
	for(i=0; i<MAX_DEVICES; i++) {
		if( Devices[i].devType!=DEVTYPE_NONE && Devices[i].state==DEVSTATE_NEW && !(SCwini & (1<<Devices[i].wsc)) ) {
			Devices[i].initProc(DP_INI_WRITE, Devices[i].wsc, Devices[i].wca, 0, i);
		}
	}

	///*********************************
	/// Write model-specific INI sections
	for(i=0; i<3; i++) {
		iniSetSec(iniSTR[INIS_HP85A+i]);

		dw = RomLoads[i];
		ScratBuf[0] = 0;
		for(j=0; j<32; j++) if( dw & (1<<j) ) sprintf((char*)ScratBuf+strlen((char*)ScratBuf), "0%o ", (unsigned int)RomList[j]);
		iniSetVar(iniSTR[INIS_ROMS], ScratBuf);

		sprintf((char*)ScratBuf, "%lu", RamSizes[i]);
		iniSetVar(iniSTR[INIS_RAM], ScratBuf);

		sprintf((char*)ScratBuf, "%d", (1<<(4+RamSizeExt[i])) & 0xFFFFFFE0);
		iniSetVar(iniSTR[INIS_EXTRAM], ScratBuf);

		iniSetVar(iniSTR[INIS_MARKS], (BYTE*)"");
		for(k=0; k<sizeof(Marks[0]); k++) {
			if( Marks[i][k] ) {
				for(j=0; j<8; j++) {
					if( Marks[i][k]&(1<<j) ) iniAddVarNum(iniSTR[INIS_MARKS], 8*k+j, 1);	// format list of Marks in OCTAL
				}
			}
		}
		iniSetVar(iniSTR[INIS_BRKPTS], (BYTE*)"");
		for(j=0; j<NUMBRKPTS; j++) {
			if( BrkPts[i][j] ) iniAddVarNum(iniSTR[INIS_BRKPTS], BrkPts[i][j], 1);	// format list of Breakpoints in OCTAL
		}
		if( i!=2 ) {	// if HP85A or HP85B
			sprintf((char*)ScratBuf, "\"%s\"", CurTape);
			iniSetVar(iniSTR[INIS_TAPE], ScratBuf);
		}
	}

	iniWrite((BYTE*)ininame);
	iniClose();
}

///****************************************
/// The MAIN window HAS been created at this point, but is not yet visible.
void ReadIni() {

	long		i, j;
	long		x, y, w, h;
	DWORD		cfgrev, r, sc;

	i = iniRead((BYTE*)ininame);
	memset(&IOcards[0], 0, sizeof(IOcards));	// remove all IO cards
	for(j=0; j<MAX_DEVICES; j++) {
		Devices[j].devType = DEVTYPE_NONE;		// doesn't exist
		Devices[j].state = DEVSTATE_UNUSED;		// has never existed
	}
	if( i==1 || i==-1 ) {	// bad prunes, start over
startover:
		iniDelAll();
		KGetWindowPos(NULL, &x, &y, &w, &h, (long*)&sc);	// get sc
//			w = KINT_WND_SIZEW;
//			h = KINT_WND_SIZEH;
//			x = y = 0;
		CfgFlags = CFG_AUTORUN;
		memset(Marks, 0, sizeof(Marks));
		CRTbrightness = 0x00E6E6E6;
		Model = 0;	// default to HP-85A

		// set up default devices
		memcpy(Devices, DefDevices, sizeof(DefDevices));
		// set up IO cards to support those devices
		for(i=0; i<MAX_DEVICES; i++) {
			if( Devices[i].devType!=DEVTYPE_NONE ) {
				Devices[i].sc -= 3;	// change 3-10 to 0-7
//				Devices[i].state = DEVSTATE_BOOT;
				sc = Devices[i].wsc = Devices[i].sc;
				Devices[i].wca = Devices[i].ca;
				r  = Devices[i].ioType;
				IOcards[sc].type = r;
				IOcards[sc].initProc = initProcs[r];	// Set it's .initProc address
				IOcards[sc].address = 0177520 + 2*sc;
				IOcards[sc].numaddr = 2;
				if( Devices[i].devType<=DEV_MAX_DISK_TYPE ) {
					MenuDiskSC = sc;
					MenuDiskTLA = Devices[i].ca;
					MenuDiskDrive = 0;
				}
			}
		}
		/// iR_READ_INI IO card devices
		for(sc=0; sc<16; sc++) if( IOcards[sc].initProc!=NULL ) IOcards[sc].initProc(iR_READ_INI, Model, sc, 0);	// give each card a chance at ReadINI time
	} else {	// ini file okay, get the values
		if( i==2 ) {	// if checksum mismatch, report them, then go ahead and process it anyway
			iniFindSec(iniSTR[INIS_INI]);	// move back to header section
			if( iniBadSum() ) {	// ini header is potentially bad
				sprintf((char*)ScratBuf+256, "Bad checksum: %s", iniSTR[INIS_INI]);
				SystemMessage((char*)ScratBuf+256);
			}
			if( !iniFindVar(iniSTR[INIS_REV]) ) goto startover;
			iniGetNum(&r);
			if( r<CURINIREV ) goto startover;

			while( (i=iniNextSec(ScratBuf, 256)) > 0 ) {	// check all the other sections for bad checksums
				if( iniBadSum() ) {	// if bad
					sprintf((char*)ScratBuf+256, "Bad checksum: %s", ScratBuf);
					SystemMessage((char*)ScratBuf+256);
				}
			}
		}
		// Get revision of INI file
		iniFindSec(iniSTR[INIS_INI]);
		if( iniFindVar(iniSTR[INIS_REV]) ) iniGetNum(&cfgrev);
		if( cfgrev<CURINIREV ) goto startover;

		iniFindSec(iniSTR[INIS_EMULATOR]);

		iniFindVar(iniSTR[INIS_WINDOW]);
		iniGetNum((DWORD*)&KWnds[0].x);
		iniGetNum((DWORD*)&KWnds[0].y);
		iniGetNum((DWORD*)&KWnds[0].w);
		iniGetNum((DWORD*)&KWnds[0].h);
		iniGetNum((DWORD*)&KWnds[0].misc);

		iniFindVar(iniSTR[INIS_HELP]);
		iniGetNum((DWORD*)&helpON);
		iniGetNum((DWORD*)&helpX);
		iniGetNum((DWORD*)&helpY);
		iniGetNum((DWORD*)&helpW);
		iniGetNum((DWORD*)&helpH);

		iniFindVar(iniSTR[INIS_FLAGS]);
		iniGetNum((DWORD*)&CfgFlags);

		iniFindVar(iniSTR[INIS_BASE]);
		iniGetNum((DWORD*)&base);

		iniFindVar(iniSTR[INIS_BRIGHT]);
		iniGetNum((DWORD*)&CRTbrightness);

		iniFindVar(iniSTR[INIS_COLORS]);
		iniGetNum((DWORD*)&MainWndClr);
		iniGetNum((DWORD*)&MainWndTextClr);
		iniGetNum((DWORD*)&MenuSelBackClr);
		iniGetNum((DWORD*)&MenuSelTextClr);
		iniGetNum((DWORD*)&MenuBackClr);
		iniGetNum((DWORD*)&MenuTextClr);
		iniGetNum((DWORD*)&MenuHiClr);
		iniGetNum((DWORD*)&MenuLoClr);
		iniGetNum((DWORD*)&PrintClr);

		iniFindVar(iniSTR[INIS_MODEL]);
		Model = 0;
		if( iniGetStr(ScratBuf, 256)>0 ) {
			if( !stricmp((char*)ScratBuf, (char*)iniSTR[INIS_HP85B]) ) Model = 1;
			else if( !stricmp((char*)ScratBuf, (char*)iniSTR[INIS_HP87]) ) Model = 2;
		}

		///*******************************************
		/// Read IO devices
		/// i = ini file dev# counter
		/// j = Devices[] index
		for(i=j=0; i<MAX_DEVICES; i++) {
			sprintf((char*)ScratBuf, "%s%ld", iniSTR[INIS_DEV], i);
			if( iniFindVar(ScratBuf) ) {
				if( iniGetStr(ScratBuf, 256)>0 ) {	// get device name
					Devices[j].devType = DEVTYPE_NONE;
					if( !strcmp((char*)ScratBuf, (char*)iniSTR[INIS_DISK525]) ) {
						Devices[j].devType = DEVTYPE_5_25;
						Devices[j].initProc = &devDISK;
					} else if( !strcmp((char*)ScratBuf, (char*)iniSTR[INIS_DISK35]) ) {
						Devices[j].devType = DEVTYPE_3_5;
						Devices[j].initProc = &devDISK;
					} else if( !strcmp((char*)ScratBuf, (char*)iniSTR[INIS_DISK8]) ) {
						Devices[j].devType = DEVTYPE_8;
						Devices[j].initProc = &devDISK;
					} else if( !strcmp((char*)ScratBuf, (char*)iniSTR[INIS_DISK5MB]) ) {
						Devices[j].devType = DEVTYPE_5MB;
						Devices[j].initProc = &devDISK;
					} else if( !strcmp((char*)ScratBuf, (char*)iniSTR[INIS_FILEPRT]) ) {
						Devices[j].devType = DEVTYPE_FILEPRT;
						Devices[j].initProc = &devFilePRT;
					} else if( !strcmp((char*)ScratBuf, (char*)iniSTR[INIS_HARDPRT]) ) {
						Devices[j].devType = DEVTYPE_HARDPRT;
						Devices[j].initProc = &devHardPRT;
					}
					if( Devices[j].devType!=DEVTYPE_NONE ) {
						if( i>=NextDevID ) NextDevID = i+1;
						Devices[j].state = DEVSTATE_BOOT;		// flag that this device was in the .ini file
						Devices[j].devID = i;
						r = Devices[j].ioType = IOTYPE_HPIB;
						iniGetNum((DWORD*)&Devices[j].sc);
						iniGetNum((DWORD*)&Devices[j].ca);
						Devices[j].sc -= 3;
						sc = Devices[j].wsc = Devices[j].sc;
						Devices[j].wca = Devices[j].ca;
						IOcards[sc].type = r;
						IOcards[sc].initProc = initProcs[r];	// Set it's .initProc address
						IOcards[sc].address = 0177520 + 2*sc;
						if( sc<8 ) IOcards[sc].numaddr = 2;
						else {
							sprintf((char*)ScratBuf, "Need to set .numaddr for IOcards[%ld] in ReadIni() in mach85.c!", sc);
							SystemMessage((char*)ScratBuf);
						}
						if( Devices[i].devType==DEVTYPE_5_25 || Devices[i].devType==DEVTYPE_3_5 ) {
							MenuDiskSC = sc;
							MenuDiskTLA = Devices[j].ca;
							MenuDiskDrive = 0;
						}
						++j;
					}
				}
			}
		}
		/// iR_READ_INI IO cards (which will iR_READ_INI their devices)
		for(sc=0; sc<16; sc++) if( IOcards[sc].initProc!=NULL ) IOcards[sc].initProc(iR_READ_INI, Model, sc, 0);	// give each card a chance at ReadINI time

		///***********************************
		/// Read model-specific INI
		for(i=0; i<3; i++) {
			iniFindSec(iniSTR[INIS_HP85A+i]);

			RomLoads[i] = 0;
			iniFindVar(iniSTR[INIS_ROMS]);
			while( !iniEndVar() ) {
				iniGetNum(&r);
				for(j=0; j<32; j++) {
					if( RomList[j]==r ) {
						RomLoads[i] |= (1<<j);
						break;
					}
				}
			}

			iniFindVar(iniSTR[INIS_RAM]);
			iniGetNum((DWORD*)&RamSizes[i]);

			iniFindVar(iniSTR[INIS_EXTRAM]);
			iniGetNum(&r);
			if( r==1024 ) r = 6;
			else if( r==512 ) r = 5;
			else if( r==256 ) r = 4;
			else if( r==128 ) r = 3;
			else if( r==64 ) r = 2;
			else if( r==32 ) r = 1;
			else r = 0;
			RamSizeExt[i] = r;

			iniFindVar(iniSTR[INIS_MARKS]);
			memset(&Marks[i], 0, sizeof(Marks[0]));
			while( !iniEndVar() ) {
				iniGetNum(&r);
				Marks[i][r>>3] |= (1<<(r&7));
			}

			iniFindVar(iniSTR[INIS_BRKPTS]);
			memset(BrkPts[i], 0, sizeof(BrkPts[0]));
			j = 0;
			while( !iniEndVar() ) {
				iniGetNum(&r);
				BrkPts[i][j++] = r;
			}

			if( i!=2 ) {	// in HP85A or HP85B
				iniFindVar(iniSTR[INIS_TAPE]);
				iniGetStr((BYTE*)&CurTape, sizeof(CurTape));
			}
		}
	}
}

//****************************************
void HP85OnStartup() {

	long	x, y, w, h, c;
//	DWORD	cfgrev;
//	long	hFile;
	long int	sc;

	NextDevID = 0;	// set initial device ID

	/// When devices are read in from the .ini file, their .devID field is set to the "devXX=" XX number from the file, which initProc() can use to find their "devXX=" variable for reading their extended configuration info.
	/// Any devices DELETED via the DEVICES dialog will have their .devID set to -1 to indicate they should NOT be written back out to the .ini file.
	/// Any devices ADDED via the DEVICES dialog will have their .devID field set to NextDevID++, so NextDevID needs to 1 more than the highest "devXX=" XX number at ReadIni().
	/// Before WriteIni(), all .devID fields, EXEPT those that are set to -1, should be set to sequential values from 0 on up.

	ReadIni();

	RomLoad = RomLoads[Model];
	RamSize = RamSizes[Model];

	KGetWindowPos(NULL, &x, &y, &w, &h, &sc);
	KSetWindowPos(NULL, KWnds[0].x, KWnds[0].y, KWnds[0].w, KWnds[0].h, sc);

	if( !SetupDispVars() ) {
		SystemMessage("Failed to load IMAGES\\*.BMP files");
		KillProgram();
		return;
	}

	// Startup IO cards
	for(c=0; c<16; c++) {
		if( IOcards[c].initProc != NULL ) IOcards[c].initProc(iR_STARTUP, Model, c, 0);
	}

	StartTimer(25);

	WaveOpen();

	HP85PWO(TRUE);

	if( helpON ) ShowHelp();
}

//****************************************
void HP85OnShutdown() {

	long	i, c;

	WaveStopBeep();
	WaveClose();

	WriteIni();

	// Shutdown IO cards
	for(c=0; c<16; c++) {
		if( IOcards[c].initProc != NULL ) IOcards[c].initProc(iR_SHUTDOWN, Model, c, 0);
	}

	EjectTape();

	StopTimer();

	StoreDisAsmRecs();
	for(i=0; i<numROMS; i++) free(pROMS[i]);
	if( ROM!=NULL ) free(ROM);

	PWOFFinternal();

	DeleteBMB(disk5MBBM);
	disk5MBBM = 0;
	DeleteBMB(disk8BM);
	disk8BM = 0;
	DeleteBMB(disk35BM);
	disk35BM = 0;
	DeleteBMB(disk525BM);
	disk525BM = 0;

	if( pKeyMem!=NULL ) {
		free(pKeyMem);
		pKeyMem = NULL;
	}
}

/* MS-Windows KEYCODE identifiers
#define VK_LBUTTON          0x01
#define VK_RBUTTON          0x02
#define VK_CANCEL           0x03
#define VK_MBUTTON          0x04
#define VK_BACK             0x08
#define VK_TAB              0x09
#define VK_CLEAR            0x0C
#define VK_RETURN           0x0D
#define VK_SHIFT            0x10
#define VK_CONTROL          0x11
#define VK_MENU             0x12
#define VK_PAUSE            0x13
#define VK_CAPITAL          0x14
#define VK_ESCAPE           0x1B
#define VK_SPACE            0x20
#define VK_PRIOR            0x21
#define VK_NEXT             0x22
#define VK_END              0x23
#define VK_HOME             0x24
#define VK_LEFT             0x25
#define VK_UP               0x26
#define VK_RIGHT            0x27
#define VK_DOWN             0x28
#define VK_SELECT           0x29
#define VK_PRINT            0x2A
#define VK_EXECUTE          0x2B
#define VK_SNAPSHOT         0x2C
#define VK_INSERT           0x2D
#define VK_DELETE           0x2E
#define VK_HELP             0x2F
#define VK_NUMPAD0          0x60
#define VK_NUMPAD1          0x61
#define VK_NUMPAD2          0x62
#define VK_NUMPAD3          0x63
#define VK_NUMPAD4          0x64
#define VK_NUMPAD5          0x65
#define VK_NUMPAD6          0x66
#define VK_NUMPAD7          0x67
#define VK_NUMPAD8          0x68
#define VK_NUMPAD9          0x69
#define VK_MULTIPLY         0x6A
#define VK_ADD              0x6B
#define VK_SEPARATOR        0x6C
#define VK_SUBTRACT         0x6D
#define VK_DECIMAL          0x6E
#define VK_DIVIDE           0x6F
#define VK_F1               0x70
#define VK_F2               0x71
#define VK_F3               0x72
#define VK_F4               0x73
#define VK_F5               0x74
#define VK_F6               0x75
#define VK_F7               0x76
#define VK_F8               0x77
#define VK_F9               0x78
#define VK_F10              0x79
#define VK_F11              0x7A
#define VK_F12              0x7B
#define VK_F13              0x7C
#define VK_F14              0x7D
#define VK_F15              0x7E
#define VK_F16              0x7F
#define VK_F17              0x80
#define VK_F18              0x81
#define VK_F19              0x82
#define VK_F20              0x83
#define VK_F21              0x84
#define VK_F22              0x85
#define VK_F23              0x86
#define VK_F24              0x87
#define VK_NUMLOCK          0x90
#define VK_SCROLL           0x91
#define	VK_OEM_PERIOD		0xBE	// just the normal '.' key
*/
/* VK_A thru VK_Z are the same as their ASCII equivalents: 'A' thru 'Z' */
/* VK_0 thru VK_9 are the same as their ASCII equivalents: '0' thru '9' */

//********************************************************
// Called from HP85OnKeyDown() and also from DLG.C when
// SAVE-ing the EDIT dialog.
void DownCursor() {

	if( CursLine < TraceLines-1 ) {
		if( LineIndex[CursLine]!=0xFFFFFFFF ) ++CursLine;
	} else DisStart = LineIndex[1];
	if( HP85ADDR(DisStart) > (0x00010000-TraceLines) ) DisStart = (0x00010000 - TraceLines)<<16;
	DrawDisAsm(KWnds[0].pBM);
	FlushScreen();
}

//********************************************************
// Called from HP85OnKeyDown()
void UpCursor() {
	/* NOTE: moving cursor UP is much more difficult than moving it down,
		because we haven't necessarily DISASSEMBLED the code above the start
		of the debug window, so we don't know where the previous "line" of
		code starts.  As a result, if we're at the start of the debug window,
		we call ScrollUp(), which goes WAY back in memory and starts disassembling,
		watching for the current address and keeping track of addresses where lines
		of code begin.  This lets us back up reasonably well and keep the disassembly
		"aligned" properly.  But there are ocassional problems.  See README.TXT for
		mention of it in the "DESIGN PROBLEMS" section.
	*/
	if( CursLine ) --CursLine;	// if not at start of disassembly window
	else if( DisStart ) ScrollUp(1);	// else if not at address 0
	else return;	// else nowhere to go
	DrawDisAsm(KWnds[0].pBM);
	FlushScreen();
}

//********************************************************
void ExecuteMenu(long	item) {

	int c;

	switch( item ) {
	 case MENU_HELP:	// HELP
		CurMenuItem = -1;	// so menu item doesn't come up highlighted after dialog
//		CreateHelpDlg();
		ShowHelp();
		break;
	 case MENU_ROMS:	// Option ROMs
		CurMenuItem = -1;
		if( (IO_TAPSTS | IO_TAPCART) & 1 ) WriteTape();	// make sure any tape changes are saved
		for(c=0; c<16; c++) {	// flush out any writes to diskettes to actual storage
			if( IOcards[c].initProc != NULL && IOcards[c].type==IOTYPE_HPIB ) IOcards[c].initProc(iR_FLUSH, Model, c, 0);
		}
		StoreDisAsmRecs();
		DisAsmNULL = TRUE;
		CreateRomDlg();
		break;
	 case MENU_OPTS:	// Emulator OPTIONS
		CurMenuItem = -1;
		CreateOptionsDlg();
		break;
	 case MENU_ADDR:	// DISASSEMBLE at address
		if( Halt85 ) {
			CurMenuItem = -1;
			CreateViewAddressDlg();
		} else {		// TYPE ASCII FILE as keystrokes
			CurMenuItem = -1;
			CreateTypeASCIIDlg();
		}
		break;
//	 case MENU_IMPORT:
	 case MENU_EDIT:
		if( Halt85 ) {	// EDIT current line
			CurMenuItem = -1;
			CurRec = LineRecs[CursLine];
			if( CurRec != NULL && CurRec->type==REC_REM ) CreateCommentDlg();
			else CreateCodeEditDlg();
		} else {		// IMPORT LIF file
			// if diskette in drive, it's initialized, and not write protected...
			if( IsDisk(MenuDiskSC, MenuDiskTLA, MenuDiskDrive) ) {
				if( IsDiskInitialized(MenuDiskSC, MenuDiskTLA, MenuDiskDrive) ) {
					if( !IsDiskProtected(MenuDiskSC, MenuDiskTLA, MenuDiskDrive) ) {
						if( ShiftDown ) CreateImportAllDlg(MenuDiskSC, MenuDiskTLA, MenuDiskDrive);
						else CreateImportDlg(MenuDiskSC, MenuDiskTLA, MenuDiskDrive);
					} else SystemMessage("Disk is write protected");
				} else SystemMessage("Disk is not initialized");
			} else SystemMessage("Disk is not loaded");
		}
		break;
//	 case MENU_EXPORT:
	 case MENU_COMM:
		if( Halt85 )  {	// Insert full-line COMMENT
			if( AddFullLineComment() ) {
				CurMenuItem = -1;
				CurRec = LineRecs[CursLine];
				CreateCommentDlg();
			}
		} else {		// EXPORT LIF file
			// if diskette in drive and it's initialized...
			if( IsDisk(MenuDiskSC, MenuDiskTLA, MenuDiskDrive) ) {
				if( IsDiskInitialized(MenuDiskSC, MenuDiskTLA, MenuDiskDrive) ) CreateExportDlg(MenuDiskSC, MenuDiskTLA, MenuDiskDrive);
				else SystemMessage("Disk is not initialized");
			} else SystemMessage("Disk is not loaded");
		}
		break;
	 case MENU_EXP:		// EXPORT ROM disassembly
		if( Halt85 ) ExportDisAsm();
		break;
	 case MENU_RUNBRK:		// RUN HP-85 emulation
		if( Halt85 ) {
			// save CPU state for drawing "changed items" in debugger
			memcpy(LastReg, Reg, sizeof(LastReg));
			LastE = E;
			LastFlags = Flags;
			LastArp = Arp;
			LastDrp = Drp;

			RunHP85();
		} else {
			HaltHP85();
		}
		ForceDisStart();
		DrawAll();
		break;
	 case MENU_RESET:	// RESET TO POWER-ON
		if( (IO_TAPSTS | IO_TAPCART) & 1 ) WriteTape();	// make sure any tape changes are saved
		for(c=0; c<16; c++) {	// flush out any changes to disks
			if( IOcards[c].initProc != NULL && IOcards[c].type==IOTYPE_HPIB ) IOcards[c].initProc(iR_FLUSH, Model, c, 0);
		}
		HP85PWO(FALSE);
		break;
	 case MENU_EXIT:	// EXIT
		KillProgram();
		break;
	 default:
		break;
	}
}

//********************************************************
// Dump all Dis REC_ records for the current 8K ROM space being viewed as a .LST file
// with DOT COMMANDS suitable for the HP80DIS program.
// geekgeek

long	OutFile;

void WriteLine(BYTE *buf) {

	Kwrite(OutFile, buf, strlen((char*)buf));
	Kwrite(OutFile, (BYTE*)"\r\n", 2);
}

void DumpRomDotCommands() {

	int		addr, blk, romnum, fullLineCnt=0;
	DISASMREC	*pRec;
	BOOL	IsB=FALSE;
	DWORD	lastFullLineAddr=0xFFFFFFFF;

	addr = HP85ADDR(DisStart);
	if( addr > 077777 ) {
		MessageBox(NULL, "Dot-Command output is only supported for ROMs", "Darn!", MB_OK);
		return;
	}
	blk = addr/8192;
	pRec = pDis[blk][0];
	romnum = ROM[060000];
	if( Model==1 && (blk<3 || romnum==0 || romnum==0300 || romnum==0320 || romnum==0321) ) IsB = TRUE;

	if( addr < 060000 ) sprintf(FilePath, "%s\\romsys%d%s.lst", SysDir, blk+1, IsB ? "B" : "");
	else sprintf(FilePath, "%s\\rom%3.3o%s.lst", SysDir, (unsigned int)ROM[060000], IsB ? "B" : "");

	if( -1 != (OutFile=Kopen(FilePath, O_WRBINNEW)) ) {
		sprintf((char*)ScratBuf, ".dis %s", FilePath);
		WriteLine(ScratBuf);
		if( pRec==NULL ) WriteLine((BYTE*)"NO RECORDS");
		else {
			for(; pRec!=NULL; pRec = pRec->pNext) {
				switch( pRec->type ) {
				 case REC_DRP:
					sprintf((char*)ScratBuf, ".drp %o", (unsigned int)(pRec->address>>16));
					WriteLine(ScratBuf);
					break;
				 case REC_BYT:
					sprintf((char*)ScratBuf, ".byt %o %o", (unsigned int)(pRec->address>>16), pRec->datover);
					WriteLine(ScratBuf);
					break;
				 case REC_DEF:
					sprintf((char*)ScratBuf, ".def %o %d", (unsigned int)(pRec->address>>16), 0);//(pRec->flags & FLG_ROMJSB_DEF));
					WriteLine(ScratBuf);
					break;
				 case REC_COD:
					sprintf((char*)ScratBuf, ".cod %o %o", (unsigned int)(pRec->address>>16), pRec->datover);
					WriteLine(ScratBuf);
					break;
				 case REC_ASC:
					sprintf((char*)ScratBuf, ".asc %o %o", (unsigned int)(pRec->address>>16), pRec->datover);
					WriteLine(ScratBuf);
					break;
				 case REC_ASP:
					sprintf((char*)ScratBuf, ".asp %o", (unsigned int)(pRec->address>>16));
					WriteLine(ScratBuf);
					break;
				 case REC_BSZ:
					sprintf((char*)ScratBuf, ".bsz %o %o", (unsigned int)(pRec->address>>16), pRec->datover);
					WriteLine(ScratBuf);
					break;
				 case REC_VAL:
					break;
				 default:
					break;
				}
				if( pRec->type!=REC_REM && pRec->Comment[0] ) {

				}
				if( pRec->label[0] ) {
					sprintf((char*)ScratBuf, ".lab %o %s", (unsigned int)(pRec->address>>16), pRec->label);
					WriteLine(ScratBuf);
				}
				if( pRec->Comment!=NULL ) {
					if( pRec->type==REC_REM ) {	// full-line comment
						if( (pRec->address>>16) != lastFullLineAddr ) {
							lastFullLineAddr = (pRec->address>>16);
							fullLineCnt = 0;
						}
						sprintf((char*)ScratBuf, ".com %o %o %s", (unsigned int)(pRec->address>>16), fullLineCnt, pRec->Comment);
						WriteLine(ScratBuf);
						++fullLineCnt;
					} else {	// eol comment
						sprintf((char*)ScratBuf, ".eol %o %s", (unsigned int)(pRec->address>>16), pRec->Comment);
						WriteLine(ScratBuf);
					}
				}
/*				if( pRec->flags & FLG_GTO ) {
					sprintf(ScratBuf, ".gto %o", (unsigned int)HP85ADDR(pRec->address));
					WriteLine(ScratBuf);
				}
*/
			}
		}
		Kclose(OutFile);
		sprintf((char*)ScratBuf, "%s created successfully with DOT-COMMANDS for HP80DIS", FilePath);
		MessageBox(NULL, (char*)ScratBuf, "Done!", MB_OK);
	} else MessageBox(NULL, "Failed creating file!", "Oh, shucks!", MB_OK);
}

//********************************************************
BOOL HP85OnKeyDown(DWORD key) {

	BYTE	k, b;
	int		i, j;
	DWORD	brk;
	WORD	m, s;

	// first toggle Shift/Ctl flags
	if( key==VK_SHIFT ) {
		SetShift(TRUE);
		ShiftDown = TRUE;
	}
	if( key==VK_CONTROL ) CtlDown = TRUE;

	// if dialog open, everything else goes to it
	if( ActiveDlg!=NULL ) return SendMsg((WND *)ActiveDlg, DLG_KEYDOWN, key, 0, 0);

	// handle keys that are active in both "run" and "debug" modes
	switch( key ) {
	 case VK_F12:
		if( CtlDown ) {
			ExecuteMenu(MENU_RESET);
			return TRUE;
		}
		break;
/*
// Enable this code, then use numpad+ followed by D (disassemble) at 60000 to see disassembly of a ROM (208=Mass Storage)
     case 109: // numpad -
        SelectROM(0);
        return TRUE;
     case 107: // numpad +
         SelectROM(208);
        return TRUE;
*/
	}
	// handle keys that are active in "debug" mode
	if( Halt85 ) {
		switch( key ) {
		 case 0xBE:	// '.' key
			DumpRomDotCommands();
			return TRUE;
		 case VK_BACK:
			if( CtlDown ) {	// clear BackAddress[] stack
				BackAddressCnt = 0;
			} else {		// back up to last JUMP from address
				if( BackAddressCnt ) {
					brk = BackAddress[--BackAddressCnt];
					if( brk > 0x00010000-TraceLines ) brk = (DWORD)(0x00010000-TraceLines);
					DisStart = (brk<<16) | 0x0000FFFF;
					CursLine = 0;
					DrawDebug();
				}
			}
			return TRUE;
		 case VK_F1:
			ExecuteMenu(MENU_HELP);
			return TRUE;
		 case VK_F2:	// mark / goto mark
			s = (WORD)HP85ADDR(LineIndex[CursLine]);
			m = (WORD)HP85ADDR(LineIndex[CursLine]+1);
			if( CtlDown ) {
				if( ShiftDown ) memset(Marks, 0, sizeof(Marks));
				else Marks[Model][s/8] ^= (1<<(s & 7));	// toggle Mark on current (or next) code line
				DrawDebug();
			} else {
				while( !(Marks[Model][m/8] & (1<<(m & 7)))  &&  (++m & 7) ) ;	// while not at a mark in the first byte
				while( Marks[Model][m/8]==0 && m>s ) m += 8;	// search to end, byte by byte, until m wraps around to 0
				while( Marks[Model][m/8]==0 && m<s ) m += 8;	// search from beginning up to start, byte by byte
				if( Marks[Model][m/8] != 0 ) {	// if found a Mark
					while( !(Marks[Model][m/8] & (1<<(m & 7))) ) ++m;	// search for specific bit in this byte
					brk = (((DWORD)m)<<16) | 0x0000FFFF;	// make code line number
					if( LineIndex[0] > brk || LineIndex[TraceLines-1] < brk ) {
						DisStart = brk;
						DrawDebug();
						ScrollUp(1);
						CursLine = 1;
					} else {
						for(CursLine=0; LineIndex[CursLine]<brk && CursLine<TraceLines-1; ++CursLine) ;
					}
					DrawDebug();
				}
			}
			return TRUE;
		 case VK_F8:	// RUN emulator (stop debugging)
			ExecuteMenu(MENU_RUNBRK);
			return TRUE;
		 case VK_F9:	// SINGLE-STEP emulator
			// save CPU state for drawing "changed items" in debugger
			memcpy(LastReg, Reg, sizeof(LastReg));
			LastE = E;
			LastFlags = Flags;
			LastArp = Arp;
			LastDrp = Drp;

			Exec85(1);
			ForceDisStart();
			DrawDebug();
			return TRUE;
		 case VK_F10:	// SET/CLEAR breakpoint on cursor line
			if( CtlDown ) {	// if CTL-F9, clear ALL breakpoints
				for(i=0; i<NUMBRKPTS; i++) BrkPts[Model][i] = 0;
				DrawDebug();
				return TRUE;
			}
			brk = HP85ADDR(LineIndex[CursLine]);
			j = -1;
			for(i=0; i<NUMBRKPTS; i++) {
				if( BrkPts[Model][i]==brk ) {
					BrkPts[Model][i] = 0;	// clear breakpoint
					DrawDebug();
					return TRUE;
				} else if( BrkPts[Model][i]==0 ) j = i;
			}
			if( j!=-1 ) {
				BrkPts[Model][j] = (WORD)brk;	// set breakpoint
				DrawDebug();
				return TRUE;
			}
			sprintf(FilePath, "Maximum %d breakpoints all in use.", NUMBRKPTS);
			SystemMessage(FilePath);
			return TRUE;
		 case VK_F11:	// STEP-OVER instruction line (ie, single-step OVER jsb and all arp/drp's, etc)
			// save CPU state for drawing "changed items" in debugger
			memcpy(LastReg, Reg, sizeof(LastReg));
			LastE = E;
			LastFlags = Flags;
			LastArp = Arp;
			LastDrp = Drp;

			while( TRUE ) {
				// get next opcode byte
				// if it's DRP/ARP, execute it and loop
				// if it's JSB, set breakpoint AFTER it, RUN, and break out of loop
				// if it's anything else, execute it and break out of loop
				b = ROM[GETREGW(4)];
				if( b < 0200 ) {	// DRP/ARP
					Exec85(1);	// do the drp/arp
					continue;	// keep going
				}
				if( b == 0306 || b == 0316 ) {	// JSB
					BrkPts[Model][NUMBRKPTS] = GETREGW(4) + 3;
					RunHP85();
					DrawAll();
				} else {
					Exec85(1);	// execute all other instructions
					ForceDisStart();
					DrawDebug();
				}
				break;
			}
			return TRUE;
		 case VK_UP:	// move debug cursor UP
			UpCursor();
			break;
		 case VK_DOWN:	// move debug cursor DOWN
			DownCursor();
			break;
		 case VK_PRIOR:	// PAGE UP debug window
			if( HP85ADDR(DisStart) ) {
				ScrollUp(TraceLines-1);
			}
			break;
		 case VK_NEXT:	// PAGE DOWN debug window
			if( HP85ADDR(DisEnd) !=0 ) {	// if not positioned perfectly at end of I/O space
				DisStart = LineIndex[TraceLines-1];	//DisEnd;
				// if too many lines for remaining I/O space, limit it
				if( HP85ADDR(DisStart) > (0x00010000-TraceLines) ) DisStart = (0x00010000 - TraceLines)<<16;
				DrawDisAsm(KWnds[0].pBM);
			}
			break;
		 default:
			break;
		}
	// handle keys that are active in "run" mode
	} else {
		if( !(Int85 & INT_KEY) && !IsKeyPressed() ) {
			SetHomeKey(FALSE);	// release fake SHIFT key for HP-87 HOME key
			switch( key ) {
			 case VK_ESCAPE:// KEY LABEL
			 	k = 0226;
			 	break;
//				IO_KEYCOD = 0226;
//				IO_KEYSTS |= 2;
//				Int85 |= INT_KEY;
//				return TRUE;
			 case VK_F1:	// K1
				k = ShiftDown ? 0204 : 0200;
				break;
			 case VK_F2:	// K2
				k = ShiftDown ? 0205 : 0201;
				break;
			 case VK_F3:	// K3
				k = ShiftDown ? 0206 : 0202;
				break;
			 case VK_F4:	// K4
				k = ShiftDown ? 0207 : 0203;
				break;
			 case VK_F5:	// K5
				if( Model==2 ) k = ShiftDown ? 0245 : 0241;
				else return TRUE;	// nothing for HP-85
				break;
			 case VK_F6:	// K6
				if( Model==2 ) k = ShiftDown ? 0254 : 0242;
				else return TRUE;	// nothing for HP-85
				break;
			 case VK_F7:	// K7
				if( Model==2 ) k = ShiftDown ? 0223 : 0234;
				else return TRUE;
				break;
			 case VK_F8:
				if( CtlDown ) {	// break into debugger
					ExecuteMenu(MENU_RUNBRK);
					return TRUE;
				} else k = ShiftDown ? 0217 : 0215;	// RUN
				break;
			 case VK_F9:	// LIST/PLIST
				k = ShiftDown ? 0225 : 0224;
				break;
			 case VK_F10:	// GRAPH
				k = (Model<2) ? 0223 : 0250;
				break;
			 case VK_F11:	// STORE/TEST
//				k = ShiftDown ? 0221 : 0251;
				break;
			 case VK_F12:	// -LINE/CLEAR
			 	if( Model<2 ) k = ShiftDown ? 0222 : 0240;
			 	else k = ShiftDown ? 0211 : 0235;
				break;
			 case VK_PAUSE:	// PAUSE/STEP
				k = ShiftDown ? 0220 : 0216;
				break;
			 case VK_BACK:
				k = ShiftDown ? 0233 : 0231;
				break;
			 case VK_RETURN:
				k = 0232;
				break;
			 case VK_LEFT:
				k = (Model<2) ? 0234 : 0237;
				break;
			 case VK_RIGHT:
				k = (Model<2) ? 0235 : 0252;
				break;
			 case VK_UP:
				if( CtlDown && Model<2 ) {
					if( PRTlen - PRTscroll > PRTh ) {
						PRTscroll += PTH;
						DrawPrinter();
					}
					return TRUE;
				} else k = (Model<2) ? 0241 : 0243;
				break;
			 case VK_DOWN:
				if( CtlDown && Model<2 ) {
					if( PRTscroll ) {
						PRTscroll -= PTH;
						DrawPrinter();
					}
					return TRUE;
				} else k = (Model<2) ? 0242 : 0244;
				break;
			 case VK_INSERT:
				k = (Model<2) ? 0243 : 0236;
				break;
			 case VK_DELETE:
				k = ShiftDown ? ((Model<2) ? 0240 : 0235) : ((Model<2) ? 0244 : 0210);
				break;
//			 case VK_END:
//				k = 160;
//				break;
			 case VK_HOME:
			 	if( Model<2 ) k = 0245;
			 	else {
					k = 0243;
					SetHomeKey(TRUE);	// fake SHIFT key down
			 	}
				break;
			 case VK_PRIOR:
				k = (Model<2) ? 0236 : 0221;
				break;
			 case VK_NEXT:
				k = (Model<2) ? 0237 : 0251;
				break;
			 default:
				return FALSE;
			}
			SetKeyDown(k);
			return TRUE;
		}
	}
	return FALSE;
}

//********************************************************
BOOL HP85OnKeyUp(DWORD key) {

	if( key==VK_SHIFT ) {
		SetShift(FALSE);
		ShiftDown = FALSE;
	}
	if( key==VK_CONTROL ) CtlDown = FALSE;

	if( ActiveDlg!=NULL ) return SendMsg((WND *)ActiveDlg, DLG_KEYUP, key, 0, 0);

	if( Halt85 ) {

	} else {
		SetKeyPressed(FALSE);
		return TRUE;
	}

	return FALSE;
}

/*
This is a function that I hacked together when I was testing/debugging
the AD opcode.  I leave it here so that I have something to start from
if/when I do further exploration of SBB/SBM.

void TestADSB() {

	WORD	firstup, seconddn, cy, res, sbb;
	int		f;

	if( -1!=(f=Kopen("adbtest", O_WRBINNEW)) ) {

		for(cy=0; cy<16; cy++) {
			for(res=0; res<16; res++) {
				sprintf(TmpPath, "0x%4.4X,", NEW_AD_BCD[cy*16+res] & 0x00FF);
				Kwrite(f, TmpPath, strlen(TmpPath));
			}
			Kwrite(f, "\r\n", 2);
		}

//		Kwrite(f, (BYTE *)"top  bot  Add[TC]  SB_BCD\r\n", 27);


		for(firstup=0; firstup<16; firstup++) {
			for(seconddn=0; seconddn<16; seconddn++) {
				cy = 0;
				res = (WORD)AddBCD(firstup, TC_BCD[seconddn], (BYTE *)&cy);
				res = ((cy<<8)+res);
				sbb = SB_BCD[firstup*16+seconddn];



//				sprintf(TmpPath, "%4.4X %4.4X %4.4X     %4.4X %c\r\n", firstup, seconddn, res, sbb, (sbb!=res)?'*':' ');
//				Kwrite(f, TmpPath, strlen(TmpPath));
			}
		}
		Kclose(f);
	}
}
*/

void HP85OnTimer() {

	DWORD	t;
	long	c;

// Originally I was going to "run" the HP-85 emulation code during WM_TIMER
// messages, but they are too erratic for the emulator to run smoothly, so
// rather than the following line, I chose to go with the HP85OnIdle() call
// in the main message pump loop in MAIN.C.  However, this might be a better
// route, if (someday) investigation discovers the problems and solutions.
//	if( Halt85==FALSE ) Exec85(6400);

	if( ActiveDlg!=NULL ) {
		// for cursor blinking, etc.
		SendMsg((WND *)ActiveDlg, DLG_TIMER, KGetTicks(), 0, 0);
		return;
	}
	if( Halt85 ) {

	} else {
		t = KGetTicks();
		if( MouseKey && t-MouseTime > MouseRepeatTime ) {
			SetKeyDown(MouseKey);
			MouseTime = t;
			MouseRepeatTime = REPEAT_FAST;
		}

		// Give IO cards a shot at doing something periodically (no guaranteed time period, but approximately every 25 milliseconds, but don't count on it too much)
		for(c=0; c<16; c++) {
			if( IOcards[c].initProc != NULL ) IOcards[c].initProc(iR_TIMER, Model, c, t);
		}

		WaveOnTimer();
	}
}

BOOL doitonce=TRUE;

//***********************************************************
void HP85OnIdle() {

	DWORD	c;
	BYTE	*pS;
	int		s, maxll;
	WORD	RMidle, Exec, EndExec, w;

	if( Halt85==FALSE ) {
		if( pKeyMem!=NULL ) {
			Exec = 072;
			EndExec = 0111;
			if( Model<2 ) {
				RMidle = 0102434;
				maxll = 96;
			} else {
				RMidle = 0103706;
				maxll = 160;
			}
			for(pS=pKeyPtr; *pS; pS++) if( *pS=='\t' ) *pS = ' ';	// convert all TAB chars to a space
			while( pKeyMem && pKeyPtr && *pKeyPtr ) {
//				while( *pKeyPtr  &&  *pKeyPtr=='\t' ) ++pKeyPtr;
				s = 0;
				while( *pKeyPtr  &&  *pKeyPtr!='\r'  &&  *pKeyPtr!='\n' ) {
					// The following three lines SLOW DOWN the typing when the "Program Development ROM" is present
					// AND the next keycode is the same as the one before it, because the PD ROM was causing the
					// second key to be skipped (de-bounced).
					while( (RomLoad & 2) && *pKeyPtr==pKeyLastKEYCOD && pKeyLastTime+100>KGetTicks() ) ;
					pKeyLastKEYCOD = *pKeyPtr;
					pKeyLastTime = KGetTicks();

					SetKeyDown(*pKeyPtr++);
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>=RMidle+7) && (w<Exec || w>=EndExec))) );
					++s;
				}
				if( s>=maxll ) { // if chars output is longer than possible line length
					SetKeyDown((Model<2)?161:163); // up cursor
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>=RMidle+7) && (w<Exec || w>=EndExec))) );
				}
				if( *pKeyPtr=='\r' || *pKeyPtr=='\n' ) {
					if( *pKeyPtr=='\r' ) {
						++pKeyPtr;
						if( *pKeyPtr=='\n' ) ++pKeyPtr;
					} else ++pKeyPtr;

					// clear the current line
					SetKeyDown((Model<2) ? 160 : 157);  // -LINE
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>RMidle+7) && (w<Exec || w>=EndExec))) );

					// press END LINE to 'enter' this line of text
					SetKeyDown(154);  // END LINE
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>RMidle+7) && (w<Exec || w>=EndExec))) );

					// move left once to the end of the previous line
					SetKeyDown((Model<2) ? 156 : 159);  // LF CURSOR
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>RMidle+7) && (w<Exec || w>=EndExec))) );

					// clear last character at end of previous line, in case it's the EXACT width of the display
					SetKeyDown((Model<2) ? 160 : 157);  // -LINE
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>RMidle+7) && (w<Exec || w>=EndExec))) );

					// move right to start of next line again
					SetKeyDown((Model<2) ? 157 : 170);  // RT CURSOR
					do {
						Exec85(500);
						w = *((WORD *)(Reg+4));
					} while( pKeyMem && pKeyPtr && (Int85 || IsKeyPressed() || ((w<RMidle || w>RMidle+7) && (w<Exec || w>=EndExec))) );
				}
			}
			if( pKeyMem!=NULL ) {
				free(pKeyMem);
				pKeyMem = NULL;
			}
		}
		if( (CfgFlags & CFG_NATURAL_SPEED) ) {	// if running at HP-85 "natural" speed AND we're past the normal startup initialization
			c = 1000;
			while( c && Halt85==FALSE && Cycles/(CYCLESPERSEC/1000) < KGetTicks()-StartTime ) {
				Exec85(5);
				c -= 5;
			}
			while( Cycles >= CYCLESPERSEC ) {
				++SecondsCycles;
				Cycles -= CYCLESPERSEC;
				StartTime += 1000;
			}
		} else {								// if running at fastest possible speed
			Exec85(1000);
		}
	}
}

//***********************************************************
BOOL HP85OnChar(DWORD c) {

	if( pKeyMem!=NULL ) {
		free(pKeyMem);
		pKeyMem = NULL;
		return TRUE;
	}
	if( ActiveDlg!=NULL ) return SendMsg((WND *)ActiveDlg, DLG_CHAR, c, 0, 0);
	if( Halt85 ) {
		switch( c ) {
		 case 'c':
		 case 'C':
			ExecuteMenu(MENU_COMM);
			break;
		 case 'd':
		 case 'D':
			ExecuteMenu(MENU_ADDR);
			break;
		 case 'e':
		 case 'E':
			ExecuteMenu(MENU_EDIT);
			break;
		 case 'x':
		 case 'X':
			ExecuteMenu(MENU_EXP);
			break;
		 case 'o':
		 case 'O':
			ExecuteMenu(MENU_OPTS);
			break;
		 case 'r':
		 case 'R':
			ExecuteMenu(MENU_ROMS);
			break;
		 case 'j':
		 case 'J':
			if( JumpAddress != 0xFFFFFFFF ) {
				if( BackAddressCnt < sizeof(BackAddress)/sizeof(DWORD) ) BackAddress[BackAddressCnt++] = (LineIndex[CursLine]>>16) & 0x0000FFFF;
				if( JumpAddress > 0x00010000-TraceLines ) JumpAddress = (DWORD)(0x00010000-TraceLines);
				DisStart = (JumpAddress<<16) | 0x0000FFFF;
				CursLine = 0;
				DrawDebug();
			}
			break;
		}
	} else {
		if( !(Int85 & INT_KEY) ) {
			SetKeyDown((BYTE)c);
		}
	}
	return TRUE;
}

//*************************************
// searches KeysXX[] for the HP85 key that's located around point (x,y)
// returns the KeysXX[] index number of that key, or -1 if none
long WhatKey(long x, long y) {

	long	k, kx, ky, m;

	m = (CfgFlags & CFG_BIGCRT) ? 2 : 1;
	if( Model < 2 ) {
		for(k=0; k<sizeof(Keys85)/sizeof(HP85KEY); k++) {
			kx = KEYx + m*Keys85[k].x;
			ky = KEYy + m*Keys85[k].y;
			if( x>=kx && y>=ky && x<kx+m*Keys85[k].w && y<ky+m*Keys85[k].h ) return k;
		}
	} else {
		for(k=0; k<sizeof(Keys87)/sizeof(HP85KEY); k++) {
			kx = KEYx + m*Keys87[k].x;
			ky = KEYy + m*Keys87[k].y;
			if( x>=kx && y>=ky && x<kx+m*Keys87[k].w && y<ky+m*Keys87[k].h ) return k;
		}
	}
	return -1;
}

#define	MKOFF	20	// offset amount for drawing DEPRESSED HP-85 keys on screen

//***********************************************************
void DrawMouseKey() {

	long	x, y, w, h, m;

	m = (CfgFlags & CFG_BIGCRT) ? 2 : 1;

	if( Model < 2 ) {
		x = m*Keys85[MouseIndex].x;
		y = m*Keys85[MouseIndex].y;
		w = m*Keys85[MouseIndex].w;
		h = m*Keys85[MouseIndex].h;
	} else {
		x = m*Keys87[MouseIndex].x;
		y = m*Keys87[MouseIndex].y;
		w = m*Keys87[MouseIndex].w;
		h = m*Keys87[MouseIndex].h;
	}

	if( MouseKey ) {	// it's down
		RectBMB(KWnds[0].pBM, KEYx +x, KEYy + y, KEYx + x + w, KEYy + y + h, CLR_DNKEYBACK, CLR_NADA);
		BltBMB(KWnds[0].pBM, KEYx + x, KEYy + y + h/MKOFF, keyBM, x + h/MKOFF, y, w-h/MKOFF, h-h/MKOFF, TRUE);
	} else {	// it's up
		BltBMB(KWnds[0].pBM, KEYx + x, KEYy + y, keyBM, x, y, w, h, TRUE);
	}
}

//***********************************************************
void HP85OnLButtDown(DWORD x, DWORD y, DWORD flags) {

	long	k;

	if( ActiveDlg!=NULL ) {
		SendMsg((WND *)ActiveDlg, DLG_LBUTTDOWN, x, y, flags);
		return;
	}
	if( Halt85 ) {
		if( (long)x>=TraceX && (long)x<(TraceX+TraceW) && (long)y>=(TraceY+12) && (long)y<(TraceY+TraceH) ) {
			CursLine = (y-TraceY-10)/12;
			if( CursLine >= TraceLines ) CursLine = TraceLines-1;
			DrawDebug();
		}
	} else {
		if( -1!=(k=WhatKey(x, y)) ) {
			if( !(Int85 & INT_KEY) ) {
				if( Model<2 ) MouseKey = (BYTE)((ShiftDown)?Keys85[k].shiftkeycod:Keys85[k].keycod);
				else MouseKey = (BYTE)((ShiftDown)?Keys87[k].shiftkeycod:Keys87[k].keycod);
				MouseIndex = k;
				SetKeyDown(MouseKey);
				MouseTime = KGetTicks();
				MouseRepeatTime = REPEAT_SLOW;
				DrawMouseKey();
			}
		}
	}
}

//***********************************************************
void HP85OnLButtUp(long x, long y, DWORD flags) {

	long	k, r, c;
	DWORD	v;

	if( ActiveDlg!=NULL ) {
		SendMsg((WND *)ActiveDlg, DLG_LBUTTUP, x, y, flags);
		return;
	}
	if( Halt85==FALSE || ActiveDlg==NULL ) {
		if( Model<2 && x>=TAPEx && y>=TAPEy && x<TAPEx+tapeBM->physW && y<TAPEy+tapeBM->physH/4 ) {
			if( flags & 8 ) {	// CTL down, type MASS STORAGE IS ":T"
				KeyMemLen = 256;
				if( NULL != (pKeyMem = (BYTE *)malloc(KeyMemLen)) ) {
					sprintf((char*)pKeyMem, "MASS STORAGE IS \":T\"\r");
					pKeyPtr = pKeyMem;
				}
			} else {
				CreateTapeDlg();
			}
			return;
		}
	}

	if( Halt85 ) {
		MouseKey = 0;

		if( ActiveDlg==NULL ) {
			c = x-REGx-77;
			r = y-REGy-REGh;
			if( c>=0 && r>=0 ) {
				c /= REGw;
				r /= REGh;
				if( c<8 && r<8 ) {	// if clicked on CPU register 00-77
					r = 8*r+c;

					if( ShiftDown ) {
						if( r<040 ) {
							if( r & 1 ) v = Reg[r];
							else v = (((DWORD)Reg[r+1])<<8) | (DWORD)Reg[r];
						} else if( (r&7)<4 ) v = (((DWORD)Reg[r+1])<<8) | (DWORD)Reg[r];
						else if( (r&7)==4 ) v = (((DWORD)Reg[r+3])<<24) | (((DWORD)Reg[r+2])<<16) | (((DWORD)Reg[r+1])<<8) | (DWORD)Reg[r];
						else if( (r&7)==5 ) v = (((DWORD)Reg[r+2])<<16) | (((DWORD)Reg[r+1])<<8) | (DWORD)Reg[r];
						else if( (r&7)==6 ) v = (((DWORD)Reg[r+1])<<8) | (DWORD)Reg[r];
						else v = Reg[r];
						EditReg(0100+r, v);
					} else {
						v = Reg[r];
						EditReg(r, v);
					}
				}
			} else if( x>=REGx && x<REGx+REGw+12 && y>=REGy+REGh && y<REGy+9*REGh ) {	// clicked on status FLAG
				r = (y-REGy-REGh)/REGh;
				Flags ^= (1<<(7-r));
				DrawDebug();
			} else if( y>=REGy+19*REGh/2 && y<REGy+23*REGh/2 ) {	// possible DRP/ARP/E register
				if( x>=REGx && x<REGx+REGw ) {					// DRP
					EditReg(0200, Drp);
				} else if( x>=REGx+REGw && x<REGx+2*REGw ) {	// ARP
					EditReg(0201, Arp);
				} else if( x>=REGx+2*REGw && x<REGx+3*REGw ) {	// E
					EditReg(0202, E);
				}
			}
		}
	} else {
		if( -1!=(k=WhatKey(x, y)) ) {

		}
		if( MouseKey ) {
			MouseKey = 0;
			DrawMouseKey();
		}
	}
	if( ActiveDlg != NULL ) {
		SendMsg((WND *)ActiveDlg, DLG_LBUTTUP, x, y, flags);
		return;
	}
	if( x>=MENUx && x<MENUx+MENUw && y>=MENUy && y<MENUy + MENUh*(long)MenuCnt ) {
		// just to be sure and safe...
		CurMenuItem = (y-MENUy)/MENUh;
		DrawMenu(KWnds[0].pBM);
		// execute the menu item
		ExecuteMenu(CurMenuItem);
	} else if( x>=BRTx && x<BRTx+BRTw && y>=BRTy && y<BRTy + BRTw ) {	// - BRIGHTNESS
		v = ((CRTbrightness & 0x00FF)-5)/25;
		if( v ) {
			v = 25*(v-1)+5;
			CRTbrightness = (v<<16) | (v<<8) | v;
			HP85InvalidateCRT();
			DrawBrightness(KWnds[0].pBM);
		}
	} else if( x>=BRTx && x<BRTx+BRTw && y>=BRTy+BRTh-BRTw && y<BRTy + BRTh ) {	// + BRIGHTNESS
		v = ((CRTbrightness & 0x00FF)-5)/25;
		if( v<10 ) {
			v = 25*(v+1)+5;
			CRTbrightness = (v<<16) | (v<<8) | v;
			HP85InvalidateCRT();
			DrawBrightness(KWnds[0].pBM);
		}
	}
}

//***********************************************************
void HP85OnMButtDown(DWORD x, DWORD y, DWORD flags) {

}

//***********************************************************
void HP85OnMButtUp(DWORD x, DWORD y, DWORD flags) {

/*
switch( flags & 0x0C ) {
 case 0:	// MIDDLE-CLICK = BASIC program load into memory
	Import(0);
	break;
 case 4:	// SHIFT-MIDDLE-CLICK = LOADBIN BPGM into memory
	Import(2);
	break;
 case 8:	// CTL-MIDDLE-CLICK = ASSM source aload into memory
	Import(1);
	break;
 case 0x0C:	// SHIFT-CTL-MIDDLE-CLICK = ASCII TEXT file to GET (as keystrokes)
	Import(3);
	break;
}
*/
			// save CPU state for drawing "changed items" in debugger
			memcpy(LastReg, Reg, sizeof(LastReg));
			LastE = E;
			LastFlags = Flags;
			LastArp = Arp;
			LastDrp = Drp;

			Exec85(1);
			ForceDisStart();
			DrawDebug();

}

//***********************************************************
void HP85OnRButtDown(DWORD x, DWORD y, DWORD flags) {

	if( Halt85 ) {
		if( ActiveDlg!=NULL ) return;
		if( (long)x>=TraceX && (long)x<(TraceX+TraceW) && (long)y>=(TraceY+12) && (long)y<(TraceY+TraceH) ) {
			CursLine = (y-TraceY-10)/12;
			if( CursLine >= TraceLines ) CursLine = TraceLines-1;
			DrawDebug();
		}
	}
}

//***********************************************************
void HP85OnRButtUp(DWORD x, DWORD y, DWORD flags) {

	DWORD	i;

	if( Halt85 ) {
		if( ActiveDlg!=NULL ) return;
		if( (long)x>=TraceX && (long)x<(TraceX+TraceW) && (long)y>=(TraceY+12) && (long)y<(TraceY+TraceH) ) {
			i = (y-TraceY-10)/12;
			if( i >= TraceLines ) i = TraceLines-1;
			if( i==CursLine ) {
				ExecuteMenu(MENU_EDIT);
			}
		}
	}
}


//***********************************************************
void HP85OnMouseMove(long x, long y, DWORD flags) {

	if( ActiveDlg != NULL ) {
		SendMsg((WND *)ActiveDlg, DLG_PTRMOVE, (DWORD)x, (DWORD)y, flags);
		return;
	}

	// check if on menu
	if( x>=MENUx && x<MENUx+MENUw && y>=MENUy && y<MENUy + MENUh*(long)MenuCnt ) {
		if( (y-MENUy)/MENUh != CurMenuItem ) {	// selection has changed
			CurMenuItem = (y-MENUy)/MENUh;
			DrawMenu(KWnds[0].pBM);
		}
	} else if( CurMenuItem >= 0 ) {	// if we've moved OFF of the menu
		CurMenuItem = -1;
		DrawMenu(KWnds[0].pBM);
	}
}

//***********************************************************
void HP85OnWheel(long x, long y, long count) {

	if( Halt85 && ActiveDlg==NULL ) {	// scroll disassembly
		if( count>0 ) {
			ScrollUp(count);
		} else {
			ScrollDn(-count);
		}
	}
	if( !Halt85 && ActiveDlg==NULL ) {
		if( Model<2 && x>=PRTx && x<PRTx+PRTw && y<PRTy && y>PRTy-PRTh ) {	// scroll internal printer printout
			if( count>0 ) {
				while( PRTlen - PRTscroll > PRTh && count--) {
					PRTscroll += PTH;
					DrawPrinter();
				}
			} else {
				while( PRTscroll && count++ ) {
					PRTscroll -= PTH;
					DrawPrinter();
				}
			}
		} else if( x>=CRTx && x<CRTx+hp85BM->physW && y>=CRTy && y<CRTy+hp85BM->physH ) {
			if( count<0 ) SetKeyDown((Model<2) ? 0236 : 0221);
			else SetKeyDown((Model<2) ? 0237 : 0251);
		}
	}
}

//*******************************************************
// Returns TRUE if Plotter/Printer ROM is installed
// HUGE kludge to make auto-setting of default MSUS at
// power-on work right, due to HPIB card interrupts
// needing to happen correctly, and that depends upon
// whether the PP ROM or the MS ROM has IRQ20 and...kludge.
BOOL PPromPresent() {return RomLoad & 0x20000;}
