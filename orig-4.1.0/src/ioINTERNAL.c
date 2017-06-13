#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include	"main.h"
#include	"mach85.h"
#include	"beep.h"

BOOL	SaveIOcIP=FALSE;

BOOL	IO_KeyEnabled;
BYTE	IO_KEYCTL;
BYTE	IO_KEYSTS;
BYTE	IO_KEYHOMSTS=0;	// for HP-87 faking of PC HOME key to having SHIFT down on the HP-87
BYTE	IO_KEYCOD;
BOOL	IO_KEYDOWN=FALSE;	// whether the most recent keystroke is being "held down" for the HP-85's purposes
DWORD	IO_KEYTIM;		// KGetTicks() when most recent keyboard interrupt occurred

WORD	IO_CRTSAD;
BOOL	IO_CRTSADbyte;	// 0 if next write to CRTSAD is first byte of 2-byte address, 1 if it's the 2nd byte
WORD	IO_CRTBAD;
BOOL	IO_CRTBADbyte;	// 0 if next write to CRTBAD is first byte of 2-byte address, 1 if it's the 2nd byte
BYTE	IO_CRTCTL;
BYTE	IO_CRTSTS;

BYTE	*CRT=NULL;

BYTE	IO_TAPCTL;
BYTE	IO_TAPSTS, IO_TAPCART;
BYTE	IO_TAPDAT;	// byte written to TAPDAT, should be able to be read back from TAPDAT

WORD	TAPBUF[2][TAPEK*1024];	// 2 tracks, 128Kbytes / track (including GAPs, SYNCs, HEADERs, DATA, etc)
DWORD	TAPPOS;		// current position of tap read/write head on the tape (ie, in TAPBUF)
BOOL	TAP_ADVANCE;
char	CurTape[64];

#define	TAP_GAP		0x8000
#define	TAP_SYNC	0x4000
#define	TAP_DATA	0x2000
#define	TAP_HOLE	0x1000

BYTE	IO_CLKSTS;
BYTE	IO_CLKCTL;
// bits 0-3 of IO_CLKCTL are used (=1) to indicate that timer is running.
// bits 4-7 of IO_CLKCTL are used (=1) to indicate that timer is interrupting
BYTE	IO_CLKDAT[4][4];
DWORD	IO_CLKbase[4];	// base REAL computer milliseconds count which == IO_CLKcount[] of 0.
						// ie, IO_CLKcount[] == LIMIT of when clock interrupts and resets its counter to 0.
						//	REALmilliseconds - IO_CLKbase[] = current millisecond count of timer
DWORD	IO_CLKcount[4];
WORD	IO_CLKnum;
WORD	IO_CLKDATindex;

BYTE	IO_PRCHAR;		// index into printer's "font ROM" of character to read data of
BYTE	IO_PRTSTS;		// current printer state (what's returned to a read from PRTSTS)
BYTE	IO_PRTCTL;		// what was last written to PRTSTS
BYTE	IO_PRTDAT;		// holds next byte of data for reading from "font ROM"
BYTE	IO_PRTLEN;		// how many bytes the HP-85 has told us it's sending to the printer
BYTE	IO_PRTCNT;		// how many bytes have so far been sent to the printer (into IO_PRTBUF)
BYTE	IO_PRTBUF[192];	// for buffering bytes being sent to the printer
BOOL	IO_PRTDMP;		// if TRUE, IO_PRTBUF has data that needs dumping to the printer

BYTE	RULITEblink=0;
DWORD	RULITEtime;
BOOL	RULITEon=TRUE;

DWORD	CRTbrightness;	// color of "white" dots on CRT

BOOL GINTen() { return (IO_KEYSTS & 128); }

void InitClocks() {

	long	i;

	for(i=0; i<4; i++) IO_CLKcount[i] = 0;
	IO_CLKnum = IO_CLKDATindex = 0;
	IO_CLKSTS = 0x80;
	IO_CLKCTL = 0;
}

void AdjustTime(DWORD t) {

	IO_CLKbase[0] += t;
	IO_CLKbase[1] += t;
	IO_CLKbase[2] += t;
	IO_CLKbase[3] += t;
}

void CheckTimerStuff(DWORD curticks) {

	long	i;

	if( IO_CLKDATindex==0 ) {	// not in middle of write to CLKDAT
		for(i=0; i<4; i++) {
			if( (IO_CLKCTL & (1<<i)) && !(IO_CLKCTL & (0x10<<i)) ) {	// timer is running and not already interrupting
				if( IO_CLKcount[i] && IO_CLKcount[i] <= (curticks - IO_CLKbase[i]) ) {	// timer has expired
					IO_CLKCTL |= (0x10<<i);		// set interrupt FF
					Int85 |= (INT_CL0 << i);	// set interrupt for this timer
					IO_CLKCTL |= (0x10<<i);		// set interrupt FF for this timer
				}
			}
		}
	}
	if( RULITEblink ) {
		if( curticks - RULITEtime > 250 ) {
			RULITEtime = curticks;
			RULITEon = !RULITEon;
			DrawRunLite();
		}
	}
}

void SetClockBase(long timer, DWORD t) {

	IO_CLKbase[timer] = t;
}

void InitKeyboard() {

	IO_KEYSTS = 0x00;
	IO_KEYDOWN = FALSE;

	// turn off 87 RunLite blinking
	RULITEblink = 0;
	RULITEon = TRUE;
}

void KEYINTgranted() {

	Int85 &= ~INT_KEY;
	IO_KEYTIM = KGetTicks();
	IO_KEYDOWN = TRUE;
}

void SetShift(BOOL down) {

	if( down ) IO_KEYSTS |= 8;
	else IO_KEYSTS &= ~8;
}

BOOL IsKeyPressed() {

	return (IO_KEYSTS & 2);
}

void SetKeyPressed(BOOL s) {

	if( s ) {

	} else {
		IO_KEYSTS &= ~2;
		IO_KEYDOWN = FALSE;
	}
}

void SetHomeKey(BOOL down) {

	IO_KEYHOMSTS = down ? 8:0;
}

void SetKeyDown(BYTE k) {

	IO_KEYCOD = k;
	IO_KEYSTS |= 2;
	Int85 |= INT_KEY;
}

void PWONinternal(BOOL first) {

	if( !first ) {
		if( CRT ) free(CRT);
		CRT = NULL;
		if( hp85BM ) DeleteBMB(hp85BM);
		hp85BM = NULL;
	}
	// init INTERRUPTS
	IO_KeyEnabled = FALSE;

	InitKeyboard();
	InitClocks();

	if( prtBM!=NULL ) DeleteBMB(prtBM);
	prtBM = NULL;

	InitCRT();

	if( Model<2 ) {
		InitPRT85();
		InitTape();
	}
}

void PWOFFinternal() {

	DeleteBMB(hp85BM);
	hp85BM = NULL;
	DeleteBMB(keyBM);
	keyBM = NULL;
	if( tapeBM ) DeleteBMB(tapeBM);
	tapeBM = NULL;
	DeleteBMB(rlBM);
	rlBM = NULL;
	if( CRT!=NULL ) free(CRT);
	CRT = NULL;
}

//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================
//
//						TAPE
//
//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================

void InitTape() {

	IO_TAPSTS = 0240;	// note: OCTAL value
	IO_TAPCART = 0;
	IO_TAPCTL = 0;

	LoadTape();
}

//*********************************************************************
void WriteTape() {

	long	hFile;

	if( Model<2 ) {
		// update TAPPOS and write TAPE file
		sprintf(FilePath, "TAPES\\%s", CurTape);
		if( -1 != (hFile=Kopen(FilePath, O_WRBINNEW)) ) {
			TAPBUF[0][0] = TAPREV;	// update tape design revision#
			*((DWORD *)&TAPBUF[0][2]) = TAPPOS;	// save tape "position"
			Kwrite(hFile, (BYTE *)TAPBUF, sizeof(TAPBUF));	// write to disk any changes to the tape
			Kclose(hFile);
		}
	}
}

//*********************************************************************
void EjectTape() {

	if( Model<2 ) {
		WriteTape();

		IO_TAPSTS &= ~1;
		IO_TAPCART = 0;
	}
}

#define	HOLE_LEN	2
//**************************************************************
void AddTapeHole(long p) {

	long	i;

	for(i=0; i<HOLE_LEN; i++) {
		TAPBUF[0][p+i] = TAPBUF[1][p+i] = TAP_HOLE;
	}
}

//**************************************************************
// End of tape (at END of tape) hole needs to be AT LEAST:
//	2*(((6+2 + 256+2)*1.75) + 80) + 128 = 1219
// bytes from the end of the tape "array", to be sure that
// the last record write on the first track doesn't overflow
// into the REAL "end of tape" holes, getting the code lost.
// So, we put the "end of tape" hole at 1280 back from the end.
//
void NewTape() {

	long	i, j;

	memset(TAPBUF, 0, sizeof(TAPBUF));	// 0's indicate GAP, no flux, no data, nothing, nada
	i = (sizeof(TAPBUF)/sizeof(TAPBUF[0][0]))/2;	// number of "byte positions" on the tape (one track)
	for(j=0; j<128; j+=HOLE_LEN) AddTapeHole(j);	// load front of TAPBUF with BIG "end of tape" hole
	for(j=0; j<128; j+=HOLE_LEN) AddTapeHole(i-j-HOLE_LEN);	// and at the end, also
	AddTapeHole(512);	// first of double-hole
	AddTapeHole(528);	// second of double-hole
	AddTapeHole(528+1536);	// single-hole
	AddTapeHole(i-1280);	// single-hole at tape-end

	TAPPOS = 528+2048;		// position tape to right of single-hole
	TAPBUF[0][0] = TAPREV;
	TAPBUF[0][1] = 0010;		// write-enabled
	*((DWORD *)&TAPBUF[0][2]) = TAPPOS;	// tape position
	IO_TAPSTS = 0250;
	IO_TAPCART = 1;			// signify cartridge inserted, not yet CAT'd
	TAP_ADVANCE = FALSE;
}

//**************************************************************
BOOL LoadTape() {

	long		hFile;

	if( Model>= 2 ) return FALSE;
	if( CurTape[0]==0 ) return FALSE;
	// read TAPE file
	sprintf(FilePath, "TAPES\\%s", CurTape);
	if( -1 == (hFile=Kopen(FilePath, O_RDBIN)) ) return FALSE;

	Kread(hFile, (BYTE *)TAPBUF, sizeof(TAPBUF));
	Kclose(hFile);

	TAPPOS = max(528+2048, *((DWORD *)&TAPBUF[0][2]));	// position tape to previous position (or right of single-hole in case of bug)
// IO_TAPSTS BIT0==0 and IO_TAPCART BIT0==1 tells system that a cartridge is in, but hasn't yet been seen by the HP-85 tape code
	IO_TAPCART = 1;
	IO_TAPSTS  = 0240 | (BYTE)(TAPBUF[0][1]);	// OR-in write-enable flag
	TAP_ADVANCE = FALSE;

	return TRUE;
}

// NOTE: TapBuf[0][1] is actually write ENABLE, not write PROTECT.
// So it's set to 0010 when ENABLED, 0000 when PROTECTED.
// Ditto for taphead[1] in Get/Set TapeWriteProtect() below

//*****************************************************************
// Returns TRUE if 'tapename' is WRITE-PROTECTED
BOOL GetTapeWriteProtect(char *tapename) {

	long	hFile;
	WORD	taphead[2];

	if( *tapename ) {
		strcpy((char*)ScratBuf, "TAPES\\");
		strcat((char*)ScratBuf, tapename);
		if( -1 != (hFile=Kopen((char*)ScratBuf, O_RDBIN)) ) {
			Kread(hFile, (BYTE *)taphead, 2*sizeof(WORD));
			Kclose(hFile);
			// taphead[0] = tape file revision#
			// taphead[1] = write-protect
			return !taphead[1];
		}
	}
	return FALSE;
}

//*****************************************************************
// Changes the state of the WRITE-PROTECT tab on a tape cartridge
void SetTapeWriteProtect(char *tapename, BOOL wp) {

	long	hFile;
	WORD	taphead[2];

	if( !strcmp(CurTape, tapename) ) {	// tape is INSERTED right now!
		IO_TAPSTS ^= 0010;		// toggle Write-Enable in TAPSTS
		// now save the new WriteProtect status in the tape's disk file
		TAPBUF[0][1] = (WORD)(wp ? 0000 : 0010);	// write-enable that can get or'd into TAPSTS
		EjectTape();		// "REMOVE" CARTRIDGE
		IO_TAPCART = 1;		// "INSERT" CARTRIDGE so "write enable" bit gets re-read by HP-85 software
		// NOTE: on a real HP-85, you had to eject the tape cartridge in order
		// to change the WriteProtect switch, and that ejection, of course, caused
		// the tape directory to be invalidated, forcing another search to the front
		// of the cartridge to read the tape directory when you re-inserted the tape.
		// The above code pretty much duplicates this "physical" cartridge change.
	} else if(  *tapename ) {
		strcpy((char*)ScratBuf, "TAPES\\");
		strcat((char*)ScratBuf, tapename);

		if( -1 != (hFile=Kopen((char*)ScratBuf, O_WRBIN)) ) {
			Kread(hFile, (BYTE *)&taphead[0], 2*sizeof(WORD));
			Kseek(hFile, sizeof(WORD), SEEK_SET);
			// taphead[0] = version#
			// taphead[1] = write-protect
			// taphead[2] & taphead[3] = TAPPOS
			taphead[1] = wp ? 0000 : 0010;	// write-enable that can get or'd into TAPSTS

			Kwrite(hFile, (BYTE *)&taphead[1], sizeof(WORD));
			Kclose(hFile);
		}
	}
}

#define	DEBUG_TAPSTS	0
//*****************************************************************
void DrawTapeStatus(PBMB hBM, long tx, long ty) {

	long	x, y, w, h, p, tl;
	char	*s;
	DWORD	clr;

#if DEBUG_TAPSTS
	char	temp[128];
#endif
	if( Model>=2 ) return;
	if( (IO_TAPSTS | IO_TAPCART) & 1 ) {	// only show tracks status if cartridge in
		x = tx;
		y = ty - 3*12;
		w = tapeBM->physW;
		h = 2*12;
		RectBMB(hBM, x, y, x+w, y+h, (IO_TAPSTS & 0010)?CLR_TAPSTS_WE:CLR_TAPSTS_WP, MainWndTextClr);
		LineBMB(hBM, x, y+12, x+w-1, y+12, MainWndTextClr);

		tl = (sizeof(TAPBUF)/sizeof(TAPBUF[0][0]))/2;
		p  = ((w-5)*TAPPOS)/tl;
		if( IO_TAPCTL & 0020 ) {	// hi speed
			clr = CLR_TAPSTS_HI;
			if( IO_TAPCTL & 0010 ) s = ">";	// hi speed forward
			else s = "<";					// hi speed backwards
		} else {					// lo speed
			if( IO_TAPCTL & 0340 ) {		// low speed writing
				clr = CLR_TAPSTS_WR;
			} else {						// low speed reading
				clr = CLR_TAPSTS_RD;
			}
			if( IO_TAPCTL & 0010 ) s = "]";	// lo speed forward
			else s = "[";					// lo speed backwards
		}

		Label85BMB(hBM, x+p, y+3+12*(IO_TAPCTL & 1), 12, (BYTE*)s, 1, clr);
	}
#if DEBUG_TAPSTS
	if( Halt85 ) {
		long	i, t, s;
		DWORD	clr;

		LineBMB(hBM, 0, 1, 1280, 1, CLR_BLACK);

		if( TAPPOS>4*1280 ) s = TAPPOS - 4*1280;
		else s = 0;
		i = (sizeof(TAPBUF)/sizeof(TAPBUF[0][0]))/2-4*1280;
		if( s>i ) s = i;

		for(i=0; i<1280; i++) {
			clr = CLR_BLACK;
			for(x=0; x<8; x++) {
				t = TAPBUF[IO_TAPCTL & 1][s+i*8+x];
				if( clr!=CLR_YELLOW ) {
					if( t & TAP_HOLE ) clr = CLR_YELLOW;
					else if( clr!=CLR_GRAY ) {
						if( t & (TAP_GAP | TAP_SYNC) ) clr = CLR_GRAY;
						else if( clr!=CLR_GREEN ) {
							if( t & TAP_DATA ) clr = CLR_GREEN;
						}
					}
				}
			}
			if( TAPPOS>=(DWORD)(s+8*i) && TAPPOS<(DWORD)(s+8*i+8) ) clr = CLR_RED;
			if( clr!=CLR_BLACK ) PointBMB(hBM, 0+i, 1, clr);
		}
	}
#endif

#if DEBUG_TAPSTS
	x = 10;
	y = TAPEy - 8*12;

	RectBMB(hBM, x, y, x+28*8, y+5*12, CLR_MAINBACK, CLR_NADA);

	h = IO_TAPCTL & 1;
	sprintf(temp, "TAPPOS=%d %4.4X %4.4X %4.4X", TAPPOS, TAPBUF[h][TAPPOS-1], TAPBUF[h][TAPPOS], TAPBUF[h][TAPPOS+1]);
	Label85BMB(hBM, x, y, 12, temp, strlen(temp), CLR_BLACK);
	y += 12;

	sprintf(temp, "CTL=%3.3o", IO_TAPCTL);
	Label85BMB(hBM, x, y, 12, temp, strlen(temp), CLR_BLACK);
	y += 12;
	sprintf(temp, "STS=%3.3o", IO_TAPSTS);
	Label85BMB(hBM, x, y, 12, temp, strlen(temp), CLR_BLACK);
	y += 12;
	sprintf(temp, "DAT=%3.3o", IO_TAPDAT);
	Label85BMB(hBM, x, y, 12, temp, strlen(temp), CLR_BLACK);
	y += 12;

	switch( IO_TAPCTL & 0034 ) {
	 case 0000:
		s = "LO BAK OFF";
		break;
	 case 0004:
		s = "LO BAK ON ";
		break;
	 case 0010:
		s = "LO FWD OFF";
		break;
	 case 0014:
		s = "LO FWD ON ";
		break;
	 case 0020:
		s = "HI BAK OFF";
		break;
	 case 0024:
		s = "HI BAK ON ";
		break;
	 case 0030:
		s = "HI FWD OFF";
		break;
	 case 0034:
		s = "HI FWD ON ";
		break;
	}
	Label85BMB(hBM, x, y, 12, s, strlen(s), CLR_BLACK);
#endif

	FlushScreen();
}

//********************************************************
void DrawTape(PBMB hBM, long tx, long ty) {

	int		y, h;

	if( Model>= 2 ) return;
	h = tapeBM->physH/4;
	y = (((IO_TAPSTS | IO_TAPCART) & 1)*2*h) + h*(IO_TAPCTL & 2)/2;
	BltBMB(hBM, tx, ty, tapeBM, 0, y, tapeBM->physW, h, TRUE);

	if( (IO_TAPSTS | IO_TAPCART) & 1 ) Label85BMB(hBM, tx, ty-10, 12, (BYTE*)CurTape, strlen(CurTape), MainWndTextClr);
	DrawTapeStatus(hBM, tx, ty);
}

//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================
//
//					POWER/RUN LIGHT
//
//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================

//********************************************************
void DrawRunLite() {

	BltBMB(KWnds[0].pBM, RLx, RLy, rlBM, 0, RULITEon?rlBM->physH/2:0, rlBM->physW, rlBM->physH/2, TRUE);
}

//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================
//
//					PRINTER
//
//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================

void InitPRT85() {

	IO_PRTSTS = 0301;	// note: OCTAL value
	// READ PRTSTS:
	//	BIT0 = 1 if printer ready
	//	BIT1-BIT5 = 0 (unused)
	//	BIT6 = 1 if data ready
	//	BIT7 = 1 if paper loaded (0 if out of paper)
	IO_PRCHAR = IO_PRTCTL = IO_PRTDAT = IO_PRTCNT = 0;
	IO_PRTDMP = FALSE;

	prtBM = CreateBMB((WORD)PRTw, (WORD)2048, NULL);	// fixed length "printer paper"
	RectBMB(prtBM, 0, 0, PRTw, 2048, CLR_WHITE, CLR_NADA);
	PRTlen = PTH;	// one alpha line's worth of paper showing
	PRTscroll = 0;	// no internal printer scrolling
}

//************************************************************
// this is different because printed characters omit one blank column of pixels
// making the character cell only 7 pixels wide instead of 8 like on the CRT.
void Print85(PBMB hBM, int x, int y, BYTE *str, DWORD len, DWORD clr) {

	int		cx, cy;
	BYTE	c;

	++x;	// shift fonts right one pixel to put a blank column on left side
	while( len-- ) {
		c = *str++;
		if( c & 128 ) {
			LineBMB(hBM, x-1, y+7, x+5, y+7, clr);
			c &= 0x7F;
		}
		for(cy=0; cy<8; cy++) {
			for(cx=0; cx<7; cx++) {
				if( HP85font[c][cx] & (0x80>>cy) ) {
					PointBMB(hBM, x+cx, y+cy, clr);
				}
			}
		}
		x += 7;
	}
}

//***************************************************************************
void PrintGraphics(PBMB hBM, int x, int y, BYTE *buf, DWORD len, DWORD clr) {

	int		cy;
	BYTE	c;

	while( len-- ) {
		c = *buf++;
		for(cy=0; cy<8; cy++) {
			if( c & 0x80 ) PointBMB(hBM, x, y+cy, clr);
			c <<= 1;
		}
		++x;
	}
}

//**************************************************************
void PaperAdv(long advH, BOOL draw) {

	if( Model<2 ) {
		if( PRTlen+advH < prtBM->physH ) PRTlen += advH;
		else {
			long	y;

			y = prtBM->physH-advH;
			BltBMB(prtBM, 0, 0, prtBM, 0, advH, prtBM->physW, y, FALSE);
			RectBMB(prtBM, 0, y, prtBM->physW, y+advH, CLR_WHITE, CLR_NADA);
		}
		if( draw ) DrawPrinter();
	}
}

//********************************************************
void DrawPrinter() {

	long	sy;//, sh;

	if( Model<2 ) {
	//	sh = min(PRTh, PRTlen);
		sy = max(0, PRTlen - PRTh - PRTscroll);
		if( CfgFlags & CFG_BIGCRT ) StretchBMB(KWnds[0].pBM, PRTx, PRTy-2*(PRTlen-sy-PRTscroll), 2*prtBM->physW, 2*(PRTlen-sy-PRTscroll), prtBM, 0, sy, prtBM->physW, PRTlen-sy-PRTscroll);
		else BltBMB(KWnds[0].pBM, PRTx, PRTy-(PRTlen-sy-PRTscroll), prtBM, 0, sy, prtBM->physW, PRTlen-sy-PRTscroll, FALSE);
	}
}

//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================
//
//					CRT DISPLAY
//
//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================

void InitCRT() {

	if( Model<2 ) {
		hp85BM = CreateBMB(256, 2*192, FALSE);

		CRT = (BYTE*)malloc(8192);
		IO_CRTSTS = 0x7C;	// all unused bits = 1
		IO_CRTCTL = 0x06;	// wiped-out and powered-down until initialized
		IO_CRTSADbyte = 0;
		IO_CRTBADbyte = 0;
		// init ALPHA CRT memory to 13's (to avoid side-ways triangles)
		memset(CRT, 13, 4*16*32);
		// init GRAPHICS CRT memory to 0's
		memset(CRT+4*16*32, 0, 256*192/8);
		// init CRT bitmap
		HP85InvalidateCRT();
	} else {
		hp85BM = CreateBMB(640, 2*240, FALSE);
		CRT = (BYTE*)malloc(16384);
		IO_CRTSTS = 0x7C;	// all unused bits = 1
		IO_CRTCTL = 0x06;	// wiped-out and powered-down until initialized
		IO_CRTSADbyte = 0;
		IO_CRTBADbyte = 0;
		// init ALPHA CRT memory to 13's (to avoid side-ways triangles)
		memset(CRT, 13, 010340);
		// init GRAPHICS CRT memory to 0's
		memset(CRT+010340, 0, 040000-010340);
		// init CRT bitmap
		HP85InvalidateCRT();
	}
}

BOOL IsPageSize24() {

	return (IO_CRTCTL & 8 );
}

//***********************************
void FixCRTbytes(WORD addr) {

	if( IO_CRTBADbyte && addr!=0177405 && addr!=0177701 ) IO_CRTBADbyte = 0;	// for safety, to keep us from getting out of sync
	if( IO_CRTSADbyte && addr!=0177404 && addr!=0177700 ) IO_CRTSADbyte = 0; // in case some idiot does a STBD instead of a STMD
}

//************************
// This is used when the ENTIRE HP-85 CRT needs to be refreshed
void HP85InvalidateCRT() {

	WORD	cp, row, col, px;
	char	cb[81], c;

	if( Model<2 ) {
		WORD	f;

		RectBMB(hp85BM, 0, 0, 256, 2*192, CLR_BLACK, CLR_NADA);

		if( !(IO_CRTCTL & 6) ) { // if not wiped out or powered down
			cp = IO_CRTSAD/2;
			f = IO_CRTSAD & 1;
			if( IO_CRTCTL & 128 ) {	// graphics
				px = 0;
				for(row=0; row<192; row++) {
					for(col=0; col<256; col++) {
						if( f ) ;
						else if( CRT[cp] & (0x01 << (7-px)) ) PointBMB(hp85BM, col, 192+row, CRTbrightness);
						if( ++px >= 8 ) {
							px = 0;
							if( ++cp >= 8192 ) cp -= 8192;
						}
					}
				}
			} else {	// alpha
				for(row=0; row<16; row++) {
					for(col=0; col<32; col++) {
						if( f ) c = ((CRT[cp] & 0x0F)<<4) | ((CRT[cp+1]>>4) & 0xF0);
						else c = CRT[cp];
						cb[col] = c;
						if( ++cp >= 2048 ) cp -= 2048;
					}
					cb[col] = 0;
					Label85BMB(hp85BM, 0, 12*row, 8, (BYTE *)cb, 32, CRTbrightness);
				}
			}
			if( CfgFlags & CFG_BIGCRT ) StretchBMB(KWnds[0].pBM, CRTx, CRTy, 512, 384, hp85BM, 0, (IO_CRTCTL & 128) ? 192 : 0, 256, 192);
			else BltBMB(KWnds[0].pBM, CRTx, CRTy, hp85BM, 0, (IO_CRTCTL & 128) ? 192 : 0, 256, 192, FALSE);
		} else {
			if( CfgFlags & CFG_BIGCRT ) RectBMB(KWnds[0].pBM, CRTx, CRTy, CRTx+2*256, CRTy+2*192, CLR_BLACK, CLR_NADA);
			else RectBMB(KWnds[0].pBM, CRTx, CRTy, CRTx+256, CRTy+192, CLR_BLACK, CLR_NADA);

		}
		FlushScreen();
	} else {
		int		dotsPERrow, rowCURbyte, startCOL;
		BYTE	mask;
		DWORD	fclr, bclr;

		if( IO_CRTCTL & 0x20 ) {	// INVERT
			bclr = CRTbrightness;
			fclr = CLR_BLACK;
		} else {					// NOT invert
			fclr = CRTbrightness;
			bclr = CLR_BLACK;
		}
		RectBMB(hp85BM, 0, 0, 640, 2*240, bclr, CLR_NADA);

		if( !(IO_CRTCTL & 6) ) { // if not wiped out or powered down
			if( IO_CRTCTL & 128 ) {	// graphics
				rowCURbyte = 010340;
				if( IO_CRTCTL & 0x40 ) {
					dotsPERrow = 544;	// graph ALL
					startCOL = 48;
				} else {
					dotsPERrow = 400;	// graph NORMAL
					startCOL = 120;
				}
				for(row=0; row<240; row++) {
					mask = 0x80;
					for(col=0; col<dotsPERrow; col++) {
						if( CRT[rowCURbyte] & mask ) PointBMB(hp85BM, startCOL+col, 240+row, fclr);
						mask >>= 1;
						if( !mask ) {
							mask = 0x80;
							if( ++rowCURbyte>=16384 ) rowCURbyte = 0;
						}
					}
				}
			} else {	// alpha
				int		numROWS, rowH, wrapADR;

				cp = IO_CRTSAD;
				if( IO_CRTCTL & 0x08 ) {	// PAGESIZE 24
					numROWS = 24;
					rowH = 10;
				} else {					// PAGESIZE 16
					numROWS = 16;
					rowH = 15;
				}
				if( IO_CRTCTL & 0x40 ) wrapADR = 037700;
				else wrapADR = 010340;

				for(row=0; row<numROWS; row++) {
					for(col=0; col<80; col++) {
						cb[col] = CRT[cp];
						if( ++cp >= wrapADR ) cp = 0;
					}
					cb[col] = 0;
					Label85BMB(hp85BM, 0, rowH*row+2, rowH, (BYTE *)cb, 80, fclr);
				}
			}
			if( CfgFlags & CFG_BIGCRT ) StretchBMB(KWnds[0].pBM, CRTx, CRTy, 1280, 480, hp85BM, 0, (IO_CRTCTL & 0x80) ? 240 : 0, 640, 240);
			else BltBMB(KWnds[0].pBM, CRTx, CRTy, hp85BM, 0, (IO_CRTCTL & 0x80) ? 240 : 0, 640, 240, FALSE);
		} else {
			if( CfgFlags & CFG_BIGCRT ) RectBMB(KWnds[0].pBM, CRTx, CRTy, CRTx+2*640, CRTy+2*240, CLR_BLACK, CLR_NADA);
			else RectBMB(KWnds[0].pBM, CRTx, CRTy, CRTx+640, CRTy+240, CLR_BLACK, CLR_NADA);

		}
		FlushScreen();
	}
};

//************************************************
// This is used when only one BYTE of the HP-85 CRT needs to be redrawn
// (to speed things up.  Redrawing the whole CRT everytime, while easier
// to code, is just too dang slow for the real world... :-)
void HP85InvalidateCRTbyte() {

	WORD	f;
	char	cb[2], c;
	int		cp, row, col, i;
	DWORD	clr;

	f = IO_CRTBAD & 1;

	if( IO_CRTCTL & 128 ) {	// graphics
		cp = (IO_CRTBAD-IO_CRTSAD);
		while( cp < 0 ) cp += 16384;
		row = cp/64;
		col = cp%64;

		if( f ) c = ((CRT[IO_CRTBAD/2] & 0x0F)<<4) | ((CRT[(IO_CRTBAD+1)/2]>>4) & 0x0F);
		else c = CRT[IO_CRTBAD/2];

		cp = (col==63)?4:8;	// only do 4 pixels if we're starting in the last nibble on the right
		for(i=0; i<cp; i++) {
			clr = (c & (0x0080 >> i)) ? CRTbrightness : CLR_BLACK;
			PointBMB(hp85BM, 4*col+i, 192+row, clr);
			if( !(IO_CRTCTL & 6) ) {	// if not wiped out or powered down
				if( CfgFlags & CFG_BIGCRT ) {
					PointBMB(KWnds[0].pBM, CRTx+8*col+2*i, CRTy+2*row, clr);
					PointBMB(KWnds[0].pBM, CRTx+8*col+2*i+1, CRTy+2*row, clr);
					PointBMB(KWnds[0].pBM, CRTx+8*col+2*i, CRTy+2*row+1, clr);
					PointBMB(KWnds[0].pBM, CRTx+8*col+2*i+1, CRTy+2*row+1, clr);
				} else PointBMB(KWnds[0].pBM, CRTx+4*col+i, CRTy+row, clr);
			}
		}
		if( CfgFlags & CFG_BIGCRT ) KInvalidate(KWnds[0].pBM, CRTx +8*col, CRTy+2*row, CRTx+8*col+16, CRTy+2*row+2);
		else KInvalidate(KWnds[0].pBM, CRTx+4*col, CRTy+row, CRTx+4*col+8, CRTy+row+1);
	} else {	// alpha
		cp = (IO_CRTBAD-IO_CRTSAD)/2;
		if( cp <0 ) cp += 2048;
		if( cp >= 512 ) return;
		row = cp/32;
		col = cp%32;

		RectBMB(hp85BM, 8*col, 12*row, 8*col+8, 12*row+12, CLR_BLACK, CLR_NADA);

		// handle even/odd CRTBAD addressing
		if( f ) cb[0] = ((CRT[IO_CRTBAD/2] & 0x0F)<<4) | ((CRT[IO_CRTBAD/2+1]>>4) & 0x0F);
		else cb[0] = CRT[IO_CRTBAD/2];

		cb[1] = 0;
		Label85BMB(hp85BM, 8*col, 12*row, 8, (BYTE *)cb, 1, CRTbrightness);

		if( CfgFlags & CFG_BIGCRT ) StretchBMB(KWnds[0].pBM, CRTx+16*col, CRTy+24*row, 16, 24, hp85BM, 8*col, 12*row, 8, 12);
		else BltBMB(KWnds[0].pBM, CRTx+8*col, CRTy+12*row, hp85BM, 8*col, 12*row, 8, 12, FALSE);
	}
	FlushScreen();
};

//************************************************
// This is used when only one BYTE of the HP-85 CRT needs to be redrawn
// (to speed things up.  Redrawing the whole CRT everytime, while easier
// to code, is just too dang slow for the real world... :-)
void HP87InvalidateCRTbyte() {

	char	cb[2], c;
	int		cp, row, col, i, startCOL, rowH, pageH;
	DWORD	clr, fclr, bclr;

	if( IO_CRTCTL & 0x20 ) {	// INVERT
		bclr = CRTbrightness;
		fclr = CLR_BLACK;
	} else {					// NOT invert
		fclr = CRTbrightness;
		bclr = CLR_BLACK;
	}
	if( IO_CRTCTL & 128 ) {	// graphics
		if( IO_CRTCTL & 0x40 ) {	// graph ALL
			cp = IO_CRTBAD;
			if( cp < 010340 /*IO_CRTSAD*/ ) cp += 16384;
			cp -= 010340;//IO_CRTSAD;
			row = cp/68;
			col = cp%68;
			startCOL = 48;
		} else {			// graph NORMAL
			cp = (IO_CRTBAD - 010340);//IO_CRTSAD);
			row = cp/50;
			col = cp%50;
			startCOL = 120;
		}
		c = CRT[IO_CRTBAD];
		for(i=0; i<8; i++) {
			clr = (c & (0x0080 >> i)) ? fclr : bclr;
			PointBMB(hp85BM, startCOL+8*col+i, 240+row, clr);
			if( !(IO_CRTCTL & 6) ) {	// if not wiped out or powered down
				if( CfgFlags & CFG_BIGCRT ) {
					PointBMB(KWnds[0].pBM, CRTx+2*startCOL+16*col+2*i, CRTy+2*row, clr);
					PointBMB(KWnds[0].pBM, CRTx+2*startCOL+16*col+2*i+1, CRTy+2*row, clr);
					PointBMB(KWnds[0].pBM, CRTx+2*startCOL+16*col+2*i, CRTy+2*row+1, clr);
					PointBMB(KWnds[0].pBM, CRTx+2*startCOL+16*col+2*i+1, CRTy+2*row+1, clr);
				} else PointBMB(KWnds[0].pBM, CRTx+startCOL+8*col+i, CRTy+row, clr);
			}
		}
		if( CfgFlags & CFG_BIGCRT ) KInvalidate(KWnds[0].pBM, CRTx+2*startCOL+16*col, CRTy+2*row, CRTx+2*startCOL+16*col+16, CRTy+2*row+2);
		else KInvalidate(KWnds[0].pBM, CRTx+startCOL+8*col, CRTy+row, CRTx+startCOL+8*col+8, CRTy+row+1);
	} else {	// alpha
		cp = IO_CRTBAD;
		if( IO_CRTCTL & 0x40 ) {	// alpha ALL
			if( cp < IO_CRTSAD ) cp += 037700;
			cp -= IO_CRTSAD;
		} else {			// alpha NORMAL
			if( cp < IO_CRTSAD ) cp += 010340;
			cp -= IO_CRTSAD;
		}
		row = cp/80;
		col = cp%80;
		if( IO_CRTCTL & 0x08 ) {	// 24 lines
			pageH = 24;
			rowH = 10;
		} else {	// 16 lines
			pageH = 16;
			rowH = 15;
		}
		if( row<0 || row>=pageH ) return;

		RectBMB(hp85BM, 8*col, rowH*row, 8*col+8, rowH*(row+1), bclr, CLR_NADA);

		cb[0] = CRT[IO_CRTBAD];
		cb[1] = 0;
		Label85BMB(hp85BM, 8*col, rowH*row+2, rowH, (BYTE *)cb, 1, fclr);

		if( CfgFlags & CFG_BIGCRT ) StretchBMB(KWnds[0].pBM, CRTx+16*col, CRTy+2*rowH*row, 16, 2*rowH, hp85BM, 8*col, rowH*row, 8, rowH);
		else BltBMB(KWnds[0].pBM, CRTx+8*col, CRTy+rowH*row, hp85BM, 8*col, rowH*row, 8, rowH, FALSE);
	}
	FlushScreen();
};

//********************************************************
void DrawCRT(PBMB hBM, long x, long y) {

	if( Model<2 ) {
		if( CfgFlags & CFG_BIGCRT ) StretchBMB(hBM, x, y, 2*256, 2*192, hp85BM, 0, (IO_CRTCTL & 128) ? 192 : 0, 256, 192);
		else BltBMB(hBM, x, y, hp85BM, 0, (IO_CRTCTL & 128) ? 192 : 0, 256, 192, FALSE);
	} else {
		if( CfgFlags & CFG_BIGCRT ) StretchBMB(hBM, x, y, 2*640, 2*240, hp85BM, 0, (IO_CRTCTL & 128) ? 240 : 0, 640, 240);
		else BltBMB(hBM, x, y, hp85BM, 0, (IO_CRTCTL & 128) ? 240 : 0, 640, 240, FALSE);
	}
}

//********************************************************
void DrawBrightness(PBMB hBM) {

	long	y1, y2, x, i, h;

	RectBMB(hBM, BRTx, BRTy, BRTx+BRTw, BRTy+BRTh, CLR_DLGBACK, CLR_NADA);
	DrawHiLoBox(hBM, BRTx, BRTy, BRTw, BRTw, CLR_DLGHI, CLR_DLGLO);
	DrawHiLoBox(hBM, BRTx, BRTy+BRTw, BRTw, BRTh-2*BRTw, CLR_DLGHI, CLR_DLGLO);
	DrawHiLoBox(hBM, BRTx, BRTy+BRTh-BRTw, BRTw, BRTw, CLR_DLGHI, CLR_DLGLO);

	Label85BMB(hBM, BRTx+(BRTw-5)/2, BRTy+(BRTw-7)/2, 12, (BYTE*)"-", 1, CLR_DLGTEXT);
	Label85BMB(hBM, BRTx+(BRTw-7)/2, BRTy+BRTh-BRTw+(BRTw-5)/2, 12, (BYTE*)"+", 1, CLR_DLGTEXT);

	h  = 10*((BRTh-8*BRTw/3)/10);
	y1 = BRTy+BRTw+(BRTh-2*BRTw-h)/2;
	y2 = y1+h;
	x = BRTx+BRTw/2;
	LineBMB(hBM, x, y1, x, y2, CLR_DLGTEXT);
	i = ((CRTbrightness & 0x00FF) - 5)/25;
	y1 += i*(h/10);
	h = BRTw/5;
	LineBMB(hBM, x-h, y1, x+h+1, y1, CLR_DLGTEXT);

	Label85BMB(hBM, BRTx+(BRTw-24)/2, BRTy-12, 12, (BYTE*)"CRT", 3, MainWndTextClr);
}

//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================
//
//			I/O READ/WRITE SERVICE ROUTINES
//
//========================================================
//********************************************************
//********************************************************
//********************************************************
//========================================================

//*****************************************************************
// 'NULL' I/O read/write function for non-implemented I/O addresses
// Both reading and writing return a value.
// If 'val' is from 0-255, then it's a WRITE and 'val' is returned.
// If 'val' is outside the 0-255 range, then it's a READ and the read value is returned (0-255)
BYTE ioNULL(WORD address, long val) {

	return 0xFF;	// doesn't exist, just return high bus
}

//*****************************************************************
// 177400 GINTEN
BYTE ioGINTEN(WORD address, long val) {

	if( val>=0 && val<256 ) {
//		if( !(IO_KEYSTS & 0x80) ) IOcardsIntEnabled = SaveIOcIP;
		IO_KEYSTS |= 0x80;	// WRITE
	}
/* This code and comments are here in case assistance is needed during an emergency.  Say calm.  Don't panic.
	else if( (Model<2 && GETREGW(4)==023256) || (Model==2 && GETREGW(4)==051411 && ROM[060000]==0320) ) {
		// HP-85 STOREBIN can do one extra byte fetch from memory while writing
		// the binary program to tape (at ROM address 023256).  This is
		// harmless, and we ignore it.

		// HP-87 STORE can do one extra byte fetch from memory while writing
		// the binary program to tape (at ROM address 023256).  This is
		// harmless, and we ignore it.
	} else {
		sprintf((char*)ScratBuf, "Read from GINTEN (IO 177400) @ %o in ROM# %o", *((WORD*)(Reg+4)), ROM[060000]);
		MessageBox(NULL, (char*)ScratBuf, "Can't DO that!", MB_OK);
		HaltHP85();
	}
*/
	return 0xFF;	// READ
}

//*****************************************************************
// 177401 GINTDS
BYTE ioGINTDS(WORD address, long val) {

	if( val>=0 && val<256 ) {
//		if( IO_KEYSTS & 0x80 ) SaveIOcIP = IOcardsIntEnabled;
//		IOcardsIntEnabled = FALSE;
		IO_KEYSTS &= ~0x80;	// WRITE
	}
/*
	else {	// included in case of debugging emergency
			sprintf((char*)ScratBuf, "Read from GINTDS (IO 177401) @ %o in ROM# %o", *((WORD*)(Reg+4)), ROM[060000]);
			MessageBox(NULL, (char*)ScratBuf, "Can't DO that!", MB_OK);
			HaltHP85();
	}
*/
	return 0xFF;	// READ
}

//*****************************************************************
// 177402 KEYSTS
BYTE ioKEYSTS(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( val & 1 ) IO_KEYSTS |= 1;	// enable keyboard
		if( val & 2 ) IO_KEYSTS &= ~1;	// disable keyboard

		if( !(IO_KEYCTL & 0x40) && (val & 0x40) ) WaveStdBeep();
		else if( (IO_KEYCTL & 0x40) && !(val & 0x40) ) WaveStopBeep();

		if( !(IO_KEYCTL & 0x20) && (val & 0x20) ) WaveBeepOn();
		else if( (IO_KEYCTL & 0x20) && !(val & 0x20) ) WaveBeepOff();

		IO_KEYCTL = val;	// save SpeakerOn, 1.2KHZ bits
	}
	return IO_KEYSTS | IO_KEYHOMSTS;	// KEYSTS + fake-a-roo for PC HOME key to HP-87 HOME key (shift-up-cursor, which doesn't have it's own keycode, but just uses SHIFT to differentiate)
}

//*****************************************************************
// 177403 KEYCOD
BYTE ioKEYCOD(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( val==1 ) {
			IO_KeyEnabled = TRUE;
			if( IO_KEYDOWN ) {
				IO_KEYSTS &= ~2;
				IO_KEYDOWN = FALSE;
			}
		}
	}
	return IO_KEYCOD;	// READ
}

//*****************************************************************
// 177404 CRTSAD85
BYTE ioCRTSAD85(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( IO_CRTSADbyte ) {
			IO_CRTSAD = (IO_CRTSAD & 0x00FF) | (((WORD)val)<<8);
			HP85InvalidateCRT();
		} else IO_CRTSAD = (IO_CRTSAD & 0xFF00) | (WORD)val;
		IO_CRTSADbyte = !IO_CRTSADbyte;
	}
	return 0xFF;	// READ
}

//*****************************************************************
// 177405 CRTBAD85
BYTE ioCRTBAD85(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( IO_CRTBADbyte ) {
			IO_CRTBAD = (IO_CRTBAD & 0x00FF) | (((WORD)val)<<8);
			if( IO_CRTCTL & 128 ) {	// graphics
				IO_CRTBAD = IO_CRTBAD & 0x3FFF;	// wrap CRTBAD within screen
			} else { // alpha
				IO_CRTBAD &= 0x0FFF;	// wrap CRTBAD within ALPHA screen
			}
		} else {
			IO_CRTBAD = (IO_CRTBAD & 0xFF00) | (WORD)val;
		}
		IO_CRTBADbyte = !IO_CRTBADbyte;
	}
	return 0xFF;	// READ
}

//*****************************************************************
// 177406 CRTSTS85
BYTE ioCRTSTS85(WORD address, long val) {

	BYTE	retval=0, i;
	DWORD	c;

	if( val>=0 && val<256 ) {	// WRITE
		if( val & 1 ) IO_CRTSTS |= 1;	// set DATA READY for read request
		if( val & 2 ) ;	// do wipeout
		else ;	// do unwipe
		if( val & 4 ) ; // do power-down
		else ; // do power-up
		if( val & 128 ) ;	// set graphics mode
		else ;	// set alpha mode

		i = (IO_CRTCTL ^ val) & 0x86;
		IO_CRTCTL = val;
		if( i ) HP85InvalidateCRT();	// if changed wipe/pwup/alpha-graphics, redraw WHOLE screen
	} else {
		retval = IO_CRTSTS;
		if( CfgFlags & CFG_NATURAL_SPEED ) {	// if running at HP-85 "natural" speed
			IO_CRTSTS &= ~0x02;	// set to RETRACE time
			c = Cycles%CYCLES60;	// CPU cycles in 1/60th of a second
			if( c < 78*CYCLES60/100 )	// display time is approximately 78% of CRT time
				IO_CRTSTS |= 0x02;	// set to DISPLAY time
		} else IO_CRTSTS ^= 0x02;	// toggle "retrace time" and "display time" every other access
	}
	return retval;	// READ
}

//*****************************************************************
// 177407 CRTDAT85
BYTE ioCRTDAT85(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		if( IO_CRTBAD & 1 ) {	// if ODD CRTBAD, get nibbles from two different BYTES
			CRT[IO_CRTBAD/2] = (CRT[IO_CRTBAD/2] & 0xF0) | (val>>4);
			CRT[IO_CRTBAD/2+1] = (CRT[IO_CRTBAD/2+1] & 0x0F) | (val<<4);
		} else {
			CRT[IO_CRTBAD/2] = val;
		}
		HP85InvalidateCRTbyte();
		IO_CRTBAD += 2;
		if( IO_CRTCTL & 128 ) {	// graphics
			IO_CRTBAD &= 0x3FFF;	// wrap CRTBAD within screen
		} else { // alpha
			IO_CRTBAD &= 0x0FFF;	// wrap CRTBAD within ALPHA screen
		}
	} else {
		if( IO_CRTBAD & 1 ) {
			retval = (((CRT[IO_CRTBAD/2] & 0x0F)<<4) | ((CRT[(IO_CRTBAD+1)/2]>>4) & 0x0F));
		} else {
			retval = CRT[IO_CRTBAD/2];
		}
		++IO_CRTBAD;
		++IO_CRTBAD;
		if( IO_CRTCTL & 128 ) {	// graphics
			IO_CRTBAD = IO_CRTBAD & 0x3FFF;	// wrap CRTBAD within screen
		} else { // alpha
			IO_CRTBAD &= 0x0FFF;	// wrap CRTBAD within ALPHA screen
		}
	}
	return retval;	// READ
}

//**************************************************************
// get current byte of data from tape.
// if HOLE, set HOLE flag in TAPSTS
// if EVEN address, set TACH bit (tach every other location)
// if PWR-UP and MOTOR ON:
//		if not GAP, clear GAP flag, else set GAP flag
//		if tape data == 0, uninitialized area of tape, clear READY, GAP, HOLE, ILIM, and STALL

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

//*****************************************************************
// 177410 TAPSTS
BYTE ioTAPSTS(WORD address, long val) {

	BYTE	i, w, retval=0;
	long	dir;

	if( val>=0 && val<256 ) {	// WRITE
		if( Model<2 ) {
			if( (IO_TAPCTL & 0340)==0 ) {	// not writing previously
				if( (val & 0340)==0040 ) {	// starting a DATA write (no sync or gap)
					// if JUST starting write of DATA, make sure we're pointing at the
					// next block of data, and not a GAP.
					do {
						i = TAPBUF[IO_TAPCTL & 1][TAPPOS];
						if( i & (TAP_DATA | TAP_HOLE) ) break;
						++TAPPOS;	// skip ahead to first DATA or HOLE byte
					} while( TRUE );
				}
			}
			if( (val & 0340)==0100 ) {	// writing a SYNC
				if( !(TAPBUF[IO_TAPCTL & 1][TAPPOS] & TAP_HOLE) ) {
					TAPBUF[IO_TAPCTL & 1][TAPPOS++] = IO_TAPDAT | TAP_SYNC;	// write the SYNC word and advance tape
				}
				val &= ~0100;	// clear SYNC command
				val |= 0040;	// set writing DATA command (always after SYNC)
			}
			if( (IO_TAPCTL ^ val) & 036 ) {	// if changes in tape pwo/motor/speed
				IO_TAPCTL = val;
				DrawTape(KWnds[0].pBM, TAPEx, TAPEy);
				FlushScreen();
				IO_TAPSTS |= 0200;	// set READY, always READY after power-up/down
			} else IO_TAPCTL = val;
		}
	} else {
		if( Model<2 ) {
			IO_TAPSTS = (IO_TAPSTS & 0251) | 0040;	// clear tach,hole,ilim,stall, set gap, leave alone write-enabled and tape-in-drive
			if( (IO_TAPSTS | IO_TAPCART) & 1 ) {	// if tape in drive
				dir = (IO_TAPCTL & 0010) ? 1 : -1;	// tape direction
				if( TAP_ADVANCE && (IO_TAPCTL & 0006)==0006 ) {
					TAPPOS += dir;	// motor's running, they're reading TAPSTS, but not TAPDAT, so keep tape advancing
					TAP_ADVANCE = FALSE;
				}
				SetTapeStatus();
				if( (IO_TAPCTL & 0006)==0006 ) {		// if MOTOR-ON and PWR-UP
					if( (IO_TAPCTL & 0340)==0000 ) {	// if READING
						if( (TAPBUF[IO_TAPCTL & 1][TAPPOS] & TAP_SYNC)  ) {	// if SYNC byte, skip it
							TAPPOS += dir;
							SetTapeStatus();
						}
						w = TAPBUF[IO_TAPCTL & 1][TAPPOS];
						if( (IO_TAPCTL & 0026)==0026 && (w & TAP_DATA) ) {	// if searching over DATA
							for(i=0; i<8; i++) {
								w = TAPBUF[IO_TAPCTL & 1][TAPPOS];
								if( !(w & TAP_DATA) ) break;
								TAPPOS += dir;	// move faster when searching over DATA
							}
							SetTapeStatus();
							IO_TAPSTS |= 0100;	// definitely should have seen tach in there
						} else
						if( TAPBUF[IO_TAPCTL & 1][TAPPOS] & TAP_DATA ) {	// if on DATA
							IO_TAPSTS |= 0200;	// set READY
							TAP_ADVANCE = TRUE;	// flag we're on data/gap.  If they don't read TAPDAT, advance on the next read of TAPSTS.
						} else {
							TAPPOS += dir;	// if not DATA, advance ptr
							SetTapeStatus();
						}
					} else {	// else we're writing
						IO_TAPSTS |= 0200;	// always ready when writing
						w = IO_TAPCTL & 0340;
						if( w==0300 ) {			// writing GAP
							// if not on hole (NEVER overwrite hole), write GAP word
							if( !(TAPBUF[IO_TAPCTL & 1][TAPPOS] & TAP_HOLE) ) TAPBUF[IO_TAPCTL & 1][TAPPOS] = TAP_GAP;
							TAPPOS += dir;
							SetTapeStatus();
						} else if( w==0100 ) {	// writing SYNC
							// should never happen if we understand and have structured things right...
							SystemMessage("Writing SYNC during read of TAPSTS");
						} else if( w==0040 ) {	// writing DATA
							// do nothing, let write to TAPDAT do the write and advance tape
						}
					}
				} else IO_TAPSTS |= 0200;	// always READY if motor off or power down
			} else if( !IO_TAPCART ) IO_TAPSTS |= 0200;	// always READY if no tape in drive
			retval = IO_TAPSTS;
			IO_TAPSTS &= ~0200;	// clear ready
			IO_TAPSTS |= IO_TAPCART;	// set cartridge in if was first check
			DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
		}
	}
	return retval;	// READ
}

//*****************************************************************
// 177411 TAPDAT
BYTE ioTAPDAT(WORD address, long val) {

	BYTE	i, w, retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		if( Model<2 ) {
			i = IO_TAPDAT = val;

			w = IO_TAPCTL & 0340;
			if( w==0300 ) {				// writing GAP
				// probably just setting up for SYNC write, do nothing
			} else if( w==0100 ) {		// writing SYNC
				// should never happen if we understand and have structured things right...
				SystemMessage("Writing to TAPDAT during writing of SYNC");
			} else if( w==0040 ) {		// writing data
				i |= TAP_DATA;
			}
			// if motor on and writing and not on a hole (never overwrite THOSE!), write the byte to tape
			if( (IO_TAPCTL & 0006)==0006 ) {	// if the motor is on
				if( w && !(TAPBUF[IO_TAPCTL & 1][TAPPOS] & TAP_HOLE) ) 	TAPBUF[IO_TAPCTL & 1][TAPPOS] = i;	// write new value
				++TAPPOS;	// advance tape
			}
			DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
		}
	} else {
		if( Model<2 ) {
			// skipping GAPS/SYNCS, get next data byte (since we're supposed to be ready)
			if( (IO_TAPCTL & 0006)==0006 ) {	// PWR-UP, MOTOR ON
				while( !(TAPBUF[IO_TAPCTL & 1][TAPPOS] & (TAP_DATA | TAP_HOLE)) ) TAPPOS += (IO_TAPCTL & 0010) ? 1 : -1;
				retval = IO_TAPDAT = (BYTE)(TAPBUF[IO_TAPCTL & 1][TAPPOS++] & 0x00FF);
				TAP_ADVANCE = FALSE;
			} else {	// MOTOR OFF, just echo what was written
				retval = IO_TAPDAT;
			}
			DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
		}
	}
	return retval;	// READ
}

//*****************************************************************
// 177412 CLKSTS
BYTE ioCLKSTS(WORD address, long val) {

	char	temp[64];
	WORD	i;
	DWORD	c;

	if( val>=0 && val<256 ) {	// WRITE
		i = (val>>6) & 0x03;
		if( val & 0x01 ) IO_CLKSTS &= ~(1<<i);	// disable that timer
		if( val & 0x02 ) IO_CLKSTS |= (1<<i);	// enable that timer
		if( val & 0x04 ) IO_CLKCTL &= ~(1<<i);	// clear running flag for that timer
		if( val & 0x08 ) IO_CLKCTL |= (1<<i);	// set running flag for that timer
		if( val & 0x10) IO_CLKcount[i] = 0;	// clear that timer
		if( val & 0x20) IO_CLKCTL &= ~(0x10<<i);	// clear interrupt flip-flop for that timer
		IO_CLKnum = i;
		IO_CLKDATindex = 0;
		// in case they're going to read, setup up IO_CLKDAT[] for the selected timer
		sprintf(temp, "%8.8u", (unsigned int)(KGetTicks() - IO_CLKbase[IO_CLKnum]));
		for(i=0; i<4; i++) IO_CLKDAT[IO_CLKnum][3-i] = ((temp[2*i]-'0')<<4) | (temp[2*i+1]-'0');
	} else {
		if( CfgFlags & CFG_NATURAL_SPEED ) {	// if running at HP-85 "natural" speed
			IO_CLKSTS |= 0x80;	// set to NOT busy
			c = Cycles%CYCLESCLK;		// NOT busy 528 cycles, busy 176 cycles
			if( c >= (CYCLESCLK-CYCLESCLKBUSY) )
				IO_CLKSTS &= ~0x80;	// set to not BUSY
		} else IO_CLKSTS ^= 0x80;	// toggle BUSY every other access
	}
	return IO_CLKSTS;	// READ
}

//*****************************************************************
// 177413 CLKDAT
BYTE ioCLKDAT(WORD address, long val) {

	BYTE	retval=0;
	WORD	i;
	DWORD	dw;

	if( val>=0 && val<256 ) {	// WRITE
		IO_CLKDAT[IO_CLKnum][IO_CLKDATindex] = val;
		if( ++IO_CLKDATindex >= 4 ) {	// we have all the bytes of the counter value they're writing
			IO_CLKbase[IO_CLKnum] = KGetTicks();	// set base system millisecond count for this timer
			IO_CLKDATindex = 0;
			dw = 0;
			for(i=0; i<4; i++) {
				dw = dw*10 + ((IO_CLKDAT[IO_CLKnum][3-i]>>4) & 0x0F);
				dw = dw*10 + (IO_CLKDAT[IO_CLKnum][3-i] & 0x0F);
			}
			IO_CLKcount[IO_CLKnum] = dw;
		}
	} else {
		retval = IO_CLKDAT[IO_CLKnum][IO_CLKDATindex];	// return next byte from requested clock counter
		if( ++IO_CLKDATindex >= 4 ) IO_CLKDATindex = 0;
	}
	return retval;	// READ
}

//*****************************************************************
// 177414 PRTLEN
BYTE ioPRTLEN(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( val>32 ) val = 32;		// shouldn't happen, but just in case, don't overrun buffer
		IO_PRTLEN = val;
		IO_PRTCNT = 0;
		if( IO_PRTLEN==0 ) {
			PRTscroll = 0;
			PaperAdv(PTH, TRUE);	// do paper advance
		} else IO_PRTDMP = TRUE;
	}
	return 0xFF;	// READ
}

//*****************************************************************
// 177415 PRCHAR
BYTE ioPRCHAR(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		IO_PRCHAR = val;
	} else {
		retval = HP85font[IO_PRCHAR & 0x7F][IO_PRTCTL & 7];
		if( IO_PRCHAR & 0x80 ) retval |= 0x01;	// set underline if needed
	}
	return retval;	// READ
}

//*****************************************************************
// 177416 PRTSTS
BYTE ioPRTSTS(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( Model<2 ) {
			IO_PRTCTL = val;
		} else {
			sprintf((char*)ScratBuf, "Unexpected access to PRTSTS (IO 177416) at %o in ROM# %o", *((WORD*)(Reg+4)), ROM[060000]);
			MessageBox(NULL, (char*)ScratBuf, "Oops!", MB_OK);
		}
	}
	return IO_PRTSTS;	// READ
}

//*****************************************************************
// 177417 PRTDAT
BYTE ioPRTDAT(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		if( Model<2 ) {
			if( IO_PRTCTL & 0x80 ) {	// graphics
				if( IO_PRTCNT < sizeof(IO_PRTBUF) ) IO_PRTBUF[IO_PRTCNT] = val;
				++IO_PRTCNT;
				if( IO_PRTDMP && (IO_PRTCNT >= sizeof(IO_PRTBUF) || IO_PRTCNT >= 192) ) {
					PRTscroll = 0;
					PrintGraphics(prtBM, 40, PRTlen, IO_PRTBUF, 192, PrintClr);
					PaperAdv(8, TRUE);	// advance one "line" BEFORE we print, so don't draw
					IO_PRTDMP = FALSE;
				}
			} else {					// alpha
				if( IO_PRTCNT < IO_PRTLEN ) IO_PRTBUF[IO_PRTCNT] = val;
				++IO_PRTCNT;
				if( IO_PRTDMP && (IO_PRTCNT >= sizeof(IO_PRTBUF) || IO_PRTCNT >= IO_PRTLEN) ) {
					PRTscroll = 0;
					IO_PRTBUF[IO_PRTLEN] = 0;
					Print85(prtBM, 24, PRTlen, IO_PRTBUF, IO_PRTLEN, PrintClr);
					PaperAdv(PTH, TRUE);	// advance one "line" BEFORE we print, so don't draw
					IO_PRTDMP = FALSE;
				}
			}
		} else {
			sprintf((char*)ScratBuf, "Unexpected access to PRTDAT (IO 177417) at %o in ROM# %o", *((WORD*)(Reg+4)), ROM[060000]);
			MessageBox(NULL, (char*)ScratBuf, "Oops!", MB_OK);
		}
	}
	return 0xFF;	// READ
}

//*****************************************************************
// 177700 CRTSAD87
BYTE ioCRTSAD87(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		if( IO_CRTSADbyte ) {
			IO_CRTSAD = ((IO_CRTSAD & 0x00FF) | (((WORD)val)<<8)) & 0x3FFF;	// limit to within CRT RAM
			HP85InvalidateCRT();
		} else IO_CRTSAD = (IO_CRTSAD & 0xFF00) | (WORD)val;
		IO_CRTSADbyte = !IO_CRTSADbyte;
	} else {
			retval = (IO_CRTSADbyte ? (IO_CRTSAD>>8) : IO_CRTSAD) & 0x00FF;
			IO_CRTSADbyte = !IO_CRTSADbyte;
	}
	return retval;	// READ
}

//*****************************************************************
// 177701 CRTBAD87
BYTE ioCRTBAD87(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		if( IO_CRTBADbyte ) {
			IO_CRTBAD = ((IO_CRTBAD & 0x00FF) | (((WORD)val)<<8)) & 0x3FFF;	// limit to within CRT RAM
		} else {
			IO_CRTBAD = (IO_CRTBAD & 0xFF00) | (WORD)val;
		}
		IO_CRTBADbyte = !IO_CRTBADbyte;
	} else {
			retval = (IO_CRTBADbyte ? (IO_CRTBAD>>8) : IO_CRTBAD) & 0x00FF;
			IO_CRTBADbyte = !IO_CRTBADbyte;
	}
	return retval;	// READ
}

//*****************************************************************
// 177702 CRTSTS87
BYTE ioCRTSTS87(WORD address, long val) {

	BYTE	i;
	BYTE	retval=0;
	DWORD	c;

	if( val>=0 && val<256 ) {	// WRITE
		if( val & 1 ) IO_CRTSTS &= ~1;	// set NOT BUSY for read request
		if( val & 2 ) ;	// do wipeout
		else ;	// do unwipe
		if( val & 4 ) ; // do power-down
		else ; // do power-up
		if( val & 8 ) ;	// set 24 lines
		else ;	// set 16 lines
		if( val & 32 ) ; // set inverse display
		else ;	// set non-inverse
		if( val & 64 ) ;	// set ALL mode
		else ;	// set NORMAL mode
		if( val & 128 ) ;	// set graphics mode
		else ;	// set alpha mode

		i = (IO_CRTCTL ^ val) & 0xEE;
		IO_CRTCTL = val;
		if( i ) HP85InvalidateCRT();	// if changed wipe/pwup/alpha-graphics, redraw WHOLE screen
	} else {
		retval = ((IO_CRTSTS & 0x10) | (IO_CRTCTL & ~0x10)) & ~0x01;
		if( CfgFlags & CFG_NATURAL_SPEED ) {	// if running at HP-85 "natural" speed
			IO_CRTSTS &= ~0x10;	// set to RETRACE time
			c = Cycles%CYCLES60;	// CPU cycles in 1/60th of a second
			if( c < 78*CYCLES60/100 )	// display time is approximately 78% of CRT time
				IO_CRTSTS |= 0x10;	// set to DISPLAY time
		} else IO_CRTSTS ^= 0x10;	// toggle "retrace time" and "display time" every other access
	}
	return retval;	// READ
}

//*****************************************************************
// 177703 CRTDAT87
BYTE ioCRTDAT87(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		CRT[IO_CRTBAD] = val;
		HP87InvalidateCRTbyte();
		++IO_CRTBAD;
		if( IO_CRTCTL & 128 ) {	// graphics
			IO_CRTBAD = IO_CRTBAD & 0x3FFF;	// wrap CRTBAD within screen
		} else { // alpha
			if( ((IO_CRTCTL & 0x40) && IO_CRTBAD >= 037677)
			 || (!(IO_CRTCTL &0x40) && IO_CRTBAD >= 010337) ) IO_CRTBAD = 0;	// wrap CRTBAD within ALPHA screen
		}
	} else {
		retval = CRT[IO_CRTBAD];
		++IO_CRTBAD;
		if( IO_CRTCTL & 128 ) {	// graphics
			if( IO_CRTBAD > 16384 ) IO_CRTBAD = 0;
		} else { // alpha
			if( IO_CRTCTL & 0x40 ) {	// ALL
				if( IO_CRTBAD >= 037700 ) IO_CRTBAD = 0;	// wrap CRTBAD within ALPHA screen
			} else {	// NORMAL
				if( IO_CRTBAD >= 010340 ) IO_CRTBAD = 0;
			}
		}
	}
	return retval;	// READ
}

//*****************************************************************
// 177430 RSELEC
BYTE ioRSELEC(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		SelectROM(val);
	} else {
/*
			MessageBox(NULL, "Read from RSELEC (IO 177430)", "Can't DO that!", MB_OK);
			HaltHP85();
*/
	}
	return 0xFF;	// READ
}

//*****************************************************************
// 177710-177713 PTR1
BYTE ioPTR1(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		if( Ptr1Cnt<3 ) {
			Ptr1 = (Ptr1 & ~(0x000000FF<<(8*Ptr1Cnt))) | (((DWORD)val)<<(8*Ptr1Cnt));
			++Ptr1Cnt;
		}
	} else {
		if( Ptr1Cnt<3 ) {
			retval = (Ptr1 >> (8*Ptr1Cnt)) & 0x0000FF;
			++Ptr1Cnt;
		} else retval = 0xFF;
	}
	return retval;	// READ
}

//*****************************************************************
// 177714-717 PTR2
BYTE ioPTR2(WORD address, long val) {

	BYTE	retval=0;

	if( val>=0 && val<256 ) {	// WRITE
		if( Ptr2Cnt<3 ) {
			Ptr2 = (Ptr2 & ~(0x000000FF<<(8*Ptr2Cnt))) | (((DWORD)val)<<(8*Ptr2Cnt));
			++Ptr2Cnt;
		}
	} else {
		if( Ptr2Cnt<3 ) {
			retval = (Ptr2 >> (8*Ptr2Cnt)) & 0x0000FF;
			++Ptr2Cnt;
		} else retval = 0xFF;
	}
	return retval;	// READ
}

//*****************************************************************
// 177704 RULITE
BYTE ioRULITE(WORD address, long val) {

	if( val>=0 && val<256 ) {	// WRITE
		RULITEblink = val;	// RULITEblink 1=blink runlite, 0=noblink
		RULITEtime = KGetTicks();
		RULITEon = TRUE;
		DrawRunLite();
	}
	return RULITEblink;	// READ (doubt that it was readable on real hardware, but what the hell)
}

