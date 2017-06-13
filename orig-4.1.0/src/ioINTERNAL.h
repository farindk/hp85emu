#ifndef ioINTincluded
#define	ioINTincluded

// NOTE: the TAPEK below, which specifies the Kbytes length of a tape cartridge,
// is not designed to be changed.  It was here to make my trial and error design
// of the tape simulation easier.  Changing it NOW will cause havoc with any "tapes"
// created earlier at the "original" tape size of 196 Kbytes / track.  In other words,
// DON'T try to make longer (or shorter) tapes by changing it.  Leave it as-is.
#define	TAPEK		196
// The TAPREV is stored into the first word of TAPBUF before a "tape" is written to
// its disk file.  This is included in case any changes or additions to the tape
// format needs to be made in the future.  Currently, the first few WORDs of the
// TAPBUF image are used for:
//	0	tape format revision (TAPREV)
//	1	write protect, 0000 or 0010, the lower byte of which gets OR'd into TAPSTS
//	2-3	a DWORD which gets stored into TAPPOS when the tape is "inserted" (loaded)
//		which controls the initial "tape position", just as a real physical tape
//		would be left positioned where it was when it was ejected.  This isn't really
//		necessary, it could always be positioned at the rewind position every time
//		it's loaded, but this adds to the "reality" of the simulation.
#define	TAPREV		0

#define	SetTapeStatus() {\
				WORD	td;\
				td = TAPBUF[IO_TAPCTL & 1][TAPPOS];\
				if( td & TAP_HOLE ) IO_TAPSTS |= 0020;\
				if( !(TAPPOS & 1) ) IO_TAPSTS |= 0100;\
				else IO_TAPSTS &= ~0100;\
				if( (IO_TAPCTL & 0006)==0006 ) {\
					if( !(td & TAP_GAP) ) IO_TAPSTS &= ~0040;\
					else IO_TAPSTS |= 0040;\
					if( td==0 ) IO_TAPSTS = (IO_TAPSTS & 0111);\
				}\
			}

#define	TAP_GAP		0x8000
#define	TAP_SYNC	0x4000
#define	TAP_DATA	0x2000
#define	TAP_HOLE	0x1000
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
BOOL GINTen();
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


#endif // ioINTincluded
