///**************************************************************
/// devFilePRT.c - printing to a system FILE
///**************************************************************

#include <stdio.h>
#include <string.h>
//#include <malloc.h>

#include	"main.h"
#include	"mach85.h"

typedef struct {
	int		devType;		// DEVTYPE_ value, to identify what kind of DEV structure this is (MUST be the first element of the structure! The same on all DEV structures)
	long	hFile;			// handle for "print to file" file PRT710.TXT
} FILEPRT;

///**************************************************************
void OpenFilePRT(int sc, int tla) {

	FILEPRT	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_FILEPRT)) ) {
		if( pD->hFile==-1 ) {
			sprintf((char*)ScratBuf, "PRT%d%2.2d.TXT", 3+sc, tla);	// change when FilePRT becomes movable
			if( -1 == (pD->hFile=Kopen((char*)ScratBuf, O_WRBIN)) ) {
				pD->hFile = Kopen((char*)ScratBuf, O_WRBINNEW);
			}
		}
		if( pD->hFile != -1 ) Kseek(pD->hFile, 0, SEEK_END);
	}
}

///**************************************************************
int devFilePRT(long reason, int sc, int tla, int d, int val) {

	FILEPRT	*pD;

	switch( reason ) {
	 case DP_INI_READ:	// tla is undefined, we're defining it HERE
	 	/// sc & tla are valid, val==index into Devices[]
	 	/// At DP_INI_READ-time, Devices[val].devID has been set to the "devXX=" number (XX) in the .ini file.
	 	/// That .devID value may very well change between DP_INI_READ and DP_INI_WRITE!
		TLAset(sc, tla, &devFilePRT);	// Tell cardHPIB what function to call for this SC/TLA address
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
			sprintf((char*)ScratBuf+32, "\"%s\" %d %d", iniSTR[INIS_FILEPRT], Devices[val].wsc+3, Devices[val].wca);	// device address
			iniSetVar(ScratBuf, ScratBuf+32);
		}
	 	break;
	 case DP_FLUSH:
flush:
		if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_FILEPRT)) ) {
			if( pD->hFile != -1 ) {
				Kclose(pD->hFile);
				pD->hFile = -1;
			}
		}
		break;
	 case DP_RESET:
	 	break;
	 case DP_STARTUP:
	 	// allocate our memory storage
	 	CreateDevData(sc, tla, sizeof(FILEPRT), DEVTYPE_FILEPRT);
	 	// and initialize it
		if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_FILEPRT)) ) {
			pD->hFile = -1;
		}
	 	break;
	 case DP_SHUTDOWN:
	 	DestroyDevData(sc, tla);
	 	break;
	 case DP_INPUT_DONE:
	 	break;
	 case DP_NEWCMD:
		switch( val ) {
		 case 0x47:		// send EOL sequence
		 case 0xA0:	// OUTPUT
		 	// both of these need to make sure the file is OPEN for the upcoming OUTPUTs (or EOL sequence)
			if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_FILEPRT)) ) {
				if( pD->hFile == -1 ) {
					if( IsListener(sc, tla) ) OpenFilePRT(sc, tla);
				}
			}
			break;
		 default:
			goto flush;
		}
		break;
	 case DP_OUTPUT:
		if( NULL!=(pD=GetDevData(sc, tla, DEVTYPE_FILEPRT)) ) {
			if( pD->hFile == -1 ) {
				if( IsListener(sc, tla) ) OpenFilePRT(sc, tla);
			}
			if( pD->hFile != -1 ) Kwrite(pD->hFile, (BYTE *)&val, 1);
		}
		break;
	}
	return val;
}

