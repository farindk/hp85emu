///**************************************************************
/// cardHPIB.c - implements emulation of an HPIB card, of course.
/// Much of this file is for interfacing to the DEVICES on the
/// emulated HPIB bus, which are in their separate "devXXXX" files.
///**************************************************************

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include	"main.h"
#include	"mach85.h"
#include	"dlg.h"
#include	"beep.h"

/*
	To simulate an interrupt, and avoid stepping all over "other cards",
	the interrupt must be 'paced' to happen when everything is safe.
	That means REQUESTING to interrupt, then waitingt until we're GRANTED the interrupt.
	The steps are:
			HPIB[sc]->IB[0]= 0xFF;		// Fill the IB[] with the reason for the interrupt
			HPIB[sc]->IB[1]= 0xFF;		// As many as necessary, frequently 2-4 are needed for multiple reads
			HPIB[sc]->IB[2]= 0xFF;
			HPIB[sc]->IB[3]= 0xFF;
			HPIB[sc]->IBptr = 0;		// point the IBptr to the start of the buffer
			HPIB[sc]->IBcnt = 4;		// set the count of them to keep from being flushed away
			IO_PendingInt |= (1<<sc);	// flag we want to interrupt
			HPIB[sc]->LastIOPcmd = 0xFF;	// no last cmd
			HPIB[sc]->PSR |= 1;			// set IBF in PSR
	The IO_PendingInt will periodically (pretty much between every instruction) be checked
	in mach85.c's Exec85() function.  Assuming interrupts aren't disabled or blocked for
	any reason, then a call will be made to the card's initProc() with iR_ENINT.  That
	function (at the end of this file, for an example) will then:
			IO_SC = 0120 | (sc<<1);		// set IO address based upon SC
			Int85 |= INT_IO;			// interrupt by IO
			IO_PendingInt &= ~(1<<sc);	// clear our request no matter what
	IO_SC is the value that will be 'returned' to the Series 80 CPU when it reads INTRSC to
	find out the IO address (lower byte) of the card that is interrupting.  'Int85' is the
	mach85.c variable that tells us which chips/IOPs are interrupting (clocks, keyboard, IOP).
	So, once those are set up, then we clear our IO_PendingInt (request) bit, and Exec85()
	and Int85 will take it from there.
*/

//**********************************************************
//	S.C. 7 I/O addresses are:
//		write to  177530 = CCR
//		read from 177530 = PSR
//		write to  177531 = OB
//		read from 177531 = IB
//
typedef struct {
	BYTE	CCR, PSR;		// Calculator Control Register and Processor Status Register
	BYTE	IB[512];		// Input Buffer
	long	IBcnt, IBptr;	// byte count in IB, index to next byte to send to CPU
	BYTE	LastIOPcmd;		// last command for IOP
	BYTE	LastHPIBcmd;	// last command for HPIB bus
	BYTE	OB;				// Output Buffer register
	BOOL	OBfull;			// if OB has a byte waiting that no one has read
	BYTE	STATE;			// what the IOP is in the process of doing (see IOSTATE_ #defines above)

	BYTE	CR[32];			// HP-IB Control Registers
	long	CRptr;			// index to next CR[] to write

	// CR[9] and CR[10] (as a WORD) contain the BURST COUNT for the HP-IB card
	long	BurstCnt;		// bytes remaining in the burst
	BYTE	BurstDir;		// 0=not in burst mode, 0x20=burst OUT, 0x21=burst IN


	BYTE	SR[32];			// HP-IB Status Registers
	long	SRptr;			// index to next SR to read

	// SR[0] always = 1 to identify the card as HP-IB

	BYTE	LA[32];			// Listen Address ACTIVE flags.  Series 80 computer is hardwired to LA 21 (LA[31] is invalid, meaning UNL)
	BYTE	TA[32];			// Talk Address ACTIVE flags.  Series 80 computer is hardwired to LA 21 (LA[31] is invalid, meaning UNT)

	// init this array with proper devFunc() addresses in initHPIB(iR_STARTUP) and initHPIB(iR_RESET)
	// function arguments are:
	//	sc = Select Code (0-7)
	//	tla = talk/listen address
	//	unit = whatever (for disk drives, it's the sub-unit drive# (0-1), otherwise whatever you need/want)
	//	arg = whatever ('val' for CCR/PSR and OB/IB calls)
	int (*devProc[31])(long reason, int sc, int tla, int unit, int arg);	// for TLAs 0-30

	void	*devData[31];	// malloc ptrs for data structure for each device

	BYTE	CA;				// secondary command addressed controller (0-7)

	BYTE	*OB_PTR;		// ptr to where to store next output byte
} tHPIB;

tHPIB	*HPIB[8];	// pointer to HPIB data structure for select codes 3-10 ('sc'=0-7)

BOOL	ResetKey;	// FALSE if power-on, TRUE when RESET key is pressed.  Used by ResetHPIB() to handle HP85A w/ PP ROM's problematic IO card logging.
BYTE	RESETok[] = {0x03, 0x03};

/*
typedef struct {
	int		id[2], units, heads, cylinders, cylsects, devType;
} DISKPARM;
*/
/// NOTE: DiskParms[] entries MUST be in the same order as DEVTYPE_'s so DEVTYPE_ can be used as an index
DISKPARM	DiskParms[]={
	{{1,0x04},2,2,33,32,DEVTYPE_5_25,NULL},
	{{1,0x04},2,2,33,32,DEVTYPE_3_5,NULL},
	{{0,0x81},2,2,75,60,DEVTYPE_8,NULL},
	{{1,0x06},1,4,153,124,DEVTYPE_5MB}
};

#if DEBUG
BYTE GetLastIOPcmd(int sc) {return HPIB[sc]->LastIOPcmd;}
#endif // DEBUG
///**************************************************************
///**************************************************************
///**************************************************************
///**************************************************************
///**************************************************************
///**************************************************************

typedef struct {
	int		devType;		// DEVTYPE_ value, to identify what kind of DEV structure this is (MUST be the first element of the structure! The same on all DEV structures)
} DEVSTRUCT;

///**************************************************************
void *CreateDevData(int sc, int tla, int cnt, int devType) {

	DEVSTRUCT	*pD = HPIB[sc]->devData[tla] = malloc(cnt);

	if( pD!=NULL ) {
		memset(pD, 0, cnt);
		pD->devType = devType;
	}
	return pD;
}

///**************************************************************
void *GetDevData(int sc, int tla, int devType) {

	DEVSTRUCT	*pD;

	if( HPIB[sc]==NULL ) return NULL;

	pD = HPIB[sc]->devData[tla];
	if( pD && pD->devType!=devType ) pD = NULL;

	return pD;
}

///**************************************************************
void DestroyDevData(int sc, int tla) {

	free(HPIB[sc]->devData[tla]);
	HPIB[sc]->devData[tla] = NULL;
}

///**************************************************************
BOOL InBurst(int sc) {

	return HPIB[sc]->BurstDir!=0;
}

///**************************************************************
void SetHPIBstate(int sc, BYTE state) {

	HPIB[sc]->STATE = state;
}

///**************************************************************
BYTE GetHPIBstate(int sc) {

	return HPIB[sc]->STATE;
}

///**************************************************************
BOOL IsCED(int sc) {

	return (HPIB[sc]->CCR & 4);
}

///**************************************************************
void SetPED(int sc) {

	HPIB[sc]->PSR = 4;
}

///**************************************************************
void SetBurst(int sc, int mode, int cnt) {

	HPIB[sc]->BurstDir = mode;
	HPIB[sc]->BurstCnt = cnt;
}

///**************************************************************
BOOL IsTalker(int sc, int tla) {

	return HPIB[sc]->TA[tla];
}

///**************************************************************
BOOL IsListener(int sc, int tla) {

	return HPIB[sc]->LA[tla];
}

///**************************************************************
BOOL TLAavailable(int sc, int tla) {

	return HPIB[sc]->devProc[tla]==NULL;
}

///**************************************************************
void TLAset(int sc, int tla, int (*pDevProc)(long reason, int sc, int tla, int unit, int arg)) {

	if( TLAavailable(sc, tla) ) HPIB[sc]->devProc[tla] = pDevProc;
}

///**************************************************************
void SetOBptr(int sc, BYTE *ptr) {

	HPIB[sc]->OB_PTR = ptr;
}

///**************************************************************
BYTE *GetOBptr(int sc) {

	return HPIB[sc]->OB_PTR;
}

///**************************************************************
void PushOB(int sc, BYTE b) {

	*HPIB[sc]->OB_PTR++ = b;
}

///**************************************************************
void StartBurstIn(int sc, BYTE *pB, int cnt) {

	HPIB[sc]->PSR  = 1;	// not busy, input buffer full
	memcpy(HPIB[sc]->IB, pB, 256);	// copy sector from disk buffer to input buffer
	HPIB[sc]->IBptr = 0;
	HPIB[sc]->IBcnt = cnt;
}

///**************************************************************
void HPIBint(int sc, BYTE *pB, int blen) {

	memcpy(&HPIB[sc]->IB[0], pB, blen);
	HPIB[sc]->IBptr = 0;
	HPIB[sc]->IBcnt = blen;
	IO_PendingInt |= (1<<sc);		// flag we want to interrupt
	HPIB[sc]->LastIOPcmd = 0xFF;		// no last cmd
	HPIB[sc]->PSR  = ((HPIB[sc]->PSR) & ~2) | 1;	// not busy, input buffer full
//	HPIB[sc]->PSR |= 1;
}

///**************************************************************
void HPIBinput(int sc, BYTE *pB, int blen) {

	memcpy(&HPIB[sc]->IB[0], pB, blen);
	HPIB[sc]->IBptr = 0;
	HPIB[sc]->IBcnt = blen;
	HPIB[sc]->PSR  = ((HPIB[sc]->PSR) & ~2) | 1;	// not busy, input buffer full
	if( blen<2 ) HPIB[sc]->PSR |= 4;	// set PED on last byte
//	HPIB[sc]->PSR |= 1;
}

///**************************************************************
void ResetHPIB(int	sc) {

	int		i, j;

	for(i=0; i<31; i++) {
		if( HPIB[sc]->devProc[i] != NULL ) {
			for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
			HPIB[sc]->devProc[i](DP_FLUSH, sc, i, 0, j);	// tell it to save any pending output
		}
	}

	// init HP-IB I/O SC
#if DEBUG
		sprintf((char*)ScratBuf,"ResetHPIB(%d) IOcIE=%d IOpI=%d", sc, IOcardsIntEnabled, (int)IO_PendingInt);
		WriteDebug(ScratBuf);
#endif // DEBUG
	HPIB[sc]->CCR  = 0x00;

	HPIBint(sc, RESETok, 2);

	HPIB[sc]->BurstDir = 0;		// not in BURST mode

	HPIB[sc]->STATE = IOSTATE_IDLE;
	memset(HPIB[sc]->LA, 0, 32);
	memset(HPIB[sc]->TA, 0, 32);

	memset(HPIB[sc]->CR, 0, 32);
	HPIB[sc]->CR[16] = 2;		// EOL length
	HPIB[sc]->CR[17] = 13;		// CR
	HPIB[sc]->CR[18] = 10;		// LF

	memset(HPIB[sc]->SR, 0, 32);
	HPIB[sc]->SR[0]  = 1;		// card = HP-IB
	HPIB[sc]->SR[1]  = 0;		// INTERRUPT CAUSE
	HPIB[sc]->SR[2]  = 0xFC;		// IFC, REN, SRQ, ATN, EOI, DAV, NDAC, NRFD (MS TO LS, LF-2-RT)
	HPIB[sc]->SR[3]  = 0;		// HP-IB DATA LINES
	HPIB[sc]->SR[4]  = 0x22;		// System Controller, A1
	HPIB[sc]->SR[5]  = 0xA0;		// System Controller, Controller Active
	HPIB[sc]->SR[6]  = 1;		// SC1 (secondary commands)

	HPIB[sc]->OB_PTR = NULL;		// flag awaiting command

	// set up device function pointers
	for(i=0; i<31; i++) {

//GEEKDO: NOTE! IF/WHEN devices are remappable in the OPTIONS dialog,
// THEN that remapping needs to be addressed here (or elsewhere!)
// Notice that the above RESET code does NOT disturb HPIB[sc]->devProc[] values!

		if( HPIB[sc]->devProc[i]!=NULL ) {
			for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
			HPIB[sc]->devProc[i](DP_RESET, sc, i, 0, j);
		}
	}

	/// ARGGGGGHHH! The following line HAS to be here to make "auto-disk detection"
	/// set the default MSUS at startup IF the PP ROM is PRESENT on the HP85A.
	/// ARGGGGGHHH! That works, but now it fails when you press the RESET button.
	i = PPromPresent();
	if( (Model==0 && !PPromPresent()) || (Model!=0) || ResetKey ) IOcardsIntEnabled = TRUE;
}

///**************************************************************
void DevOutput(int sc, BYTE val) {

	int		i;

	for(i=0; i<31; i++) if( HPIB[sc]->LA[i] && HPIB[sc]->devProc[i]!=NULL ) HPIB[sc]->devProc[i](DP_OUTPUT, sc, i, 0, val);
}

///*****************************************************************
void ExecIOPcmd(int sc, int val) {

	long	i, p;

	for(i=0; i<31; i++) if( (HPIB[sc]->LA[i] || HPIB[sc]->TA[i]) && HPIB[sc]->devProc[i]!=NULL ) HPIB[sc]->devProc[i](DP_NEWCMD, sc, i, 0, val);

	HPIB[sc]->LastIOPcmd = (val & 0xF0);
	// here follows implementation of device-independed commands
	switch( val & 0xF0 ) {
	 case 0x00:	// READ STATUS
		HPIB[sc]->SRptr = val & 0x0F;
		if( HPIB[sc]->SRptr>6 ) {	// invalid STATUS register
			HPIB[sc]->IB[0]= 0xFF;
			HPIB[sc]->IB[1]= 0xFF;
			HPIB[sc]->IB[2]= 0xFF;
			HPIB[sc]->IB[3]= 0xFF;
			HPIB[sc]->IBptr = 0;
			HPIB[sc]->IBcnt = 4;			// takes 4 of them to keep from being flushed away
			IO_PendingInt |= (1<<sc);		// flag we want to interrupt
			HPIB[sc]->LastIOPcmd = 0xFF;		// no last cmd
			HPIB[sc]->PSR |= 1;
#if DEBUG
		sprintf((char*)ScratBuf,"ReadStatus(%d)", sc);
		WriteDebug(ScratBuf);
#endif // DEBUG
		} else {
			HPIB[sc]->IBcnt = 32-HPIB[sc]->SRptr;
			memcpy(HPIB[sc]->IB, HPIB[sc]->SR+HPIB[sc]->SRptr, HPIB[sc]->IBcnt);
			HPIB[sc]->IBptr = 0;
			HPIB[sc]->PSR |= 1;	// set IBF
		}
		break;
	 case 0x10:	// INPUT
	 	break;
	 case 0x20:	// BURST I/O
	 	if( val==0x20 ) {
			HPIB[sc]->BurstCnt = 256;
			HPIB[sc]->BurstDir = val;
	 	}
		break;
	 case 0x30:	// INTERRUPT CONTROL
	 	// happens when MS ROM is interrupting/halting all IOP except one that's doing BURST IO
		IO_PendingInt &= ~(1<<sc);	// throw away our interrupt so we don't interfere with the BURST
#if DEBUG
sprintf((char*)ScratBuf+6000,"0x30 INTERRUPT CONTROL sc=%d val=0x%2.2X LastIOPcmd=0x%2.2X IOcIE=%d IOpI=%d", sc, val, HPIB[sc]->LastIOPcmd, IOcardsIntEnabled, (int)IO_PendingInt);
WriteDebug(ScratBuf+6000);
#endif // DEBUG
		break;
	 case 0x40:	// INTERFACE CONTROL
		switch( val ) {
		 case 0x44:		// Parallel Poll
			HPIB[sc]->IBcnt = 1;
			for(i=p=0; i<8; i++) {
				DEVSTRUCT	*pD;

				pD = HPIB[sc]->devData[i];
				if( pD && pD->devType<=DEV_MAX_DISK_TYPE && HPIB[sc]->devProc[i]!=NULL && HPIB[sc]->devProc[i](DP_PPOLL, sc, i, 0, 0) ) p |= (0x80>>i);
			}
			HPIB[sc]->IB[0] = (BYTE)p;
			HPIB[sc]->IBptr = 0;
			HPIB[sc]->PSR |= 5;	// set IBF & PED
			break;
		 case 0x45:		// send MTA
			HPIB[sc]->TA[TLA85] = TRUE;
			break;
		 case 0x46:		// send MLA
			HPIB[sc]->LA[TLA85] = TRUE;
			break;
		 case 0x47:		// send EOL sequence
			for(i=0; i<(HPIB[sc]->CR[16] & 7); i++) DevOutput(sc, HPIB[sc]->CR[17+i]);
			break;
		 case 0x49:		// RESUME I/O
			break;
		 default:
			sprintf((char*)ScratBuf, "Unknown INTERFACE CONTROL: 0x%2.2X", (unsigned int)val);
			MessageBox(NULL, (char*)ScratBuf, "Got some more work to do!", MB_OK | MB_ICONEXCLAMATION);
			break;
		}
		break;
	 case 0x70:	// READ AUXILIARY
		switch( val ) {
		 case 0x77:
			// assumes they're reading EOI/EOL register...
			HPIB[sc]->CRptr = 16;
			HPIB[sc]->IBcnt = 32-HPIB[sc]->CRptr;
			memcpy(HPIB[sc]->IB, HPIB[sc]->CR + HPIB[sc]->CRptr, HPIB[sc]->IBcnt);
			HPIB[sc]->IBptr = 0;
			HPIB[sc]->PSR |= 1;	// set IBF
			break;
		 default:
			sprintf((char*)ScratBuf, "Unknown READ AUXILIARY: %2.2X", (unsigned int)val);
			MessageBox(NULL, (char*)ScratBuf, "Got some more work to do!", MB_OK | MB_ICONEXCLAMATION);
			break;
		}
		break;
	 case 0x80:
	 case 0x90:	// WRITE CONTROL
		HPIB[sc]->CRptr = val & 0x1F;
		break;
	 case 0xA0:	// OUTPUT
		break;
	 case 0xB0:	// SEND / ATNCMD
		HPIB[sc]->STATE = IOSTATE_WAITFORCMD;
		break;
	 case 0xE0:	// WRITE AUXILIARY
		if( val!=0xE6 ) {	// setting up to READ EOI/EOL register through READ AUXILIARY
			sprintf((char*)ScratBuf, "Unknown WRITE AUXILIARY: %2.2X", (unsigned int)val);
			MessageBox(NULL, (char*)ScratBuf, "Got some more work to do!", MB_OK | MB_ICONEXCLAMATION);
			break;
		}
		break;
	 default:
		sprintf((char*)ScratBuf, "Unknown IOP command: %2.2X : %2.2X", HPIB[sc]->LastIOPcmd, (unsigned int)val);
		MessageBox(NULL, (char*)ScratBuf, "Got some more work to do!", MB_OK | MB_ICONEXCLAMATION);
		break;
	}
}

///*****************************************************************
// 177530 CCRPSR
BYTE ioHPIB_CCRPSR(WORD address, long val) {

	BYTE	retval=0;
	int		sc;

	sc = (address-0177520)/2;

	if( val>=0 && val<256 ) {	// WRITE (CCR)
#if DEBUG
	sprintf((char*)ScratBuf, "CCR %2.2lX IOcIE=%d IOpI=%d LastIOPcmd=%d PSR%d= start:%2.2X end:", val, IOcardsIntEnabled, (int)IO_PendingInt, HPIB[sc]->LastIOPcmd, sc+3, HPIB[sc]->PSR);
#endif
		if( /*(HPIB[sc]->CCR & 0x80) && !*/(val & 0x80) ) {
#if DEBUG
	WriteDebug(ScratBuf);
#endif
			ResetKey = TRUE;
			ResetHPIB(sc);
#if DEBUG
	sprintf((char*)ScratBuf, "Back from ResetHPIB(). IOcIE=%d IOpI=%d LastIOPcmd=%d PSR%d= start:%2.2X end:", IOcardsIntEnabled, (int)IO_PendingInt, HPIB[sc]->LastIOPcmd, sc+3, HPIB[sc]->PSR);
#endif
		} else {
			// if INT is set, set PACK, else clear PACK
			if( val & 1 ) HPIB[sc]->PSR |= 8;	// set PACK
			else HPIB[sc]->PSR &= ~8;	// clear PACK

			if( (val & 2) && (HPIB[sc]->CCR & 0x01) && HPIB[sc]->OBfull ) {	// if CMD bit set AND in INTerrupt AND have a command in OB, execute the CMD
				HPIB[sc]->CCR = val;
				val = HPIB[sc]->OB;
				ExecIOPcmd(sc, val);
			}
			HPIB[sc]->CCR = val;

			if( (val & 4) && HPIB[sc]->IBcnt ) {	// if CED && still stuff to send
				HPIB[sc]->IBcnt = 0;	// flush rest of buffer
				HPIB[sc]->PSR &= ~1;	// clear IBF
			}
		}
#if DEBUG
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), "%2.2X IOcIE=%d IOpI=%d", HPIB[sc]->PSR, IOcardsIntEnabled, (int)IO_PendingInt);
	WriteDebug(ScratBuf);
#endif
	} else {	// READ (PSR)
#if DEBUG
	sprintf((char*)ScratBuf, "PSR%d %2.2X IOcIE=%d IOpI=%d LastIOPcmd=%d", sc+3, HPIB[sc]->PSR, IOcardsIntEnabled, (int)IO_PendingInt, HPIB[sc]->LastIOPcmd);
	WriteDebug(ScratBuf);
#endif
		retval = HPIB[sc]->PSR;
		if( retval & 4 ) HPIB[sc]->PSR &= ~4;	// clear PED
	}
	return retval;	// READ
}

///*****************************************************************
// 177531 IBOB
BYTE ioHPIB_IBOB(WORD address, long val) {

	BYTE	retval=0, i, j;
	int		sc, v7;

	sc = (address-0177521)/2;

	if( val>=0 && val<256 ) {	// WRITE (OB)
#if DEBUG
	sprintf((char*)ScratBuf, "OB %2.2lX IOcIE=%d IOpI=%d LastIOPcmd=%d PSR%d= start:%2.2X end:", val, IOcardsIntEnabled, (int)IO_PendingInt, HPIB[sc]->LastIOPcmd, sc+3, HPIB[sc]->PSR);
#endif
		HPIB[sc]->OB = val;
		HPIB[sc]->OBfull = TRUE;	// preset to OB buffer full - WHEN OB BYTE IS USED, OBfull *MUST* be cleared!!!!

		if( !HPIB[sc]->BurstDir && (HPIB[sc]->CCR & 2) ) ExecIOPcmd(sc, val);
		else if( HPIB[sc]->BurstDir ) {
			HPIB[sc]->OBfull = FALSE;
			if( HPIB[sc]->BurstDir==0x21 ) {	// if BURST IN
#if DEBUG
//	sprintf((char*)ScratBuf+strlen(ScratBuf), "%2.2X IOcIE=%d IOpI=%d", HPIB[sc]->PSR, IOcardsIntEnabled, (int)IO_PendingInt);
//	WriteDebug(ScratBuf);
#endif
				return 0;	// ignore, dummy write to start BURST IN
			}
			if( HPIB[sc]->BurstDir==0x20 ) {	// if BURST OUT
				if( HPIB[sc]->OB_PTR != NULL ) {
					if( !IsDiskProtected(sc, BurstTLA(sc), CurDiskUnit(sc, BurstTLA(sc))) ) {
						*HPIB[sc]->OB_PTR = val;
#if DEBUG
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), "OB %2.2X IOcIE=%d IOpI=%d LastIOPcmd=%d", (BYTE)val, IOcardsIntEnabled, (int)IO_PendingInt, HPIB[sc]->LastIOPcmd);
	WriteDebug(ScratBuf);
#endif
					} else {	// trying to write to a write-protected disk
						GenWriteProtectError(sc, BurstTLA(sc));
					}
					++HPIB[sc]->OB_PTR;
					if( --HPIB[sc]->BurstCnt == 0 ) {		// need to terminate BURST IO with interrupt
						for(i=0; i<31; i++) if( HPIB[sc]->LA[i] && HPIB[sc]->devProc[i]!=NULL ) HPIB[sc]->devProc[i](DP_TERMBURST, sc, i, 0, 0);
					}
				}
				return 0;	// it's a write, the return value doesn't matter
			}
		} else if( HPIB[sc]->STATE==IOSTATE_WAITFORCMD ) {
			HPIB[sc]->OBfull = FALSE;
			// The MSbit of 'val' is the PARITY bit, and is set when the lower 7 bits
			// have an ODD number of bits set so we screen it out.
			v7 = val & 0x7F;	// strip the parity bit
			if( v7>=0x20 && v7<0x60 ) {	// LAD/UNL/TAD/UNT, don't pass on to devices
				if( v7 < 0x40 ) {		// LAD/UNL
					if( v7==0x3F ) {	// UNL
						// Flush devFilePRT to file
						for(i=0; i<31; i++) {
							if( HPIB[sc]->devProc[i]==&devFilePRT ) {
								for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
								HPIB[sc]->devProc[i](DP_FLUSH, sc, i, 0, j);
							}
						}
						// UNL everybody
						memset(HPIB[sc]->LA, 0, 32);
					} else {			// LAD
						// turn on LISTEN for addressed device
						HPIB[sc]->LA[val & 0x1F] = TRUE;
						// if not disk drive, set state back to IDLE
						if( ((DEVSTRUCT*)HPIB[sc]->devData[val & 0x1F])->devType > DEV_MAX_DISK_TYPE ) HPIB[sc]->STATE = IOSTATE_IDLE;
					}
				} else {				// TAD/UNT
					// UNT everybody
					memset(HPIB[sc]->TA, 0, 32);
					// if TAD, turn on TALK for addressed device
					if( v7!=0x5F ) HPIB[sc]->TA[val & 0x1F] = TRUE;
				}
			} else {
				i = v7 & 0x1F;
				// if UNT then SECONDARY, IDENTIFY the device at the SECONDARY ADDRESS
				if( HPIB[sc]->LastHPIBcmd==0x5F ) {
					if( v7>=0x60 && HPIB[sc]->devProc[i]!=NULL ) HPIB[sc]->devProc[i](DP_HPIB_CMD, sc, i, 1, v7);
				}
				for(i=0; i<31; i++) if( (HPIB[sc]->LA[i] || HPIB[sc]->TA[i]) && HPIB[sc]->devProc[i]!=NULL ) HPIB[sc]->devProc[i](DP_HPIB_CMD, sc, i, 0, v7);
			}
			HPIB[sc]->LastHPIBcmd = v7;
			if( HPIB[sc]->CCR & 4 ) {	// CED set
				HPIB[sc]->STATE = IOSTATE_IDLE;
			}
		} else if( HPIB[sc]->STATE==IOSTATE_IDLE ) {
			if( !(HPIB[sc]->CCR & 0x01) ) {	// if NOT in INTerrupt
				HPIB[sc]->OBfull = FALSE;
				// The actual IOP command was written first and handled in ExecIOPcmd,
				// which saved the command in HPIB[sc]->LastIOPcmd.  This is now a following
				// command "argument byte" which needs to be dealt with.  Currently, only
				// a few of the commands have such following argument bytes.
				switch( HPIB[sc]->LastIOPcmd ) {
				 case 0x00:	// READ STATUS
				 	// don't default and output the byte
					break;
				 case 0x10:	// INPUT
				 	// don't default and output the byte
					break;
				 case 0x80:	// WRITE CONTROL
				 case 0x90:
					HPIB[sc]->CR[HPIB[sc]->CRptr++] = val;
					break;
				 case 0xA0:	// OUTPUT
					DevOutput(sc, val);
					if( HPIB[sc]->CCR & 4 ) {	// if CED set, OUTPUT EOL sequence
						for(i=0; i<(HPIB[sc]->CR[16] & 7); i++) DevOutput(sc, HPIB[sc]->CR[17+i]);
					}
					break;
				 case 0xE0:	// WRITE AUXILIARY
					// only used when reading EOI/EOL, ignore
				 	// don't default and output the byte
					break;
				 default:
					DevOutput(sc, val);
					break;
				}
			}
		}
	} else {	// READ (IB)
		retval = HPIB[sc]->IB[HPIB[sc]->IBptr];
#if DEBUG
	sprintf((char*)ScratBuf, "IB %2.2X IOcIE=%d IOpI=%d PSR%d= start:%2.2X end:", retval, IOcardsIntEnabled, (int)IO_PendingInt, sc+3, HPIB[sc]->PSR);
#endif
		if( HPIB[sc]->IBcnt ) {
			HPIB[sc]->PSR |= 1;			// make sure IBF set
			if( --HPIB[sc]->IBcnt ) {	// if not last byte
				++HPIB[sc]->IBptr;			// advance to next byte to input
				HPIB[sc]->PSR &= ~4;		// make sure PED is not set (not last byte of data)
			} else {					// IS last byte
											// don't advance ptr, leave pointing at last byte so any following reads will get that byte
				HPIB[sc]->PSR |= 4;			// set PED since this is the last byte of data
			}
		} else HPIB[sc]->PSR &= ~5;	// clear IBF and PED

		if( HPIB[sc]->IBcnt==0 ) {	// end of INPUT, notify devices
			for(i=0; i<31; i++) {
				if( HPIB[sc]->devProc[i]!=NULL ) {
					for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
					HPIB[sc]->devProc[i](DP_INPUT_DONE, sc, i, 0, j);
				}
			}
		}
	}
#if DEBUG
	sprintf((char*)ScratBuf+strlen((char*)ScratBuf), "%2.2X IOcIE=%d IOpI=%d", HPIB[sc]->PSR, IOcardsIntEnabled, (int)IO_PendingInt);
	WriteDebug(ScratBuf);
#endif
	return retval;	// READ
}

///******************************************
///******************************************
///******************************************
///******************************************
///******************************************
///******************************************
///******************************************
/// function to call for setup/de-setup/etc
///
// model=0,1,2 (HP85A, HP85B, HP87)
// sc=0-15 (0=S.C. 3, 7=S.C. 10, 8-15=non-select-code I/O)
//
// NOTE: iR_READ_INI gets sent BEFORE iR_STARTUP, so that the
// INI file settings are in memory in order for iR_STARTUP to
// properly ...er... start up.  So, in the iR_READ_INI code,
// do NOT depend upon anything having been set up yet!
//
/*
	When the program is first started:
		HP85OnStartup() calls ReadIni().
		ReadIni() sends all IO cards initProc(iR_READ_INI) [which for HPIB is initHPIB(iR_READ_INI)],
		Then HP85OnStartup() calls initProc(iR_STARTUP).
		Then HP85OnStartup() calls HP85PWO().
	HP85POW() calls initProc(iR_RESET).

	When the user OPENs the SERIES 80 OPTIONS dialog:
		initProc(iR_FLUSH)	[to save any memory-held changes to disk/printer/wherever]
		CreateRomDlg()		[to create the dialog]
	When the user CLOSEs the SERIES 80 OPTIONS dialog:
		SetupDispVars()		[to re-setup the display for the 'new' machine]
		HP85PWO()			[which sends initProc(iR_RESET) to reset the 'new' machine to power-on state]
*/
int initHPIB(long reason, long model, long sc, DWORD arg) {

	long	i, j;

#if DEBUG
	if( reason!=iR_TIMER ) {
		sprintf((char*)ScratBuf,"initHPIB(REASON==%ld, MODEL==%ld, SC==%ld, %lu) IOcIE=%d IOpI=%d", reason, model, sc, arg, IOcardsIntEnabled, (int)IO_PendingInt);
		WriteDebug(ScratBuf);
	}
#endif // DEBUG
	switch( reason ) {
	 case iR_STARTUP:	// program is starting up, do intialization for this card
		// FIRST take over IO vectors for our Select Code
		ioProc[0177520+sc*2-0177400] = &ioHPIB_CCRPSR;	// hook into the IO
		ioProc[0177521+sc*2-0177400] = &ioHPIB_IBOB;

		for(i=0; i<31; i++) {
			if( HPIB[sc]->devProc[i]!=NULL ) {
				for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
				HPIB[sc]->devProc[i](DP_STARTUP, sc, i, 0, j);
			}
		}
		// reset the HPIB and peripherals
//		ResetHPIB(sc);
		ResetKey = FALSE;
		break;
	 case iR_SHUTDOWN:	// program is shutting down, release/save anything you need
		for(i=0; i<31; i++) {
			if( HPIB[sc]->devProc[i]!=NULL ) {
				for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
				HPIB[sc]->devProc[i](DP_FLUSH, sc, i, 0, j);
			}
		}
		for(i=0; i<31; i++) {
			if( HPIB[sc]->devProc[i]!=NULL ) {
				for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
				HPIB[sc]->devProc[i](DP_SHUTDOWN, sc, i, 0, j);
			}
		}
		break;
	 case iR_ENINT:
		// When a card interrupts the CPU and the CPU reads INTRSC,
		// it expects the card to provide the lower byte of its IO address.
		// Since the IO addresses start at 177520, that gives us a starting
		// lower byte of 120 (octal).  Each select code gets two addresses
		// (CCR/PSR and OB/IB), so we have to double the select code and add
		// it to 0120 to generate the appropriate value to respond with,
		// which we store in IO_SC.
		//
		// SC bits are 0101BBB0, where BBB=
		//	000 = SC 3  177520
		//	001 = SC 4  177522
		//	010 = SC 5  177524
		//	011 = SC 6  177526
		//	100 = SC 7  177530
		//	101 = SC 8  177532
		//	110 = SC 9  177534
		//	111 = SC 10 177536
		if( IOcardsIntEnabled && !(Int85 & INT_IO) && IO_PendingInt & (1<<sc) ) {	// if we have an INT pending, take the int
#if DEBUG
		sprintf((char*)ScratBuf,"ENTRY iR_ENINT(%ld, %ld, %ld, %lu) IOcIE=%d IOpI=%d", reason, model, sc, arg, IOcardsIntEnabled, (int)IO_PendingInt);
		WriteDebug(ScratBuf);
#endif // DEBUG
			IO_SC = 0120 | (sc<<1);		// set IO address based upon SC
			Int85 |= INT_IO;			// interrupt by IO
			IO_PendingInt &= ~(1<<sc);	// clear our request no matter what
#if DEBUG
		sprintf((char*)ScratBuf,"EXIT iR_ENINT(%ld, %ld, %ld, %lu) IOcIE=%d IOpI=%d", reason, model, sc, arg, IOcardsIntEnabled, (int)IO_PendingInt);
		WriteDebug(ScratBuf);
#endif // DEBUG
		}
		break;
	 case iR_DISINT:

		break;
	 case iR_TIMER:
		for(i=0; i<31; i++) if( HPIB[sc]->devProc[i]!=NULL ) HPIB[sc]->devProc[i](DP_TIMER, sc, i, 0, arg);
		break;
	 case iR_RESET:
	 	ResetKey = FALSE;
	 	ResetHPIB(sc);
		break;
	 case iR_WRITE_INI:
		for(i=0; i<MAX_DEVICES; i++) {
			if( Devices[i].devType!=DEVTYPE_NONE && Devices[i].sc==sc ) {
				Devices[i].initProc(DP_INI_WRITE, Devices[i].sc, Devices[i].ca, 0, i);
			}
		}
		break;
	 case iR_READ_INI:
		 // NOTE: See the NOTE at the top of this function!!!
		 // 	(This gets called BEFORE iR_STARTUP!!!!)

		// since this is the first place called during program startup, we allocate the data structure here the first time through
		if( HPIB[sc]==NULL ) {
			HPIB[sc] = (tHPIB*)malloc(sizeof(tHPIB));
			memset(HPIB[sc], 0, sizeof(tHPIB));
		}
		for(i=0; i<MAX_DEVICES; i++) {
			if( Devices[i].devType!=DEVTYPE_NONE && Devices[i].sc==sc ) {
				Devices[i].initProc(DP_INI_READ, Devices[i].sc, Devices[i].ca, 0, i);
			}
		}
		break;
	 case iR_DRAW:
	 	for(i=0; i<31; i++) {
			if( HPIB[sc]->devProc[i]!=NULL ) {
				for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
				HPIB[sc]->devProc[i](DP_DRAW, sc, i, 0, j);
			}
	 	}
		break;
	 case iR_FLUSH:	// flush any output data, either RESET or SHUTDOWN is coming...
		for(i=0; i<31; i++) {
			if( HPIB[sc]->devProc[i]!=NULL ) {
				for(j=0; j<MAX_DEVICES; j++) if( Devices[j].sc==sc && Devices[j].ca==i ) break;
				HPIB[sc]->devProc[i](DP_FLUSH, sc, i, 0, j);
			}
		}
		break;
	 default:
	 	sprintf((char*)ScratBuf, "Unexpected call to initHPIB(%ld, %ld, %ld, %lu)", reason, model, sc, arg);
		MessageBox(NULL, (char*)ScratBuf, "Oops!", MB_OK);
		break;
	}
	return 0;
}

