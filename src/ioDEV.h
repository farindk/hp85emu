#ifndef DEVincluded
#define	DEVincluded

///***************************************************************************
///***************************************************************************
///
/// INTERNAL I/O STUFF
///
///***************************************************************************
///***************************************************************************

/*
extern BOOL	IO_KeyEnabled;
extern BYTE	IO_KEYCTL;
extern BYTE	IO_KEYSTS;
extern BYTE	IO_KEYHOMSTS;	// for HP-87 faking of PC HOME key to having SHIFT down on the HP-87
extern BYTE	IO_KEYCOD;
extern BOOL	IO_KEYDOWN;	// whether the most recent keystroke is being "held down" for the HP-85's purposes
extern DWORD	IO_KEYTIM;		// KGetTicks() when most recent keyboard interrupt occurred

extern WORD	IO_CRTSAD;
extern BOOL	IO_CRTSADbyte;	// 0 if next write to CRTSAD is first byte of 2-byte address, 1 if it's the 2nd byte
extern WORD	IO_CRTBAD;
extern BOOL	IO_CRTBADbyte;	// 0 if next write to CRTBAD is first byte of 2-byte address, 1 if it's the 2nd byte
extern BYTE	IO_CRTCTL;
extern BYTE	IO_CRTSTS;

extern BYTE	*CRT;

extern BYTE	IO_TAPCTL;
extern BYTE	IO_TAPSTS, IO_TAPCART;
extern BYTE	IO_TAPDAT;	// byte written to TAPDAT, should be able to be read back from TAPDAT

extern WORD	TAPBUF[2][TAPEK*1024];	// 2 tracks, 128Kbytes / track (including GAPs, SYNCs, HEADERs, DATA, etc)
extern DWORD	TAPPOS;		// current position of tap read/write head on the tape (ie, in TAPBUF)
extern BOOL	TAP_ADVANCE;
extern char	CurTape[64];

extern BYTE	IO_CLKSTS;
extern BYTE	IO_CLKCTL;
// bits 0-3 of IO_CLKCTL are used (=1) to indicate that timer is running.
// bits 4-7 of IO_CLKCTL are used (=1) to indicate that timer is interrupting
extern BYTE	IO_CLKDAT[4][4];
extern DWORD	IO_CLKbase[4];	// base REAL computer milliseconds count which == IO_CLKcount[] of 0.
						// ie, IO_CLKcount[] == LIMIT of when clock interrupts and resets its counter to 0.
						//	REALmilliseconds - IO_CLKbase[] = current millisecond count of timer
extern DWORD	IO_CLKcount[4];
extern WORD		IO_CLKnum;
extern WORD		IO_CLKDATindex;

extern BYTE	IO_PRCHAR;		// index into printer's "font ROM" of character to read data of
extern BYTE	IO_PRTSTS;		// current printer state (what's returned to a read from PRTSTS)
extern BYTE	IO_PRTCTL;		// what was last written to PRTSTS
extern BYTE	IO_PRTDAT;		// holds next byte of data for reading from "font ROM"
extern BYTE	IO_PRTLEN;		// how many bytes the HP-85 has told us it's sending to the printer
extern BYTE	IO_PRTCNT;		// how many bytes have so far been sent to the printer (into IO_PRTBUF)
extern BYTE	IO_PRTBUF[192];	// for buffering bytes being sent to the printer
extern BOOL	IO_PRTDMP;		// if TRUE, IO_PRTBUF has data that needs dumping to the printer
*/
extern DWORD	CRTbrightness;

void SetTapeWriteProtect(char *tapename, BOOL wp);
BOOL GetTapeWriteProtect(char *tapename);
void DrawTapeStatus(PBMB hBM, long tx, long ty);
void DrawTape(PBMB hBM, long tx, long ty);
void AdjustTime(DWORD t);
void SetClockBase(long timer, DWORD t);
void InitClocks();
void CheckTimerStuff(DWORD curticks);
void InitCRT();
void InitPRT85();
void InitKeyboard();
void InitTape();
void DrawCRT(PBMB hBM, long x, long y);
void DrawBrightness(PBMB hBM);
void DrawRunLite();
void FixCRTbytes(WORD addr);
#endif
BOOL GINTen();
#if TODO
void KEYINTgranted();
void PWONinternal(BOOL first);
void PWOFFinternal();
void SetShift(BOOL down);
BOOL IsKeyPressed();
void SetKeyPressed(BOOL s);
void SetHomeKey(BOOL down);
void SetKeyDown(BYTE k);
BOOL IsPageSize24();

//*****************************************************************
// I/O read/write function for I/O addresses
// Both reading and writing return a value.
// If 'val' is from 0-255, then it's a WRITE and the return value is undefined.
// If 'val' is outside the 0-255 range, then it's a READ and the read value is returned (0-255)
BYTE ioNULL(WORD address, long val);
BYTE ioGINTEN(WORD address, long val);
BYTE ioGINTDS(WORD address, long val);
BYTE ioKEYSTS(WORD address, long val);
BYTE ioKEYCOD(WORD address, long val);
BYTE ioCRTSAD85(WORD address, long val);
BYTE ioCRTBAD85(WORD address, long val);
BYTE ioCRTSTS85(WORD address, long val);
BYTE ioCRTDAT85(WORD address, long val);
BYTE ioTAPSTS(WORD address, long val);
BYTE ioTAPDAT(WORD address, long val);
BYTE ioCLKSTS(WORD address, long val);
BYTE ioCLKDAT(WORD address, long val);
BYTE ioPRTLEN(WORD address, long val);
BYTE ioPRCHAR(WORD address, long val);
BYTE ioPRTSTS(WORD address, long val);
BYTE ioPRTDAT(WORD address, long val);
BYTE ioCRTSAD87(WORD address, long val);
BYTE ioCRTBAD87(WORD address, long val);
BYTE ioCRTSTS87(WORD address, long val);
BYTE ioCRTDAT87(WORD address, long val);
BYTE ioRSELEC(WORD address, long val);
BYTE ioPTR1(WORD address, long val);
BYTE ioPTR2(WORD address, long val);
BYTE ioRULITE(WORD address, long val);

///***************************************************************************
///***************************************************************************
///
/// IO CARD/DEVICE STUFF
///
///***************************************************************************
///***************************************************************************
//************************
// I/O Card structure
//	typeID values (obviously, most of these are not implemented in this emulator):

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

//	ALL data and variables necessary to 'implement' (control) a 'card' MUST
//	be kept in a single data structure pointed to by the 'data' variable of
//	this structure.  That's because a system might have multiple HP-IB cards
//	(for example) with varying sets of devices "on that HP-IB interface", and
//	different for the three different computer models the emulator implements.
//	So the same code module that implements HP-IB should be able to 'implement'
//	multiple HP-IB cards (and at different select codes) simply by setting up
//	unique data structures for each one and not using ANY global variables
//	that would be card-specific.

typedef struct {
	WORD	address;	// base I/O address (like 177530, etc)
	WORD	numaddr;	// number of sequential I/O addresses used (usually 2)
	long	type;		// see IOTYPE_ #define list
	int		(*initProc)(long reason, long model, long sc, DWORD arg);	// functional to call for setup/de-setup/etc
	void	*data;		// S.C.-specific pointer to the card's data structure (up to the card's emulator code to set up during 'setup' call to initProc()
} IOCARD;

extern IOCARD	IOcards[16];

#define	iR_STARTUP		1	// program starting up, read INI variables, allocate resources, initialize
#define	iR_SHUTDOWN		2	// program is shutting down, write INI variables, give up any resources
#define	iR_ENINT		3	// enable IO card to interrupt CPU
#define	iR_DISINT		4	// disable IO card from interrupting CPU
#define	iR_TIMER		5	// periodic timer (roughly every 25 milliseconds)
#define	iR_RESET		6	// reset the card and peripherals
#define	iR_WRITE_INI	7	// write INI records for this interface and devices
#define	iR_READ_INI		8	// read INI records for this interface and devices
#define	iR_DRAW			9	// draw anything the IO card supports that needs drawing
#define	iR_FLUSH		10	// flush out any disk changes to real storage

///***************************************************************************
///***************************************************************************
///
/// HPIB STUFF
///
///***************************************************************************
///***************************************************************************

#define	TLA85	21	// Series 80 Computer (controller)

#define	IOSTATE_IDLE		0	// doing nothing in particular
#define	IOSTATE_WAITFORCMD	1	// waiting for an HP-IB command (like UNL, MTA, etc)

#define	DISKF_AUTOCAT0	0x00000001
#define	DISKF_AUTOCAT1	0x00000002

#define	DP_INI_READ		0	// read settings from INI file
#define	DP_INI_WRITE	1	// write settings to INI file
#define	DP_FLUSH		2	// dump/terminate any ongoing output
#define	DP_RESET		3	// RESET device
#define	DP_STARTUP		4	// device is starting up (at program start-up)
#define	DP_SHUTDOWN		5	// device is shutting down (at program exit)
#define	DP_INI_CREATE	6	// device was ADDED in the DEVICES dialog, do modified DP_INI_WRITE
#define	DP_INPUT_DONE	7	// the IB has been emptied
#define	DP_NEWCMD		8	// new IO CARD command, chance to clean up anything left hanging (NOT being SENT a command to execute, just a "timing" notification)
#define	DP_OUTPUT		9
#define	DP_TERMBURST	10
#define DP_HPIB_CMD		11
#define	DP_TIMER		12
#define	DP_DRAW			13
#define	DP_PPOLL		14	// returns TRUE if disk device is READY

/// ALL disk drive DEV_ types *MUST* be <= DEV_MAX_DISK_TYPE
#define	DEV_DEFAULT_82901		0
#define	DEV_MAX_DISK_TYPE		9
#define	DEV_DEFAULT_FILEPRT		10
#define	DEV_DEFAULT_HARDPRT		20

#define	DEVTYPE_5_25		0
#define	DEVTYPE_3_5			1
#define	DEVTYPE_8			2
#define	DEVTYPE_5MB			3

#define	DEVTYPE_FILEPRT		10
#define	DEVTYPE_HARDPRT		11
#define	DEVTYPE_NONE		-1

#define	DEVSTATE_BOOT		0	// existed in INI file
#define	DEVSTATE_NEW		1	// not in INI file, but ADDED in DEVICES dialog
#define	DEVSTATE_DELETED	2	// in INI file, but DELETED in DEVICES dialog
#define	DEVSTATE_UNUSED		3	// not in INI file, not ADDED in DEVICES, just non-existent, unwanted, a sad sorry state of device

typedef struct {
	int		devID;		// unique device number
	int		devType;	// DEVTYPE_ (DEVTYPE_DISKETTE, DEVTYPE_FILEPRT, DEVTYPE_HARDPRT, etc)
	int		ioType;		// IOTYPE_ (always IOTYPE_HPIB for now)
	int		sc, wsc;	// Select Code of IO card this device is on, and SC to WRITE to the INI file (for purposes of moving devices around)
	int		ca, wca;	// Controller Address (for HPIB) at which this device exists, and CA to WRITE to the INI file (for purposes of moving it)
	int		state;		// DEVSTATE_ whether device at boot-time, or added or deleted in DEVICES dialog
	int		(*initProc)(long reason, int sc, int tla, int d, int val);
} DEV;

typedef struct {
	BYTE	id[2];
	int		units, heads, cylinders, cylsects, devType;
	PBMB	*pBM;
} DISKPARM;

DISKPARM	DiskParms[10];

int initHPIB(long reason, long model, long sc, DWORD arg);
void ResetHPIB(int	sc);
void DrawDiskStatus(int sc, int tla, int d);
void DrawDisks(int sc, int tla);
void WriteDisk(int sc, int tla, int d);
void EjectDisk(int sc, int tla, int d);
void NewDisk(int sc, int tla, int d, BYTE *pName);
BOOL LoadDisk(int sc, int tla, int d, BYTE *pName);

void GetDiskListRect(int sc, int tla, int d, RECT *r);
void DiskMouseClick(int sc, int tla, int x, int y, int flags);
void DirDisk(int sc, int tla, int d);
BOOL IsDisk(int sc, int tla, int drive);
BOOL IsDiskInitialized(int sc, int tla, int drive);
BOOL IsDiskProtected(int sc, int tla, int drive);
char *GetDiskName(int sc, int tla, int d);
void WriteDiskByte(int sc, int tla, int d, int offset, BYTE p);
BYTE ReadDiskByte(int sc, int tla, int d, int offset);
int GetDiskFolder(int sc, int tla, int d);
void SetDiskFolder(int sc, int tla, int d, int f);;
void ImportFile(int sc, int tla, int d, char *osName, char *lifN);
void ExportLIF(int sc, int tla, int d, char *lifN, char *osName);
void RelocateDisks(int sc, int tla, int d);
void AdjustDisks(int sc, int tla);
void *CreateDevData(int sc, int tla, int cnt, int devType);
void *GetDevData(int sc, int tla, int devType);
void DestroyDevData(int sc, int tla);
BOOL IsTalker(int sc, int tla);
BOOL IsListener(int sc, int tla);
BOOL TLAavailable(int sc, int tla);
void TLAset(int sc, int tla, int (*pDevProc)(long reason, int sc, int tla, int unit, int arg));
void SetOBptr(int sc, BYTE *ptr);
BYTE *GetOBptr(int sc);
void PushOB(int sc, BYTE b);
void HPIBint(int sc, BYTE *pB, int blen);
void HPIBinput(int sc, BYTE *pB, int blen);
BOOL InBurst(int sc);
void SetBurst(int sc, int mode, int cnt);
int CurDiskUnit(int sc, int tla);
void GenWriteProtectError(int sc, int tla);
int BurstTLA(int sc);
void StartBurstIn(int sc, BYTE *pB, int cnt);
void SetHPIBstate(int sc, BYTE state);
BYTE GetHPIBstate(int sc);
BOOL IsCED(int sc);
void SetPED(int sc);

extern WORD	BurstInt85;
extern int	DiskSize[8][32];		// size of disk for [sc][tla] disk drives (set at iR_READ_INI time)

///***************************************************************************
///***************************************************************************
///
/// EXTERNAL DEVICES STUFF
///
///***************************************************************************
///***************************************************************************

int devFilePRT(long reason, int sc, int tla, int d, int val);
int devHardPRT(long reason, int sc, int tla, int d, int val);
int devDISK(long reason, int sc, int tla, int d, int val);

#endif // DEVincluded
