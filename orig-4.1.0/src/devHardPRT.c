///**************************************************************
/// devHardPRT.c - printing to a system FILE
///**************************************************************

#include <stdio.h>
#include <string.h>
//#include <malloc.h>

#include	"main.h"
#include	"mach85.h"

typedef struct {
	int		devType;		// DEVTYPE_ value, to identify what kind of DEV structure this is (MUST be the first element of the structure! The same on all DEV structures)
	// values for printing to actual printer
	DWORD	PageW;			// Width of page in printer pixels
	DWORD	PageH;			// Height of page in printer pixels
	DWORD	ppiX;			// Pixels Per Inch in X direction
	DWORD	ppiY;			// Pixels Per Inch in Y direction
	DWORD	PageStarted;	// if a page has been started or not
	DWORD	Y;				// Y-position on page of next line
	BYTE	buf[512];		// output buffer for current line
	DWORD	bufptr;			// index into buf[] for next char
	WORD	FontH;			// Height of 1 line of text on printer
	WORD	FontAscendH;	// ascending Height of printer font
	DWORD	hPRT;			// 'handle' for REAL printer (0 if not opened for printing)
} HARDPRT;

///**************************************************************
void DumpDev20(int sc, int tla) {

	HARDPRT	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_HARDPRT)) ) {
		// if something in printer buffer
		if( pD->bufptr ) {
			// if page not yet started on printer
			if( !pD->PageStarted ) {
				KPrintBeginPage();
				pD->Y = 0;
				pD->PageStarted = TRUE;
			}
			pD->buf[pD->bufptr] = 0;	// nul-term buffer string
			KPrintText(0, pD->Y, (char*)pD->buf, CLR_BLACK);	// print it
			pD->bufptr = 0;	// empty buffer
		}
	}
}

///**************************************************************
int devHardPRT(long reason, int sc, int tla, int d, int val) {

	HARDPRT	*pD;

	switch( reason ) {
	 case DP_INI_READ:
	 	/// sc & tla are valid, val==index into Devices[]
	 	/// At DP_INI_READ-time, Devices[val].devID has been set to the "devXX=" number (XX) in the .ini file.
	 	/// That .devID value may very well change between DP_INI_READ and DP_INI_WRITE!
		TLAset(sc, tla, &devHardPRT);	// Tell cardHPIB what function to call for this SC/TLA address
		/// if no .ini file existed, then the Devices[val].state will =DEVSTATE_NEW, else it twill =DEVSTATE_BOOT
	 	break;
	 case DP_INI_WRITE:
	 	/// NOTE: SC and CA for devices MUST be written from Devices[].wsc and Devices[].wca,
	 	/// *NOT* from Devices[].sc and Devices[].ca, in case the device address was changed
	 	/// via the DEVICES dialog.
	 	///
	 	/// NOTE: for DP_INI_WRITE, 'val' is the Devices[] index for this device
	 	/// At DP_INI_WRITE-time, Devices[val].devID has been set to the sequential number XX
	 	/// to be written to the .ini file's "devXX=" entry.  The devID value may very well
	 	/// be different between DP_INI_READ-time and DP_INI_WRITE-time.  Do NOT count on them
	 	/// staying the same between those points.
	 	///
	 	/// NOTE: Devices[].state must be checked for:
	 	///		DEVSTATE_BOOT    = the device exists/is-live, write its state out to .ini
	 	///		DEVSTATE_NEW     = the device was added in DEVICES dialog, but doesn't yet exist, write out a "new state" for it
	 	///		DEVSTATE_DELETED = the device exists/is-live, but was deleted in the DEVICES dialog, do NOT write out ANY .ini entry for it
		if( Devices[val].state==DEVSTATE_BOOT || Devices[val].state==DEVSTATE_NEW ) {
			iniSetSec(iniSTR[INIS_EMULATOR]);
			sprintf((char*)ScratBuf, "%s%d", iniSTR[INIS_DEV], Devices[val].devID);
			sprintf((char*)ScratBuf+32, "\"%s\" %d %d", iniSTR[INIS_HARDPRT], Devices[val].wsc+3, Devices[val].wca);	// device address
			iniSetVar(ScratBuf, ScratBuf+32);
		}
	 	break;
	 case DP_FLUSH:
		if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_HARDPRT)) ) {
			if( pD->PageStarted && pD->hPRT ) {
				if( pD->bufptr ) DumpDev20(sc, tla);
				if( pD->PageStarted ) {
					KPrintEndPage();
					pD->PageStarted = FALSE;
				}
			}
			if( pD->hPRT ) {
				KPrintStop();
				pD->hPRT = 0;
			}
		}
		break;
	 case DP_RESET:
	 	break;
	 case DP_STARTUP:
	 	// allocate our memory storage
	 	CreateDevData(sc, tla, sizeof(HARDPRT), DEVTYPE_HARDPRT);
	 	// and initialize it
		if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_HARDPRT)) ) {
			pD->PageStarted = pD->hPRT = 0;
		}
	 	break;
	 case DP_SHUTDOWN:
	 	DestroyDevData(sc, tla);
	 	break;
	 case DP_INPUT_DONE:
	 	break;
	 case DP_NEWCMD:
		switch( val & 0xF0 ) {
		 case 0xA0:	// OUTPUT
			if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_HARDPRT)) ) {
				if( pD->hPRT==0 ) {	// if printer is not 'open', 'open' it
					pD->hPRT = KPrintStart(&pD->PageW, &pD->PageH, &pD->ppiX, &pD->ppiY);
					pD->PageStarted = FALSE;
					pD->bufptr = 0;

					KPrintBeginPage();
					pD->Y = 0;
					pD->PageStarted = TRUE;
					// 12 point, fixed pitch font
					KPrintFont(12, 1, &pD->FontH, &pD->FontAscendH);
				}
			}
			break;
		}
	 	break;
	 case DP_OUTPUT:
		if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_HARDPRT)) ) {
			if( val==13 ) DumpDev20(sc, tla);
			else if( val==10 ) {
				pD->Y += pD->FontH;	// move down 1 line on page
				// if at bottom of page
				if( pD->Y + pD->FontH > pD->PageH ) {
					KPrintEndPage();
					pD->PageStarted = FALSE;
				}
			} else if( val==12 ) {
				DumpDev20(sc, tla);
				if( !pD->PageStarted ) {
					KPrintBeginPage();
					pD->Y = 0;
					pD->PageStarted = TRUE;
				}
				KPrintEndPage();
				pD->PageStarted = FALSE;
			} else {
				// leave 1 byte at end of buffer for nul-terminator
				if( pD->bufptr < sizeof(pD->buf)-1 ) pD->buf[pD->bufptr++] = val;
			}
		}
		break;
	}
	return val;
}

