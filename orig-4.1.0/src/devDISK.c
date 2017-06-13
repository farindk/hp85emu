///**************************************************************
/// devFilePRT.c - printing to a system FILE
///**************************************************************
/*

When adding a DEVTYPE_
	In ioDev.h:
		add DEVTYPE_ definition.
	In mach85.c:
		add entry in devProcs[].
		add iniSTR[] 'dev' label and #define in support.h
		add entry in ReadIni() code that reads in devices (if/else blocks)
	In dlg.c:
		add '+dev' BUTT definition  to DevicesDlg
		add to DEVD_names[]
	In dlg.h:
		add "dev name" to DEVD_names[]
Additionally, for DISK type devices:
	In cardHPIB.c:
		add entry to DiskParms[]
	In mach85.c:
		add disk-X-BM=NULL variable definition
		load the disk-X-BM bitmap in SetupDispVars()
		delete the disk-X-BM bitmap in HP85OnShutdown()
	In mach85.h:
		add extern disk-X-BM
	In devDISK.c:
		set .pBM for disk-X-BM in DP_STARTUP
		add to DrawDiskStatus()
		add to DiskMouseClick()
*/
#include <stdio.h>
#include <string.h>
//#include <malloc.h>

#include	"main.h"
#include	"mach85.h"

#if DEBUG
BYTE GetLastIOPcmd(int sc);
#endif // DEBUG

char	HPWLIFname[512];	// used during LIF<->WinDOS operations

//*********************************************
// for initializing the CHECKs for each drive at iR_STARTUP
CHECK DWshowINIT = {
	ID_DWSHOW0, CTL_CHECK,		// id, type
	10,10, 12, 12,					// xoff, yoff, w, h
	NULL, NULL, NULL, NULL,			// prev, next, child1, parent
	WF_CTL | CF_TABSTOP | CF_GROUP,	// flags
	&CheckProc,						// proc
	0,								// altchar
	NULL,							// kWnd
	""		// pText
};

typedef struct {
	int		devType;		// DEVTYPE_ value, to identify what kind of DEV structure this is (MUST be the first element of the structure! The same on all DEV structures)
	BYTE	UNIT;			// used by REQUEST STATUS
	BYTE	CMD;			// last CMD sent to disk drive

	struct	{
		BYTE	unit;
		BYTE	cylhi;
		BYTE	cyllo;
		BYTE	head;
		BYTE	sector;
		BYTE	unused;
	} HEDPOS;

	struct	{
		BYTE	unit;
		BYTE	formtype;
		BYTE	interleave;
		BYTE	bytedata;
	} FORMAT;

	struct	{
		BYTE	unit;
		BYTE	seccnt1;
		BYTE	seccnt2;
		BYTE	unused;
	} VERIFY;

	BYTE	DA[2];				// for REQUEST DISK ADDRESS
	BYTE	DSJ;				// 0 or 1 status value
	BYTE	STATUSwp, STATUStype;// write-protect and type status for return values for REQUEST STATUS command

	BYTE	DISKNAME[2][128];	// file names of disk images
	long	FOLDER[2];		// sub-folders for disk images
	BYTE	*DISK[2];			// ptrs to malloc'd RAM for :D700-:D701 disk images
	BOOL	ACTIVE[2];			// active flags (red light off/on)
	DWORD	ACTTIME[2];			// last TimeMilli() the drive was accessed (for ACTIVE[] time-out)

	BOOL	OUTd[2];			// if the drive is being written (TRUE) or read (FALSE) for keeping auto-directory listing up-to-date

	/// Disk window housekeeping variables
	long	iDiskWndX, iDiskWndY, iDiskWndW, iDiskWndH, iDiskWndMisc;	// for intialization ONLY!  NOT updated! (read at iR_READ_INI, use at iR_STARTUP)
	long	iDiskWnd;

	CHECK	DWshow[2];			// checkboxes for the auto-cat disk window
	long	DISKx, DISKy;		// location of disk IMAGE in disk WINDOW
	long	nameX[2], nameY[2], nameW;	// x/y and width of disk 'names'
	long	catX[2], catY, catW[2], catH;	// x/y and w/h of disk auto-cat windows
	DWORD	DiskFlags;			// DISKF_ settings (see .h)
} DISKDATA;

int		DiskType[8][32];		// holds devType for [sc][tla] disk drives for doing GetDevData() calls
int		DiskSize[8][32];		// size of disk for [sc][tla] disk drives (set at iR_READ_INI time)

BYTE	DISKERRint[]={0x4C, 0x4C, 0x4C, 0x4C};	// interrupt 'Disc error' triggers
BYTE	BURSTint[]={0x01, 0x01};
BYTE	IDENTITY82901[] = {0x01, 0x04};
int		ActiveBurstTLA;		// TLA of Burst-initiating disk drive

///**************************************************************
/// Generate "Disc error" if UNIT doesn't exist
BOOL UnitOK(int sc, int tla) {

	DISKDATA	*pD;

	if( NULL==(pD=GetDevData(sc, tla, DiskType[sc][tla])) || pD->UNIT>DiskParms[pD->devType].units || pD->HEDPOS.unit>DiskParms[pD->devType].units ) {
		HPIBint(sc, DISKERRint, 4);
#if DEBUG
		sprintf((char*)ScratBuf,"UnitOK(%d) IOcIE=%d IOpI=%d", sc, IOcardsIntEnabled, (int)IO_PendingInt);
		WriteDebug(ScratBuf);
#endif // DEBUG
		return FALSE;
	}
	return TRUE;
}

///************************************************
/// For dlg.c
void AdjustDisks(int sc, int tla) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) AdjustDiskLists(pD->iDiskWnd);
}

///************************************************
void DiskMouseClick(int sc, int tla, int x, int y, int flags) {

	int			d, xdiv, i;
	DISKDATA	*pD;
	PBMB		dBM;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) && Halt85==FALSE && ActiveDlg==NULL ) {
		dBM = *DiskParms[pD->devType].pBM;
		if( DiskType[sc][tla]==DEVTYPE_3_5 ) xdiv = (CfgFlags & CFG_BIGCRT)?277:138;
		else if( DiskType[sc][tla]==DEVTYPE_5_25 ) xdiv = disk525BM->physW/2;
		else if( DiskType[sc][tla]==DEVTYPE_8 ) xdiv = disk8BM->physW/2;
		else if( DiskType[sc][tla]==DEVTYPE_5MB ) xdiv = disk5MBBM->physW;
		else {
			SystemMessage("Set xdiv in devDISK.c @ DiskMouseClick");
			xdiv = 32000;	// force 'd' to 0 below
		}
		if( x>=pD->DISKx && y>=pD->DISKy && x<pD->DISKx+dBM->physW && y<pD->DISKy+dBM->physH/2 ) {
			d = (x>=pD->DISKx + xdiv)?1:0;

			if( flags & 8 ) {	// CTL down, send MASS STORAGE IS ":Dxxx" to keyboard buffer
				KeyMemLen = 256;
				if( NULL != (pKeyMem = (BYTE *)malloc(KeyMemLen)) ) {
					sprintf((char*)pKeyMem, "MASS STORAGE IS \":D%d%d%d\"\r", 3+sc, 0, d);
					pKeyPtr = pKeyMem;
				}
			} else if( flags & 4 ) {	// SHIFT down

			} else {
				CreateDiskDlg(sc, tla, d, pD->devType);
			}
		} else {
			for(i=0; i<DiskParms[pD->devType].units; i++) {
				if( x>=pD->DWshow[i].xoff && x<pD->DWshow[i].xoff+pD->DWshow[i].w && y>=pD->DWshow[i].yoff && y<pD->DWshow[i].yoff+pD->DWshow[i].h ) {
					SendMsg((WND*)&pD->DWshow[i], DLG_ACTIVATE, 0, 0, 0);
				}
			}
			AdjustDiskLists(pD->iDiskWnd);
			DrawDisks(sc, tla);
		}
	}
}

///******************************************
void GenWriteProtectError(int sc, int tla) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		pD->DSJ = 1;
		pD->STATUStype = 023;
	}
}

///******************************************
int BurstTLA(int sc) {

	return ActiveBurstTLA;
}

///******************************************
int CurDiskUnit(int sc, int tla) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) return pD->UNIT;
	return 0;
}

///******************************************
BOOL IsDisk(int sc, int tla, int unit) {

	DISKDATA	*pD;

	if( NULL==(pD=GetDevData(sc, tla, DiskType[sc][tla])) || sc<0 || sc>7 || tla<0 || tla>7 || unit<0 || unit>=DiskParms[pD->devType].units ) return FALSE;
	return pD->DISKNAME[unit][0];
}

///******************************************
BOOL IsDiskInitialized(int sc, int tla, int unit) {

	DISKDATA	*pD;

	if( NULL==(pD=GetDevData(sc, tla, DiskType[sc][tla])) || !IsDisk(sc, tla, unit) ) {
		return FALSE;
	} else {
		return *pD->DISK[unit]==0x80;
	}
}

///******************************************
BOOL IsDiskProtected(int sc, int tla, int unit) {

	DISKDATA	*pD;

	if( NULL==(pD=GetDevData(sc, tla, DiskType[sc][tla])) || !IsDisk(sc, tla, unit) ) return TRUE;
	return pD->DISK[unit][255]==0x01;
}

///******************************************
char *GetDiskName(int sc, int tla, int unit) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) return (char*)pD->DISKNAME[unit];
	return NULL;
}

///******************************************
void WriteDiskByte(int sc, int tla, int unit, int offset, BYTE b) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) pD->DISK[unit][offset] = b;
}

///******************************************
BYTE ReadDiskByte(int sc, int tla, int unit, int offset) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) return pD->DISK[unit][offset];
	return 0xFF;
}

///******************************************
int GetDiskFolder(int sc, int tla, int unit) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) return pD->FOLDER[unit];
	return 0;
}

///******************************************
void SetDiskFolder(int sc, int tla, int unit, int f) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) pD->FOLDER[unit] = f;
}

///*********************************************************************************
// At entry:
//	hFile is the source file, open and positioned at the start of actual LIF file data.
//	ScratBuf[256-287] (32 bytes) contains the LIF directory entry for the file
// Returns TRUE if import successful, FALSE if fails.
//
BOOL ImportFileSub(int sc, int tla, int unit, long hFile, char *lifN) {

	WORD	stype, slensects, slenbytes, sbytesrec;//, sbegsect;
	WORD	dtype, dbegsect, dlensects, remsects, nxtsect;
	BYTE	*pD, *pDhole;
	long	i, nd;
	char	lifName[16];
	BOOL	retval=FALSE;
	DISKDATA	*pDD;

	if( NULL!=(pDD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		for(i=0; lifN[i]; i++) lifName[i] = lifN[i];
		// tail-fill target LIF filename with blanks
		for(; i<10; i++) lifName[i] = ' ';
		lifName[i] = 0;

		// file type is stored BIG-endian, but we don't care
		stype = *((WORD *)(ScratBuf+256+10));
		// beginning sector and file len in sectors are both
		// stored BIG-endian
		slensects = (((WORD)ScratBuf[256+18])<<8) + (WORD)ScratBuf[256+19];
		// file len in bytes and bytes per logical record are both
		// stored LITTLE-endian
		slenbytes = *((WORD *)(ScratBuf+256+28));
		sbytesrec = *((WORD *)(ScratBuf+256+30));

		if( pDD->DISKNAME[unit]==0 || pDD->DISK[unit][0]!=0x80 ) {
			SystemMessage("Unable to write to disk");
		} else {
		// scan :D700 directory for PURGED file that is long enough
		// or for NEXT-AVAIL FILE with enough sectors left on disk.
			// get # dir entries in D700
			nd = 8*(long)((((WORD)pDD->DISK[unit][18])<<8)+((WORD)pDD->DISK[unit][19]));
			// get ptr to first directory entry
			pD = pDD->DISK[unit] + 256*((((WORD)pDD->DISK[unit][10])<<8)+((WORD)pDD->DISK[unit][11]));
			pDhole = NULL;
			// empty disk, beginning sector# =
			//	sect# of beginning of directory
			//	+ #sectors in the directory
			dbegsect = ((((WORD)pDD->DISK[unit][10])<<8)+((WORD)pDD->DISK[unit][11])) + nd/8;
			nxtsect = dbegsect;
			for(i=0; i<nd; i++, pD+=32) {
				dtype = *((WORD *)(pD+10));
				if( dtype==0xFFFF ) {	// reached NEXT-AVAIL-FILE
					if( pDhole==NULL ) {
						pDhole = pD;
					}
					break;
				}
				dbegsect = (((WORD)(*(pD+14)))<<8) + (WORD)(*(pD+15));
				dlensects = (((WORD)(*(pD+18)))<<8) + (WORD)(*(pD+19));
				nxtsect = dbegsect + dlensects;
				// if not PURGED and file name is same, error
				if( dtype && !strncmp((char*)pD, lifName, 10) ) {
					sprintf((char*)ScratBuf+1024, "Duplicate LIF file name: %s", lifName);
					SystemMessage((char*)ScratBuf+1024);
					goto bail;
				}
				if( pDhole==NULL ) {
					if( dtype==0 ) {	// if PURGEd file
						if( dlensects >= slensects ) pDhole = pD;
					}
				}
			}
			pD = pDhole;
			dtype = *((WORD *)(pD+10));
			if( dtype==0xFFFF ) {
				// total sectors = 33*32 (#tracks * sectorsPERtrack)
				// # directory sectors = nd/8 (#directoryEntries / #entriesPERsector)
				// dbegsect = beginning sector# for this file or hole
	// Changed 8-4-16 was subtracting volume and directory sectors twice!
	//				remsects = 33*32-nd/8-2-dbegsect;
				remsects = DiskParms[pDD->devType].cylinders * DiskParms[pDD->devType].cylsects-dbegsect;
				dlensects= slensects;
				dbegsect = nxtsect;
			} else {
				remsects = (((WORD)(*(pD+18)))<<8) + (WORD)(*(pD+19));
				dlensects= remsects;
				dbegsect = (((WORD)(*(pD+14)))<<8) + (WORD)(*(pD+15));
			}
		// create directory entry
			if( i>=nd ) {
				sprintf((char*)ScratBuf+1024, "LIF Directory is full: %s", lifName);
				SystemMessage((char*)ScratBuf+1024);
			} else if( slensects > remsects ) {
				sprintf((char*)ScratBuf+1024, "Full disc: %s", lifName);
				SystemMessage((char*)ScratBuf+1024);
			} else {
				strncpy((char*)pD, lifName, 10);
				*((WORD *)(pD+10)) = stype;
				*((WORD *)(pD+12)) = 0;
				pD[14] = (dbegsect>>8)&0x00FF;
				pD[15] = dbegsect & 0x00FF;
				*((WORD *)(pD+16)) = 0;
				pD[18] = (dlensects>>8)&0x00FF;
				pD[19] = dlensects & 0x00FF;
				*((WORD *)(pD+20)) = 0;
				*((WORD *)(pD+22)) = 0;
				*((WORD *)(pD+24)) = 0;
				pD[26] = 0x80;
				pD[27] = 0x01;
				*((WORD *)(pD+28)) = slenbytes;
				*((WORD *)(pD+30)) = sbytesrec;
		// read file contents into DEV_DISK at appropriate sector location
				pD = pDD->DISK[unit] + 256*dbegsect;
				Kread(hFile, pD, 256*slensects);
				retval = TRUE;
			}
		}
	}
bail:
	return retval;
}

///*********************************************************************************
void ImportFile(int sc, int tla, int unit, char *osName, char *lifN) {

	long	hFile;
	char	*pN;
	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		if( -1 == (hFile=KopenAbs(osName, O_RDBIN)) ) {
			sprintf((char*)ScratBuf+1024, "Unable to open: %s", osName);
			SystemMessage((char*)ScratBuf+1024);
		} else {
			for(pN=osName + strlen(osName); pN>osName && *(pN-1)!='\\' && *(pN-1)!=':'; --pN);	// strip PATH off FILENAME
			if( !stricmp(pN, "HPWLIF.DIR") ) {	// if importing a directory of HPWUTIL files
				char	*pLIFDIR, *pCURDIR, *pHPWADDNAME, *pDOSLIST, tmpLIF[16];
				long	i, LifDirLen, NumDirs;

				LifDirLen = Kseek(hFile, 0, SEEK_END);
				Kseek(hFile, 0, SEEK_SET);
				pLIFDIR = (char *)malloc(LifDirLen);
				if( pLIFDIR==NULL ) SystemMessage("Failed to allocate memory for LIF directory");
				else {
					Kread(hFile, (BYTE*)pLIFDIR, LifDirLen);
					Kclose(hFile);

					// have HPWLIF.DIR file in pLIFDIR, now process all files in its directory

					strcpy(HPWLIFname, osName);
					pHPWADDNAME = HPWLIFname + (pN - osName);	// point to HPWLIF.DIR part of pathname, that's where we'll put each file name

					NumDirs = ((long)(*((BYTE*)(pLIFDIR+19))));	// limited to 255 directory sectors, but come ON, that's a reasonable limit!
					pDOSLIST = pLIFDIR + (2+NumDirs)*256;	// skip to end of LIF directory to find list of DOS names, 16 bytes each, nul-term'd, how convenient
					NumDirs *= 8;	// number of directory entries

					for(pCURDIR=pLIFDIR+512; NumDirs--; pCURDIR += 32, pDOSLIST += 16) {	// for each LIF file
						if( *((WORD*)(pCURDIR+10)) != 0xFFFF ) {	// if not deleted/empty file
							strcpy(pHPWADDNAME, pDOSLIST);
							if( -1 == (hFile=KopenAbs(HPWLIFname, O_RDBIN)) ) {
								sprintf((char*)ScratBuf+512, "Error opening %s", HPWLIFname);
								SystemMessage("ScratBuf+512");
								goto bailhpw;
							}
							for(i=0; i<10; i++) tmpLIF[i] = pCURDIR[i];	// copy target LIF name
							for(; i-- && tmpLIF[i]==' '; ) tmpLIF[i] = 0;	// trim trailing spaces for ImportFileSub expectations
							memcpy(ScratBuf+256, pCURDIR, 32);	// copy the directory entry for ImportFileSub

	//	hFile is the source file, open and positioned at the start of actual LIF file data.
	//	ScratBuf[256-287] (32 bytes) contains the LIF directory entry for the file

							ImportFileSub(sc, tla, unit, hFile, tmpLIF);
							Kclose(hFile);
						}
					}
				}
	bailhpw:
				free(pLIFDIR);
			} else {	// else importing a LIFUTIL file
				Kread(hFile, (BYTE *)ScratBuf, 512);	// read VOLUME and DIRECTORY sectors
				ImportFileSub(sc, tla, unit, hFile, lifN);	// try to import the file into the current :D700 disk
				Kclose(hFile);
			}
		}
	}
}

///******************************************
void ExportLIF(int sc, int tla, int unit, char *lifN, char *osName) {

	long	hFile;
	WORD	stype, sbegsect, slensects, slenbytes, sbytesrec;
	BYTE	*pD;
	long	i, nd;
	DISKDATA	*pDD;

	if( NULL!=(pDD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		// get # dir entries in D700
		nd = 8*(long)((((WORD)pDD->DISK[unit][18])<<8)+((WORD)pDD->DISK[unit][19]));
		// get ptr to first directory entry
		pD = pDD->DISK[unit] + 256*((((WORD)pDD->DISK[unit][10])<<8)+((WORD)pDD->DISK[unit][11]));
		for(i=0; i<nd; i++, pD+=32) {
			stype = *((WORD *)(pD+10));
			if( stype==0xFFFF ) {	// reached NEXT-AVAIL-FILE
				SystemMessage("LIF file not found");
				return;
			}
			// if not PURGED and file name is same, found file
			if( stype && !strncmp((char*)pD, lifN, 10) ) {
				sbegsect = (((WORD)(*(pD+14)))<<8) + (WORD)(*(pD+15));
				slensects = (((WORD)(*(pD+18)))<<8) + (WORD)(*(pD+19));
				slenbytes = *((WORD *)(pD+28));
				sbytesrec = *((WORD *)(pD+30));

			// create OS file
				if( -1 == (hFile=KopenAbs(osName, O_WRBINNEW)) ) {
					SystemMessage("Unable to create OS destination file!");
					return;
				}

			// write LIF volume/directory sectors to OS file
				memset(ScratBuf, 0, 512);
				ScratBuf[0] = 0x80;				// volume ID
				strcpy((char*)ScratBuf+2, "HFSLIF");	// volume name
				ScratBuf[11] = 1;				// start directory sector
				ScratBuf[12] = 0x10;			// HP 3000 constant
				ScratBuf[19] = 1;				// length of directory in sectors

				strncpy((char*)ScratBuf+256, lifN, 10);		// file name
				*((WORD *)(ScratBuf+256+10)) = stype;	// file type
				ScratBuf[256+15] = 2;					// start sector#
				ScratBuf[256+18] = (slensects>>8)&0x00FF;
				ScratBuf[256+19] = slensects & 0x00FF;	// len of file in sectors
				ScratBuf[256+26] = 0x80;
				ScratBuf[256+27] = 0x01;				// entire file is in this volume
				*((WORD *)(ScratBuf+256+28)) = slenbytes;
				*((WORD *)(ScratBuf+256+30)) = sbytesrec;

				ScratBuf[256+32+10] = 0xFF;
				ScratBuf[256+32+11] = 0xFF;				// filetype for next file is NEXT-AVAIL

				Kwrite(hFile, (BYTE *)ScratBuf, 512);

			// write LIF file to OS file
				Kwrite(hFile, pDD->DISK[unit] + 256*sbegsect, 256*slensects);
				Kclose(hFile);

				ActiveDlg = NULL;
				DrawAll();
				return;
			}
		}
		SystemMessage("LIF source file not found");
	}
}

///********************************************************
void DrawDiskStatus(int sc, int tla, int unit) {

	int			sx, sy, dx, dy, w, h;
	DISKDATA	*pD;
	PBMB		dBM;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) && pD->iDiskWnd ) {
		dBM = *DiskParms[pD->devType].pBM;
		if( pD->devType==DEVTYPE_5_25 ) {
			if( CfgFlags & CFG_BIGCRT ) {
				sx = pD->DISKNAME[unit][0] ? 17 : 184;
				sy = pD->ACTIVE[unit] ? 125 : 21;
				w  = 140;
				h  = 59;
				dx = pD->DISKx+17+unit*167;
				dy = pD->DISKy+21;
			} else {
				sx = pD->DISKNAME[unit][0] ? 9 : 92;
				sy = pD->ACTIVE[unit] ? 63 : 11;
				w  = 69;
				h  = 28;
				dx = pD->DISKx+9+unit*83;
				dy = pD->DISKy+11;
			}
		} else if( pD->devType==DEVTYPE_3_5 ) {
			if( CfgFlags & CFG_BIGCRT ) {
				sx = pD->DISKNAME[unit][0] ? 149 : 283;
				sy = pD->ACTIVE[unit] ? 132 : 28;
				w  = 124;
				h  = 37;
				dx = pD->DISKx+149+unit*134;
				dy = pD->DISKy+28;
			} else {
				sx = pD->DISKNAME[unit][0] ? 75 : 142;
				sy = pD->ACTIVE[unit] ? 62 : 10;
				w  = 62;
				h  = 23;
				dx = pD->DISKx+75+unit*67;
				dy = pD->DISKy+10;
			}
		} else if( pD->devType==DEVTYPE_8 ) {
			if( CfgFlags & CFG_BIGCRT ) {
				sx = pD->DISKNAME[unit][0] ? 15 : 147;
				sy = pD->ACTIVE[unit] ? 110 : 6;
				w  = 129;
				h  = 55;
				dx = pD->DISKx+15+unit*132;
				dy = pD->DISKy+6;
			} else {
				sx = pD->DISKNAME[unit][0] ? 7 : 73;
				sy = pD->ACTIVE[unit] ? 55 : 3;
				w  = 65;
				h  = 27;
				dx = pD->DISKx+7+unit*66;
				dy = pD->DISKy+3;
			}
		} else if( pD->devType==DEVTYPE_5MB ) {
			if( CfgFlags & CFG_BIGCRT ) {
				sx = pD->DISKNAME[unit][0] ? 4 : 246;
				sy = pD->ACTIVE[unit] ? 102 : 2;
				w  = 238;
				h  = 98;
				dx = pD->DISKx+4;
				dy = pD->DISKy+2;
			} else {
				sx = pD->DISKNAME[unit][0] ? 2 : 122;
				sy = pD->ACTIVE[unit] ? 51 : 1;
				w  = 118;
				h  = 49;
				dx = pD->DISKx+2;
				dy = pD->DISKy+1;
			}
		} else {
			SystemMessage("update DrawDiskStatus() in devDISK.c");
			sx = sy = dx = dy = w = h = 0;
		}
		BltBMB(KWnds[pD->iDiskWnd].pBM, dx, dy, dBM, sx, sy, w, h, TRUE);
	}
}

///********************************************************
void ActivateDisk(int sc, int tla, int unit, BOOL activate) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		if( (pD->ACTIVE[unit] && !activate) || (!pD->ACTIVE[unit] && activate) ) {
			pD->ACTIVE[unit] = !pD->ACTIVE[unit];
			DrawDiskStatus(sc, tla, unit);
			if( !activate && pD->OUTd[unit] ) {
				DeleteDiskList(pD->iDiskWnd, unit);
				DirDisk(sc, tla, unit);
				pD->OUTd[unit] = FALSE;
			}
		}
		pD->ACTTIME[unit] = KGetTicks();
	}
}

///********************************************************
void DrawDisks(int sc, int tla) {

	int			x, y, w, i, ucnt;
	DWORD		bclr;
	DISKDATA	*pD;
	PBMB		dBM;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) && pD->iDiskWnd ) {
		dBM = *DiskParms[pD->devType].pBM;
		ucnt= DiskParms[pD->devType].units;
		// draw whole window's background
		RectBMB(KWnds[pD->iDiskWnd].pBM, 0, 0, KWnds[pD->iDiskWnd].pBM->physW, KWnds[pD->iDiskWnd].pBM->physH, MainWndClr, CLR_NADA);

		// we play games here with the divisor of the dBM->physW to convert ucnt=2 to a 1 (draw full width) and ucnt=1 to a 2 (draw only 1/2 width)
		// 3 (binary 11) ^ 2 (binary 10) = 1 (binary 01), while 3 (binary 11) ^ 1 (binary 01) = 2 (binary 10)
		BltBMB(KWnds[pD->iDiskWnd].pBM, pD->DISKx, pD->DISKy, dBM, 0, 0, dBM->physW/(3^ucnt), dBM->physH/2, TRUE);

		w = pD->nameW;
		for(i=0; i<DiskParms[pD->devType].units; i++) {
			sprintf((char*)ScratBuf, "%d%d%d", 3+sc, tla, i);
			x = pD->DISKx + dBM->physW/4 - 12 + i*dBM->physW/ucnt;
			Label85BMB(KWnds[pD->iDiskWnd].pBM, x, pD->DISKy-10, 12, ScratBuf, strlen((char*)ScratBuf), CLR_BLACK);
			y = pD->nameY[i];
			DrawDiskStatus(sc, tla, i);
			SendMsg((WND*)&pD->DWshow[i], DLG_DRAW, 0, 0, 0);
			if( pD->DISKNAME[i][0] ) {
				x = pD->nameX[i];
				if( pD->DISK[i][255] ) bclr = CLR_LIGHTRED;
				else bclr = CLR_LIGHTGREEN;
				RectBMB(KWnds[pD->iDiskWnd].pBM, x, y-2, x+w, y+9, bclr, CLR_NONE);
				sprintf((char*)ScratBuf, "[%ld] %s", pD->FOLDER[i], pD->DISKNAME[i]);
				ClipBMB(KWnds[pD->iDiskWnd].pBM, x, y, pD->nameW, 12);
				Label85BMB(KWnds[pD->iDiskWnd].pBM, x+8, y, 12, ScratBuf, strlen((char*)ScratBuf), CLR_BLACK);
				ClipBMB(KWnds[pD->iDiskWnd].pBM, -1, 0, 0, 0);
			}
		}
	}
}

///***********************************************************
/// Figures out where everything in the DISK window should be drawn.
/// 'd' in this case is the BASE unit# (in case of multiple disk windows)
void RelocateDisks(int sc, int tla, int unit) {

	int			ucnt;
	long		cw, cy;
	DISKDATA	*pD;
	PBMB		dBM;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		dBM = *DiskParms[pD->devType].pBM;
		ucnt= DiskParms[pD->devType].units;
		// we play games here with the divisor of the dBM->physW to convert ucnt=2 to a 1 (draw full width) and ucnt=1 to a 2 (draw only 1/2 width)
		// 3 (binary 11) ^ 2 (binary 10) = 1 (binary 01), while 3 (binary 11) ^ 1 (binary 01) = 2 (binary 10)
		pD->DISKx = (KWnds[pD->iDiskWnd].vw - dBM->physW/(3^ucnt))/2;
		pD->DISKy = 11+max(1, dBM->physH/25);

		// This code assumes that the two "enable auto-CAT" checkboxes are SQUARE (same W/H)
		// and uses the H for both dimensions, because the DRAW code for the checkbox MODIFIES
		// the W to be 4 wider (adjusting the width for the label width, which starts 4 pixels
		// to the right of the checkbox.  A terrible hack, but you get what you pay for...
		if( GetCheck((WND*)&pD->DWshow[0]) ) {	// drive 0 auto-cat is enabled
			if( ucnt>1 && GetCheck((WND*)&pD->DWshow[1]) ) {	// drive 1 auto-cat is enabled
				cw = KWnds[pD->iDiskWnd].vw/2-2;
				cy = pD->DISKy + dBM->physH/2 + 10 + 12;
				pD->nameW = cw - pD->DWshow[0].h;
				pD->nameX[0] = pD->DWshow[0].h -1;
				pD->catX[0] = 0;
				pD->nameY[0] = cy - 12;
				pD->nameY[1] = cy - 12;
				pD->DWshow[0].yoff = pD->nameY[0] - 3;
				pD->DWshow[1].yoff = pD->nameY[1] - 3;
				pD->catY = cy;
				pD->catH = KWnds[pD->iDiskWnd].vh-cy;
				pD->catW[0] = cw;
				pD->catW[1] = cw;
				pD->DWshow[0].xoff = 0;
				pD->DWshow[1].xoff = KWnds[pD->iDiskWnd].vw-pD->DWshow[1].h;
				pD->catX[1] = KWnds[pD->iDiskWnd].vw/2+2;
				pD->nameX[1] = pD->catX[1];
			} else {								// drive 1 auto-cat is DISabled
				cw = KWnds[pD->iDiskWnd].vw-4-3*pD->DWshow[0].h;
				cy = pD->DISKy + dBM->physH/2 + 10 + 12 + (ucnt>1?14:0);
				pD->nameW = cw;
				pD->nameX[0] = pD->DWshow[0].h -1;
				pD->catX[0] = 0;
				pD->nameY[0] = cy - 12;
				pD->nameY[1] = cy - 12 - 14;
				pD->DWshow[0].yoff = pD->nameY[0] - 3;
				pD->DWshow[1].yoff = pD->nameY[1] - 3;
				pD->catY = cy;
				pD->catH = KWnds[pD->iDiskWnd].vh-cy;
				pD->catW[0] = KWnds[pD->iDiskWnd].vw;
				pD->catW[1] = 0;
				pD->DWshow[0].xoff = 0;
				pD->DWshow[1].xoff = KWnds[pD->iDiskWnd].vw-pD->DWshow[1].h;
				pD->catX[1] = 0;
				pD->nameX[1] = pD->DWshow[1].xoff - cw;
			}
		} else {								// drive 0 auto-cat is DISabled
			if( ucnt>1 && GetCheck((WND*)&pD->DWshow[1]) ) {	// drive 1 auto-cat is enabled
				cw = KWnds[pD->iDiskWnd].vw-4-3*pD->DWshow[0].h;
				cy = pD->DISKy + dBM->physH/2 + 10 + 12 + 14;
				pD->nameW = cw;
				pD->nameX[0] = pD->DWshow[0].h -1;
				pD->catX[0] = 0;
				pD->nameY[0] = cy - 12 - 14;
				pD->nameY[1] = cy - 12;
				pD->DWshow[0].yoff = pD->nameY[0] - 3;
				pD->DWshow[1].yoff = pD->nameY[1] - 3;
				pD->catY = cy;
				pD->catH = KWnds[pD->iDiskWnd].vh-cy;
				pD->catW[0] = 0;
				pD->catW[1] = KWnds[pD->iDiskWnd].vw;
				pD->DWshow[0].xoff = 0;
				pD->DWshow[1].xoff = KWnds[pD->iDiskWnd].vw-pD->DWshow[1].h;
				pD->catX[1] = 0;
				pD->nameX[1] = pD->DWshow[1].xoff - cw;
			} else {								// drive 1 auto-cat is DISabled
				cw = KWnds[pD->iDiskWnd].vw-4-3*pD->DWshow[0].h;
				cy = pD->DISKy + dBM->physH/2 + 10 + 12 + 14;
				pD->nameW = cw;
				pD->nameX[0] = pD->DWshow[0].h -1;
				pD->catX[0] = 0;
				pD->nameY[0] = cy - 12 - 14;
				pD->nameY[1] = cy - 12;
				pD->DWshow[0].yoff = pD->nameY[0] - 3;
				pD->DWshow[1].yoff = pD->nameY[1] - 3;
				pD->catY = cy;
				pD->catH = 0;
				pD->catW[0] = 0;
				pD->catW[1] = 0;
				pD->DWshow[0].xoff = 0;
				pD->DWshow[1].xoff = KWnds[pD->iDiskWnd].vw-pD->DWshow[1].h;
				pD->catX[1] = 0;
				pD->nameX[1] = pD->DWshow[1].xoff - cw;
			}
		}
	}
}

///***********************************************************
/// assumes that RelocateDisks() has been called previously by KWnd[] create/resize code, thus the pD->cat XYWH variables have already been set.
void GetDiskListRect(int sc, int tla, int unit, RECT *r) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		r->top = pD->catY;
		r->bottom = pD->catY + pD->catH;
		r->left = pD->catX[unit];
		r->right = pD->catX[unit] + pD->catW[unit];
	}
}

///*********************************************************************
void WriteDisk(int sc, int tla, int unit) {

	long		hFile;
	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) && pD->DISKNAME[unit][0] && pD->DISK[unit]!=NULL ) {
		sprintf(FilePath, "DISKS%ld\\%s", pD->FOLDER[unit], pD->DISKNAME[unit]);
		if( -1 != (hFile=Kopen(FilePath, O_WRBINNEW)) ) {
			Kwrite(hFile, (BYTE *)(pD->DISK[unit]), DiskSize[sc][tla]);	// write to disk any changes to the floppy image
			Kclose(hFile);
		}
	}
}

///*********************************************************************
void EjectDisk(int sc, int tla, int unit) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		WriteDisk(sc, tla, unit);
		pD->DISKNAME[unit][0] = 0;
		memset(pD->DISK[unit], 0, DiskSize[sc][tla]);
		DeleteDiskList(pD->iDiskWnd, unit);
	}
}

///**************************************************************
void NewDisk(int sc, int tla, int unit, BYTE *pName) {

	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		memset(pD->DISK[unit], 0, DiskSize[sc][tla]);
		strcpy((char *)pD->DISKNAME[unit], (char*)pName);
	}
}

typedef struct {
	WORD	type;
	char	name[6];
} LIFTYPES;

// NOTE: if the last char of the file string is a '#', that indicates the file is 'secured' or 'private'.
LIFTYPES LIFtype[]={
	{0x0001,"85ASM"},	// LIF ASCII (85 ASSEMBLY SOURCE) and 71 TEXT (but preference is given to 85ASM)
	{0x00FF,"71LX-"},	// HP 71 LEX, disabled
	{0xE004,"**** "},	// 80 EXTENDED ****
	{0xE005,"****#"},
	{0xE006,"****#"},
	{0xE007,"****#"},
	{0xE008,"BPGM "},	// 80 BPGM
	{0xE009,"BPGM#"},
	{0xE00A,"BPGM#"},
	{0xE00B,"BPGM#"},
	{0xE00C,"GRAF "},	// 87 GRAF
	{0xE00D,"GRAF#"},
	{0xE00E,"GRAF#"},
	{0xE00F,"GRAF#"},
	{0xE010,"DATA "},	// 80 DATA
	{0xE011,"DATA#"},
	{0xE012,"DATA#"},
	{0xE013,"DATA#"},
	{0xE014,"87ASM"},	// 87 ASSM
	{0xE015,"87AS#"},
	{0xE016,"87AS#"},
	{0xE017,"87AS#"},
	{0xE01C,"PKEY "},	// ROOT/PKEY
	{0xE020,"PROG "},	// 80 PROG and/or 41 CALC (preference in auto-CAT is given to Series 80)
	{0xE021,"PROG#"},
	{0xE022,"PROG#"},
	{0xE023,"PROG#"},
	{0xE024,"PSET "},	// 87 PSET
	{0xE025,"PSET#"},
	{0xE026,"PSET#"},
	{0xE027,"PSET#"},
	{0xE02C,"BKUP "},	// 87 BKUP
	{0xE02D,"BKUP#"},
	{0xE02E,"BKUP#"},
	{0xE02F,"BKUP#"},
	{0xE030,"41XM "},	// HP 41 X-Memory file
	{0xE034,"FORM "},	// 87 FORM
	{0xE035,"FORM#"},
	{0xE036,"FORM#"},
	{0xE037,"FORM#"},
	{0xE040,"41WAL"},	// HP 41 Write All
	{0xE050,"41KAS"},	// HP 41 key assigments
	{0xE052,"75KAS"},	// HP 75 text and key assigments
	{0xE053,"75APT"},	// HP 75 appointments
	{0xE058,"75GEN"},	// HP 75 General file
	{0xE060,"41STS"},	// HP 41 status
	{0xE070,"41ML "},	// HP 41 X-Memory and ROM dump
	{0xE080,"41PRG"},	// HP 41 program
	{0xE084,"MIKS "},	// 87 MIKSAM
	{0xE085,"MIKS#"},
	{0xE086,"MIKS#"},
	{0xE087,"MIKS#"},
	{0xE088,"75BAS"},	//  75BAS HP 75 Basic
	{0xE089,"75LEX"},	//  75LEX HP 75 LEX
	{0xE08A,"75WKS"},	//  75WKS HP 75 Visicalc worksheet
	{0xE08B,"75ROM"},	//  75DMP HP 75 ROM dump
	{0xE0B0,"41BUF"},	//  41BUF HP 41 Buffer file
	{0xE0D0,"x1DAT"},	//  x1DAT HP 41 data, HP 71 stream data
	{0xE0D1,"TEXT#"},	//  TEXT#, secured
	{0xE0F0,"71DAT"},	//  71DAT HP 71 data
	{0xE0F1,"71DA#"},	//  71DA# HP 71 data, secured
	{0xE204,"71BIN"},	//  71BIN HP 71 binary program
	{0xE205,"71BN#"},	//  71BN# binary program secured
	{0xE206,"71BN#"},	//  71BN# binary program private
	{0xE207,"71BN#"},	//  71BN# binary program secured private
	{0xE208,"71LEX"},	//  71LEX HP 71 language extension
	{0xE209,"71LX#"},	//  71LX# HP 71 language extension, secured
	{0xE20A,"71LX#"},	//  71LX# HP 71 language extension, private
	{0xE20B,"71LX#"},	//  71LX# HP 71 language extension, secured, private
	{0xE20C,"71KEY"},	//  71KEY HP 71 key definition
	{0xE20D,"71KE#"},	//  71KE# HP 71 key definition, secured
	{0xE214,"71BAS"},	//  71BAS HP 71 Basic
	{0xE215,"71BA#"},	//  71BA# HP 71 Basic, secured
	{0xE216,"71BA#"},	//  71BA# HP 71 Basic, private
	{0xE217,"71BA#"},	//  71BA# HP 71 Basic, secured, private
	{0xE218,"714TH"},	//  714TH HP 71 Forth
	{0xE219,"714T#"},	//  714T# HP 71 Forth, secured
	{0xE21A,"714T#"},	//  714T# HP 71 Forth, private
	{0xE21B,"714T#"},	//  714T# HP 71 Forth, secured, private
	{0xE21C,"71ROM"},	//  71ROM HP 71 ROM
	{0xE222,"71GRF"},	//	71GRF HP 71 Graphics
	{0xE223,"71GR#"},	//	71GRF HP 71 Graphics secured
	{0xE224,"71ADR"},	//	71GRF HP 71 Address file
	{0xE225,"71AD#"},	//	71GRF HP 71 Address file secured
	{0xE22C,"71OBJ"},	//	71OBJ HP 71 Object file
	{0xE22D,"71OB#"},	//	71OB# HP 71 Object file secured
	{0xE22E,"71SYM"},	//	71SYM HP 71 Symbol file
	{0xE22F,"71SY#"},	//	71SYM HP 71 Symbol file secured
	{0xE808,"9HPLD"},	//  9HPLD HP 9000 HPL data
	{0xE810,"9HLPP"},	//  9HLPP HP 9000 HPL program
	{0xE814,"9HPLx"},	//  9HPLx HP 9000 HPL?
	{0xE818,"9HPLK"},	//  9HPLK HP 9000 HPL Keys
	{0xE820,"9HPLy"},	//  9HPLy HP 9000 HPL?
	{0xE942,"9SYS "},	//  9SYS  HP 9000 System
	{0xE94B,"UNIX "},	//  UNIX  HP Unix
	{0xE950,"9BASC"},	//  9BASC HP 9000 Basic Program
	{0xE961,"9BDAT"},	//  9BDAT HP 9000 BDAT
	{0xE971,"9BIN "},	//  9BIN  HP 9000 Binary
	{0xEA0A,"9PDAT"},	//  9PDAT HP 9000 Pascal Data
	{0xEA32,"9PCOD"},	//  9PCOD HP 9000 Pascal Code
	{0xEA3E,"9PTXT"},	//  9PTXT HP 9000 Pascal Text
};

///*************************************************************
void DirDisk(int sc, int tla, int unit) {

	BYTE		*dname, *ddata;
	WORD		stype, slensects, sbytesrec;//, sbegsect, slenbytes;
	BYTE		*pD;
	int			i, j, nd, tcnt, used, tsects;
	DISKDATA	*pDD;

	if( NULL!=(pDD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		dname = pDD->DISKNAME[unit];
		ddata = pDD->DISK[unit];
		if( dname[0] ) {
			if( *ddata==0x80 ) {
	// scan LIF diskette directory for LIF file name
				used = 2;	// 2 volume sectors
				if( Model==0 && pDD->devType==DEVTYPE_5MB ) {
					AddDiskList(pDD->iDiskWnd, unit, " ");
					AddDiskList(pDD->iDiskWnd, unit, "===WARNING: NOT HP85A COMPATIBLE===");
					AddDiskList(pDD->iDiskWnd, unit, " ");
				}
				sprintf((char*)ScratBuf, "[ Volume ]: %6.6s", ddata+2);
				AddDiskList(pDD->iDiskWnd, unit, (char*)ScratBuf);
				AddDiskList(pDD->iDiskWnd, unit, "Name      \tType\tBytes\tRecs");

				// get # dir entries
				nd = 8*(long)((((WORD)ddata[18])<<8)+((WORD)ddata[19]));
				used += nd/8;	// + #directory sectors
				// get ptr to first directory entry
				pD = ddata + 256*((((WORD)ddata[10])<<8)+((WORD)ddata[11]));
				for(i=0; i<nd; i++, pD+=32) {
					stype = *((WORD *)(pD+10));
					stype = ((stype>>8)&0x00FF) | ((stype&0x00FF)<<8);
					if( stype==0xFFFF ) {	// reached NEXT-AVAIL-FILE
						break;
					}
					// if not PURGED
//					sbegsect = (((WORD)(*(pD+14)))<<8) + (WORD)(*(pD+15));
					slensects = (((WORD)(*(pD+18)))<<8) + (WORD)(*(pD+19));
					used += slensects;
//					slenbytes = *((WORD *)(pD+28));
					sbytesrec = *((WORD *)(pD+30));
					if( stype ) {
						if( sbytesrec==0 ) sbytesrec = 256;
						tcnt = sizeof(LIFtype)/sizeof(LIFTYPES);
						for(j=0; j<tcnt; j++) if( LIFtype[j].type==stype ) break;
						if( j<tcnt ) sprintf((char*)ScratBuf, "%10.10s\t%5.5s\t%d\t%d", pD, LIFtype[j].name, sbytesrec, (256*slensects)/sbytesrec);
						else sprintf((char*)ScratBuf, "%10.10s\t-->%4.4X\t%d\t%d", pD, stype, sbytesrec, (256*slensects)/sbytesrec);
					} else {
						sprintf((char*)ScratBuf, "----[[[PURGED]]]----\t%d", slensects);
					}
					AddDiskList(pDD->iDiskWnd, unit, (char*)ScratBuf);
				}
			} else {
					AddDiskList(pDD->iDiskWnd, unit, "[Bad or uninitialized disk]");
					used = 0;
			}
			tsects = DiskParms[pDD->devType].cylinders*DiskParms[pDD->devType].cylsects;
			sprintf((char*)ScratBuf, " ==((%d/%d - %d%% used))==", used, tsects, 100*used/tsects);
			AddDiskList(pDD->iDiskWnd, unit, (char*)ScratBuf);
		}
	}
}

///**************************************************************
BOOL LoadDisk(int sc, int tla, int unit, BYTE *pName) {

	long		hFile;
	DISKDATA	*pD;

	if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
		if( pName[0] ) {
			sprintf(FilePath, "DISKS%ld\\%s", pD->FOLDER[unit], pName);

			if( -1 == (hFile=Kopen(FilePath, O_RDBIN)) ) return FALSE;

			Kread(hFile, (BYTE *)(pD->DISK[unit]), DiskSize[sc][tla]);
			Kclose(hFile);
			strcpy((char*)pD->DISKNAME[unit], (char*)pName);

			DeleteDiskList(pD->iDiskWnd, unit);
			DirDisk(sc, tla, unit);
		}
	}
	return TRUE;
}

///**************************************************************
int devDISK(long reason, int sc, int tla, int d, int val) {

	int			u, i, dt, state;//, sa;
	DWORD		n;
	DISKDATA	*pD;
	BYTE		tmp[4];

	switch( reason ) {
	 case DP_INI_READ:
	 	/// sc & tla are valid, val==index into Devices[]
	 	/// At DP_INI_READ-time, Devices[val].devID has been set to the "devXX=" number (XX) in the .ini file.
	 	/// That .devID value may very well change between DP_INI_READ and DP_INI_WRITE!
		TLAset(sc, tla, &devDISK);	// Tell cardHPIB what function to call for this SC/TLA address
		DiskType[sc][tla] = dt = Devices[val].devType;	// save for all later GetDevData() calls
		DiskSize[sc][tla] = 256 * DiskParms[dt].cylinders * DiskParms[dt].cylsects;
		// allocate our memory storage
		CreateDevData(sc, tla, sizeof(DISKDATA), DiskType[sc][tla]);
		// and initialize it
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			sprintf((char*)ScratBuf, "%s%d", iniSTR[INIS_DEV], Devices[val].devID);
			// set default values of no .ini file
			pD->DiskFlags = DISKF_AUTOCAT0 | DISKF_AUTOCAT1;
			pD->iDiskWndW = 475;
			pD->iDiskWndH = KWnds[0].h;
			pD->iDiskWndX = (KWnds[0].x>400)? (KWnds[0].x-pD->iDiskWndW+2*WINDOW_FRAMEX) : (KWnds[0].x+KWnds[0].w);
			pD->iDiskWndY = KWnds[0].y+WINDOW_TITLEY;
			pD->DISKNAME[0][0] = pD->DISKNAME[1][0] = 0;
			pD->FOLDER[0] = pD->FOLDER[1] = 0;
			// if no .ini file existed, then the Devices[val].state will = DEVSTATE_NEW
			if( Devices[val].state==DEVSTATE_BOOT && iniFindSec(iniSTR[INIS_EMULATOR]) && iniFindVar(ScratBuf) ) {
				if( iniGetStr(ScratBuf, 256)>0 ) {	// get (skip) device name
					iniGetNum(&n);	// skip sc
					iniGetNum(&n);	// skip ca

					iniGetNum(&pD->DiskFlags);
					iniGetNum((DWORD*)&pD->FOLDER[0]);
					iniGetStr(pD->DISKNAME[0], sizeof(pD->DISKNAME[0]));
					if( DiskParms[dt].units>1 ) {
						iniGetNum((DWORD*)&pD->FOLDER[1]);
						iniGetStr(pD->DISKNAME[1], sizeof(pD->DISKNAME[1]));
					}
					iniGetNum((DWORD*)&pD->iDiskWndX);
					iniGetNum((DWORD*)&pD->iDiskWndY);
					iniGetNum((DWORD*)&pD->iDiskWndW);
					iniGetNum((DWORD*)&pD->iDiskWndH);
					iniGetNum((DWORD*)&pD->iDiskWndMisc);
					pD->iDiskWndY += WINDOW_TITLEY;
				}
			}
			break;
		}
		SystemMessage("Failed to create disk drive");
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
	 	state = Devices[val].state;
		iniSetSec(iniSTR[INIS_EMULATOR]);
		sprintf((char*)ScratBuf, "%s%d", iniSTR[INIS_DEV], Devices[val].devID);
		if( state==DEVSTATE_BOOT && NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			pD->DiskFlags &= ~(DISKF_AUTOCAT0 | DISKF_AUTOCAT1);
			if( GetCheck((WND*)&pD->DWshow[0]) ) pD->DiskFlags |= DISKF_AUTOCAT0;
			if( GetCheck((WND*)&pD->DWshow[1]) ) pD->DiskFlags |= DISKF_AUTOCAT1;
			sprintf((char*)ScratBuf+32, "\"%s\" %d %d 0x%8.8lX", (char*)iniSTR[INIS_DISK525+pD->devType], Devices[val].wsc+3, Devices[val].wca,	pD->DiskFlags);
			for(i=0; i<DiskParms[pD->devType].units; i++) sprintf((char*)ScratBuf+32 + strlen((char*)ScratBuf+32), " %ld \"%s\"", pD->FOLDER[i], pD->DISKNAME[i]);
			sprintf((char*)ScratBuf+32 + strlen((char*)ScratBuf+32), " %ld %ld %ld %ld %ld", KWnds[pD->iDiskWnd].x, KWnds[pD->iDiskWnd].y-(DlgNoCap?WINDOW_TITLEY:0), KWnds[pD->iDiskWnd].w, KWnds[pD->iDiskWnd].h, KWnds[pD->iDiskWnd].misc);
			iniSetVar(ScratBuf, ScratBuf+32);
		} else if( state==DEVSTATE_NEW ) {
			sprintf((char*)ScratBuf+32, "\"%s\" %d %d 0x%8.8X", (char*)iniSTR[INIS_DISK525+Devices[val].devType], Devices[val].wsc+3, Devices[val].wca,	DISKF_AUTOCAT0 | DISKF_AUTOCAT1);
			for(i=0; i<DiskParms[Devices[val].devType].units; i++) strcat((char*)ScratBuf+32, " 0 \"\"");
			sprintf((char*)ScratBuf+32 + strlen((char*)ScratBuf+32), " %ld %ld %d %ld %ld",
				((KWnds[0].x>474)? (KWnds[0].x-475) : (KWnds[0].x+KWnds[0].w)),// + val*WINDOW_FRAMEX,
				KWnds[0].y+WINDOW_TITLEY + val*WINDOW_TITLEY,
				475,
				KWnds[0].h,
				KWnds[0].misc);
			iniSetVar(ScratBuf, ScratBuf+32);
		}
	 	break;
	 case DP_FLUSH:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			for(i=0; i<DiskParms[pD->devType].units; i++) WriteDisk(sc, tla, i);
		}
		break;
	 case DP_RESET:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			pD->DSJ = 0;
			pD->ACTIVE[0] = pD->ACTIVE[1] = FALSE;
		}
	 	break;
	 case DP_STARTUP:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			// Allocate disk memory and load disks
			for(i=0; i<DiskParms[pD->devType].units; i++) {
				pD->DISK[i] = (BYTE *)malloc(DiskSize[sc][tla]);
				memset(pD->DISK[i], 0, DiskSize[sc][tla]);
			}

			switch( DiskType[sc][tla] ) {
			 case DEVTYPE_5_25:
				DiskParms[pD->devType].pBM = &disk525BM;
				break;
			 case DEVTYPE_3_5:
				DiskParms[pD->devType].pBM = &disk35BM;
				break;
			 case DEVTYPE_8:
				DiskParms[pD->devType].pBM = &disk8BM;
				break;
			 case DEVTYPE_5MB:
				DiskParms[pD->devType].pBM = &disk5MBBM;
				break;
			 default:
				SystemMessage("Set DiskParms[].pBM in devDISK.c @ DP_STARTUP");
				break;
			}
			// Create DISK DRIVE
			sprintf((char*)ScratBuf+6000, "Series 80 %s", iniSTR[INIS_DISK525+pD->devType]);
			pD->iDiskWnd = KCreateWnd(pD->iDiskWndX, pD->iDiskWndY, pD->iDiskWndW, pD->iDiskWndH, (char*)ScratBuf+6000, &DiskWndProc, FALSE);
			((DISKETC*)KWnds[pD->iDiskWnd].etc)->sc = sc;
			((DISKETC*)KWnds[pD->iDiskWnd].etc)->tla= tla;

			KGetWindowPos(KWnds[pD->iDiskWnd].hWnd, &KWnds[pD->iDiskWnd].x, &KWnds[pD->iDiskWnd].y, &KWnds[pD->iDiskWnd].w, &KWnds[pD->iDiskWnd].h, &KWnds[pD->iDiskWnd].misc);
			if( pD->iDiskWnd>0 ) {
				for(i=0; i<DiskParms[pD->devType].units; i++) {
					memcpy(&pD->DWshow[i], &DWshowINIT, sizeof(DWshowINIT));
					pD->DWshow[i].id += i;
					pD->DWshow[i].yoff += i*20;
					SendMsg((WND *)&pD->DWshow[i], DLG_CREATE, (DWORD)NULL/*parent*/, (DWORD)NULL/*prev sib*/, (DWORD)&KWnds[pD->iDiskWnd]);
					if( pD->DiskFlags & (DISKF_AUTOCAT0<<i) ) pD->DWshow[i].flags |= CF_CHECKED;
				}
				ShowWindow(KWnds[pD->iDiskWnd].hWnd, SW_SHOW);
			}
			for(i=0; i<DiskParms[pD->devType].units; i++) LoadDisk(sc, tla, i, pD->DISKNAME[i]);
			if( sc<MenuDiskSC || (sc==MenuDiskSC && tla<MenuDiskTLA) ) {
				MenuDiskSC = sc;
				MenuDiskTLA = tla;
			}
		}
	 	break;
	 case DP_SHUTDOWN:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			for(i=0; i<DiskParms[pD->devType].units; i++) {
				free(pD->DISK[i]);		// free the disk memory
				pD->DISK[i] = NULL;
			}
			KDestroyWnd(pD->iDiskWnd);
		}
	 	DestroyDevData(sc, tla);
	 	break;
	 case DP_INPUT_DONE:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) && InBurst(sc) ) {	// need to terminate BURST IO with interrupt
#if DEBUG
	sprintf((char*)ScratBuf+1000, "-->TermBURST IOcIE=%d IOpI=%d", IOcardsIntEnabled, (int)IO_PendingInt);
	WriteDebug(ScratBuf+1000);
#endif // DEBUG
			HPIBint(sc, BURSTint, 2);	// generate end-of-burst interrupt to Series 80 CPU
#if DEBUG
		sprintf((char*)ScratBuf+1024,"Term BURST IO(%d) LastIOPcmd=0x%2.2X GINT=%d IOcardsIntEnabled=%d IO_PendingInt=%d Int85=%d", sc, GetLastIOPcmd(sc), GINTen(), IOcardsIntEnabled, (int)IO_PendingInt, Int85);
		WriteDebug(ScratBuf+1024);
#endif // DEBUG
			SetBurst(sc, 0, 256);

			// bump sector#
			if( ++pD->HEDPOS.sector >= DiskParms[pD->devType].cylsects/DiskParms[pD->devType].heads ) {
				pD->HEDPOS.sector = 0;
				if( ++pD->HEDPOS.head >= DiskParms[pD->devType].heads ) {
					pD->HEDPOS.head = 0;
					if( ++pD->HEDPOS.cyllo==0 ) ++pD->HEDPOS.cylhi;
				}
			}
			pD->ACTTIME[pD->HEDPOS.unit] = KGetTicks();
		}
		break;
	 case DP_NEWCMD:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			switch( val & 0xF0 ) {
			 case 0x20:	// BURST I/O
				IOcardsIntEnabled = TRUE;
				ActiveBurstTLA = tla;
				if( val==0x20 ) {	// BURST OUT
					if( UnitOK(sc, tla) ) {
						if( !pD->ACTIVE[pD->HEDPOS.unit] ) ActivateDisk(sc, tla, pD->HEDPOS.unit, TRUE);
						if( 256*pD->HEDPOS.cylhi + pD->HEDPOS.cyllo<DiskParms[pD->devType].cylinders && pD->HEDPOS.sector<DiskParms[pD->devType].cylsects/DiskParms[pD->devType].heads && pD->HEDPOS.head<DiskParms[pD->devType].heads ) {
							SetOBptr(sc, pD->DISK[pD->HEDPOS.unit] + ((256*pD->HEDPOS.cylhi+pD->HEDPOS.cyllo)*DiskParms[pD->devType].cylsects + pD->HEDPOS.head*DiskParms[pD->devType].cylsects/DiskParms[pD->devType].heads + pD->HEDPOS.sector)*256);
							pD->OUTd[pD->HEDPOS.unit] = TRUE;
						}
					}
				} else if( val==0x21 ) {	// BURST IN
					if( UnitOK(sc, tla) ) {
						if( !pD->ACTIVE[pD->HEDPOS.unit] ) ActivateDisk(sc, tla, pD->HEDPOS.unit, TRUE);
						if( 256*pD->HEDPOS.cylhi + pD->HEDPOS.cyllo<DiskParms[pD->devType].cylinders && pD->HEDPOS.sector<DiskParms[pD->devType].cylsects/DiskParms[pD->devType].heads && pD->HEDPOS.head<DiskParms[pD->devType].heads ) {
							StartBurstIn(sc, pD->DISK[pD->HEDPOS.unit] + ((256*pD->HEDPOS.cylhi+pD->HEDPOS.cyllo)*DiskParms[pD->devType].cylsects + pD->HEDPOS.head*DiskParms[pD->devType].cylsects/DiskParms[pD->devType].heads + pD->HEDPOS.sector)*256, 256);
						}
					}
				} else {
					sprintf((char*)ScratBuf, "Unknown BURST IO CMD: 0x%2.2X", (unsigned int)val);
					MessageBox(NULL, (char*)ScratBuf, "Got some more work to do!", MB_OK | MB_ICONEXCLAMATION);
				}
				SetBurst(sc, val & 0x7F, 256);
				break;
			 case 0xA0:	// OUTPUT
				SetOBptr(sc, NULL);	// flag awaiting command
				SetHPIBstate(sc, IOSTATE_IDLE);
				break;
			}
		}
	 	break;
	 case DP_OUTPUT:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			if( GetOBptr(sc)==NULL ) {
				pD->CMD = val;
				switch( (val & 0x1F) ) {
				 case 0x00:	//
				 case 0x02:	// SEEK command
					SetOBptr(sc, (BYTE *)(&pD->HEDPOS));
					break;
				 case 0x03:	// REQUEST STATUS command
					SetOBptr(sc, (BYTE *)(&pD->UNIT));
					break;
				 case 0x05:	// READ command (followed by unit#)
					SetOBptr(sc, (BYTE *)(&pD->HEDPOS));
					break;
				 case 0x07:	// VERIFY command
					SetOBptr(sc, (BYTE *)(&pD->VERIFY));
					break;
				 case 0x08:	// WRITE command (followed by unit#)
					SetOBptr(sc, (BYTE *)(&pD->HEDPOS));
					break;
				 case 0x0B:	// INITIALIZE (followed by unit#)
					SetOBptr(sc, (BYTE *)(&pD->UNIT));
					break;
				 case 0x14:	// REQUEST DISC ADDRESS command (followed by dummy byte)
					pD->DA[0] = tla;
					SetOBptr(sc, (BYTE *)(&pD->DA[0]));
					break;
				 case 0x18:	// FORMAT (initialize disk)
					SetOBptr(sc, (BYTE *)(&pD->FORMAT));
					break;
				 default:
					sprintf((char*)ScratBuf, "Unknown OUTPUT to SC %d DRIVE 00 = 0x%2.2X", sc+3, val);
					MessageBox(NULL, (char*)ScratBuf, "Oops", MB_OK);
					break;
				}
			} else {
				PushOB(sc, (BYTE)val);
				if( IsCED(sc) ) {
					pD->DSJ = 0;
					switch( (pD->CMD & 0x1F) ) {
					 case 0x02:	// SEEK command
						break;
					 case 0x03:	// REQUEST STATUS command
	setwp:
						if( UnitOK(sc, tla) ) pD->STATUSwp = *(pD->DISK[pD->UNIT]+255) ? 0100 : 0;	// set write-protect flag
						break;
					 case 0x05:	// READ command (followed by unit#)
						pD->STATUStype = 0;
						pD->UNIT = pD->HEDPOS.unit;
	setstatus:
						if( UnitOK(sc, tla) ) {
							pD->DSJ = (*(pD->DISK[pD->UNIT])==0x80 && pD->DISKNAME[pD->UNIT][0]) ? 0 : 1;
							pD->STATUSwp = *(pD->DISK[pD->UNIT]+255) ? 0100 : 0;	// set write-protect flag
						}
						break;
					 case 0x07:	// VERIFY command
						pD->STATUStype = 0;
						pD->UNIT = pD->VERIFY.unit;
						goto setstatus;
					 case 0x08:	// WRITE command (followed by unit#)
						pD->STATUStype = 0;
						pD->UNIT = pD->HEDPOS.unit;
						if( UnitOK(sc, tla) ) {
							pD->STATUSwp = *(pD->DISK[pD->UNIT]+255) ? 0100 : 0;	// set write-protect flag
							pD->DSJ = (*(pD->DISK[pD->UNIT])==0x80 && pD->DISKNAME[pD->UNIT][0] && !pD->STATUSwp) ? 0 : 1;
						}
						break;
					 case 0x14:	// REQUEST DISC ADDRESS command (followed by dummy byte)
						// use whichever DEV_UNIT was last accessed
						goto setstatus;
					 case 0x18:	// FORMAT (initialize disk)
						pD->STATUStype = 0;
						u = pD->FORMAT.unit;
						if( UnitOK(sc, tla) && pD->DISKNAME[u][0] && !*(pD->DISK[u]+255)) {
							memset(pD->DISK[u], pD->FORMAT.bytedata, DiskSize[sc][tla]);
							pD->DISK[u][255] = 0;
							pD->OUTd[u] = TRUE;
						} else {
							pD->STATUStype = 023;
							pD->DSJ = 1;
							break;
						}
						goto setwp;
					 default:
						break;
					}
					SetOBptr(sc, NULL);
				}
			}
		}
	 	break;
	 case DP_TERMBURST:	// terminate burst output
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			HPIBint(sc, BURSTint, 2);	// end of BURST out interrupt
			SetBurst(sc, 0, 256);	// disable BURST mode
			// bump sector#
			if( ++pD->HEDPOS.sector >= DiskParms[pD->devType].cylsects/DiskParms[pD->devType].heads ) {
				pD->HEDPOS.sector = 0;
				if( ++pD->HEDPOS.head >= DiskParms[pD->devType].heads ) {
					pD->HEDPOS.head = 0;
					if( ++pD->HEDPOS.cyllo==0 ) ++pD->HEDPOS.cylhi;
				}
			}
			pD->ACTTIME[pD->HEDPOS.unit] = KGetTicks();
		}
		break;
	 case DP_HPIB_CMD:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
//			sa = val & 0x1F;	// get SECONDARY address (in case it's appropriate)
			if( d==1 ) {	// IDENTIFY
/// Return: (heads,cyls,secs/cyl/total secs are all DECIMAL numbers)
///
/// From the 85A MS ROM source listing (thus what the 85A supports):
///												HEADS	CYLS	SECS/CYL	TOTAL_SECS
///		x01,x04 mini-floppy (82901/9121)		2		33		32			1056
///		x00,x81 8" (9134A/9895A)				2		75		60			4320
///		x01,x05 quad mini						4		68		32			2176
///
///	From the 85B MS ROM source listing (thus what the 85B supports):
///												HEADS	CYLS	SECS/CYL	TOTAL_SECS
///		x01,x04 mini-floppy (82901/9121)		2		33		32			1056
///		x00,x81 8" (9134A/9895A)				2		75		60			4320
///		x01,x06 5 MB hard disk (9135A OPT 10)	4		152		124			18848
///		x01,x0A 10 MB hard disk (9134B)			4		305		124			37820
///
///	From the 87 MS ROM source listing (thus what the 87 supports):
///												HEADS	CYLS	SECS/CYL	TOTAL_SECS
///		x01,x04 mini-floppy (82901/9121)		2		33		32			1056
///		x00,x81 8" (9134A/9895A)				2		75		60			4320
///		x01,x06 5 MB hard disk (9135A OPT 10)	4		153		124			18972
///
/// NOTES:
///		1) The 85A supports an ID of 1,5 (quad mini) that the other two do not.
///		2) The 85B adds support for a 5MB and 10MB hard disk.
///		3) The 87 drops support for the 10MB disk and increases the 5MB disk's cylinder count by 1.
///


///GEEKDO: NOTE: the use of 'sa' here is WRONG!  It needs to be FIXED!!!!!

				if( TRUE /* (sc,tla) is devDISK*/ ) {	// DISK ID
					HPIBinput(sc, DiskParms[pD->devType].id, 2);
				} else SetPED(sc);
			} else {	// not IDENTIFY
				switch( val ) {	// parity was stripped before it was sent here
				 case 0x04:	// SDC - Selected Device Clear (returns all listening devices to device-defined state)
					break;
				 case 0x05:	// PPC - Parallel Poll Configure - Causes the devices being addressed as listeners to be configured to a Parallel Poll response specified by the following secondary (secondary codes 96-111) or to disable Parallel Poll response (secondary code 112)
					break;
				 case 0x08:	// GET - Initiates a pre-programmed, device dependent action simultaneously for any device which has been previously addressed to listen (e.g. start with measuring)
					break;
				 case 0x14:	// DCL - Device CLear (returns ALL devices to device-defined state)
					break;
				 case 0x18:	// SPE - Serial Poll Enable
					break;
				 case 0x19:	// SPD - Serial Poll Disable
					break;
				 case 0x60:	// SECONDARY GROUP COMMAND P11, DEVICE 0
				 case 0x61:
				 case 0x62:
				 case 0x63:
				 case 0x64:
				 case 0x65:
				 case 0x66:
				 case 0x67:
				 	SetPED(sc);
		//						MessageBox(NULL, "Implement ATNCMD 0x60!", "Oops", MB_OK);
					break;
				 case 0x68:	// secondary device commands
					if( IsListener(sc, tla) ) {
						// sending more arguments

					} else if( IsTalker(sc, tla) ) {
						if( (pD->CMD & 0x1F)==0x03 ) {	// REQUEST STATUS
							tmp[0]= pD->STATUStype;	// STATUS bytes
							tmp[1]= 0x00;
							tmp[2]= 0x00;
							tmp[3]= pD->STATUSwp;
							HPIBinput(sc, tmp, 4);
						} else if( (pD->CMD & 0x1F)==0x14 ) {	// REQUEST DISC ADDRESS
							// returns (for last unit addressed) returns (CYLINDER BYTE1, CYLINDER BYTE2, HEAD, SECTOR)
							//	pD->HEDPOS.cylhi;
							//	pD->HEDPOS.cyllo;
							//	pD->HEDPOS.head;
							//	pD->HEDPOS.sector;

		//GEEKDO: Are these the RIGHT values that we should be returning????
							n = DiskParms[pD->devType].cylinders;	// cylinder past end of disk
							tmp[0]= (n>>8) & 0x00FF;	// cylhi of end-of-disk
							tmp[1]= (n & 0xFF);			// cyllo of end-of-disk
							tmp[2]= 0;					// head of end-of-disk
							tmp[3]= 0;					// sector of end-of-disk
							HPIBinput(sc, tmp, 4);
						} else MessageBox(NULL, "unexpected 0x68 ATNCMD", "Oops", MB_OK);
					} else MessageBox(NULL, "No one listening or talking at 0x68/0xE8 ATNCMD", "Oops", MB_OK);
					break;
				 case 0x69:	// SECONDARY GROUP COMMAND TALK?
		//						MessageBox(NULL, "Implement ATNCMD 0x69!", "Oops", MB_OK);
					break;

				 case 0x6A:	// SECONDARY GROUP COMMAND LISTEN?
					//MessageBox(NULL, "Implement ATNCMD 0x6A!", "Oops", MB_OK);
					break;
				 case 0x6B:	//
					break;
				 case 0x6C:	// SECONDARY LISTEN
					break;
				 case 0x70:	// return DSJ byte?
				 	tmp[0] = pD->DSJ;
				 	HPIBinput(sc, tmp, 1);
					break;
				 default:
					sprintf((char*)ScratBuf, "Unknown HP-IB data: [%d:%d] %d : %2.2X", sc, tla, GetHPIBstate(sc), (unsigned int)val);
					MessageBox(NULL, (char*)ScratBuf, "Got some more work to do!", MB_OK | MB_ICONEXCLAMATION);
					break;
				}
				break;
			}
		}
		break;
	 case DP_TIMER:
		if( NULL!=(pD=GetDevData(sc, tla, DiskType[sc][tla])) ) {
			for(i=0; i<DiskParms[pD->devType].units; i++) if( pD->ACTIVE[i]  &&  (DWORD)val > pD->ACTTIME[i]+1000 ) {
#if DEBUG
		sprintf((char*)ScratBuf,"devDISK(DP_TIMER, %d, %d, %u) IOcIE=%d IOpI=%d", sc, tla, val, IOcardsIntEnabled, (int)IO_PendingInt);
		WriteDebug(ScratBuf);
#endif // DEBUG
				ActivateDisk(sc, tla, i, FALSE);
			}
		}
		break;
	 case DP_DRAW:
		DrawDisks(sc, tla);
		break;
	 case DP_PPOLL:
	 	val = 1;	// always READY :-)
		break;
	}
	return val;
}

