/*******************************************************
  DLG.C - dialog file for HP-85 emulator.
  Copyright 2006-2017 Everett Kaser
  All rights reserved
 See DOCS\README.TXT for legal usage.

WARNING!!! This program is designed and compiled as a 32-bit program!!!
Pointers to objects are converted to DWORDs and vice-versa.  If you
port this to a 64-bit or greater architecture, changes will have to be
made!!!

 ********************************************************/

#include	"main.h"
#include	"mach85.h"

#include	<stdio.h>
#include	<ctype.h>
#include	"string.h"

//********************************************************************************************************
// Globals

DLG *ActiveDlg=NULL;
WND	*wndFocus=NULL;

BOOL	DlgNoCap=FALSE;
int		LastType=0;					//
BYTE	ScratBuf[8192];				// scratch buffer
BYTE	ClipBoard[256];				// for Ctl-C, Ctl-X, and Ctl-V in EDIT controls
char	TapeNames[MAX_TAPES][64];	// names for RadioButtons in TapeDlg
char	RenTapeOldBuf[128];
char	DiskNames[MAX_DISKS][64];	// names for RadioButtons in DiskDlg

BOOL	AltDown=FALSE;

DWORD	EVDinput;

WORD	TapeDoubleID;
WORD	DiskDoubleID;

long	DiskSC, DiskTLA, DiskDlgDrive, DiskDevType, DiskDlgFolder;// whether unit 0 or 1, and which DISKS sub-folder
BYTE	*pDiskName;					// ptr to DEV_00_DISKNAME or DEV_01_DISKNAME

DWORD ROMctlID[MAXROMS]={
	ID_ROMD_GRAPHICS, ID_ROMD_PD, ID_ROMD_MIKSAM, ID_ROMD_LANG, ID_ROMD_EXT, ID_ROMD_ASM, ID_ROMD_STRUCT,
	ID_ROMD_FOR, ID_ROMD_MAT, ID_ROMD_MAT2, ID_ROMD_IO, ID_ROMD_EMS, ID_ROMD_MS, ID_ROMD_ED,
	ID_ROMD_SERV, ID_ROMD_AP2, ID_ROMD_AP, ID_ROMD_PP};

DWORD ROMD_Model, ROMD_RomLoads[3], ROMD_RamSizes[3], ROMD_RamSizeExt[3];

///*********************************************************************************
WND *FindWndID(DWORD id) {

	WND*pWnd;

	pWnd = (WND *)ActiveDlg;
	if( pWnd->id==id ) return pWnd;
	pWnd = pWnd->child1;
	while( pWnd && pWnd->id!=id ) pWnd = pWnd->nextsib;
	return pWnd;
}

///*********************************************************************
DWORD SendMsg(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	if( pWnd!=NULL && pWnd->wndproc!=NULL ) return (*pWnd->wndproc)(pWnd, msg, arg1, arg2, arg3);
	else return 0;
}

///*********************************************************************
DWORD SendMsgID(DWORD id, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	return SendMsg(FindWndID(id), msg, arg1, arg2, arg3);
}

///*********************************************************************
void NotifyChildren(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	WND	*pC;

	for(pC=pWnd->child1; pC != NULL; pC=pC->nextsib) SendMsg(pC, msg, arg1, arg2, arg3);
}

///************************************
void SetCheck(WND *pWnd, DWORD checked) {

	if( pWnd ) {
		if( pWnd->flags & CF_CHECKED ) {
			if( checked ) return;
		} else {
			if( !checked ) return;
		}
		SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
	}
}

///***********************************************************************
void SetCheckID(DWORD id, DWORD checked) {

	SetCheck(FindWndID(id), checked);
}

///************************************
DWORD GetCheck(WND *pWnd) {

	if( pWnd ) return pWnd->flags & CF_CHECKED;
	return 0;
}

///***********************************************************************
DWORD GetCheckID(DWORD id) {

	return GetCheck(FindWndID(id));
}

///*********************************************************************
void ChangeFocus(WND *pWnd) {

	WND	*pF;

	pF = wndFocus;
	wndFocus = NULL;
	if( pWnd != pF ) SendMsg(pF, DLG_KILLFOCUS, 0, 0, 0);
	if( pWnd==NULL ) pWnd = ActiveDlg->child1;
	while( pWnd->flags & WF_NOFOCUS ) {
		pWnd = pWnd->nextsib;
		if( pWnd==NULL ) pWnd = ActiveDlg->child1;
	}
	wndFocus = pWnd;
	SendMsg(wndFocus, DLG_SETFOCUS, 0, 0, 0);

	ActiveDlg->wndLCfocus = wndFocus;
}

///***********************************************************************
void CreateDlg(DLG *pDlg, void **pCTLs, long numctls, BOOL drawflag, KWND *kWnd) {

	WND		*pW, *pF;
	long	i;

	ActiveDlg = pDlg;
	pF = NULL;
	ActiveDlg->child1 = pW = (WND *)SendMsg((WND *)pCTLs[0], DLG_CREATE, (DWORD)ActiveDlg, (DWORD)NULL, (DWORD)kWnd);
	for(i=1; i<numctls; i++) {
		pW->nextsib = (WND *)SendMsg((WND *)pCTLs[i], DLG_CREATE, (DWORD)ActiveDlg, (DWORD)pW, (DWORD)kWnd);
		pW = pW->nextsib;
		if( pW->flags & CF_FOCUS ) pF = pW;
	}
	if( pF==NULL ) pF = ActiveDlg->child1;
	wndFocus = NULL;
	ChangeFocus(pF);

	if( drawflag ) SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
void GetWndXY(WND *pW, long *pX, long *pY) {

	*pX = pW->xoff;
	*pY = pW->yoff;
	for(pW=pW->parent; pW != NULL; pW=pW->parent) {
		*pX += pW->xoff;
		*pY += pW->yoff;
	}
}

///*************************************
WND *FindPtWnd(long x, long y) {

	long	x1, y1;
	WND		*pW;

	for(pW=ActiveDlg->child1; pW; pW=pW->nextsib) {
		GetWndXY(pW, &x1, &y1);
		if( (x>=x1 && x<x1+pW->w && y>=y1 && y<y1+pW->h)
		 && !(pW->flags & WF_NOFOCUS)
		 &&  (pW->flags & CF_TABSTOP) ) return pW;
	}
	return NULL;
}

///*********************************************************************
void DrawOutBox(PBMB hBM, long x1, long y1, long x2, long y2, DWORD clrhi, DWORD clrlo) {

	LineBMB(hBM, x1, y2, x1, y1, clrhi);
	LineBMB(hBM, x1, y1, x2, y1, clrhi);
	LineBMB(hBM, x2, y1+1, x2, y2, clrlo);
	LineBMB(hBM, x2, y2, x1+1, y2, clrlo);
}

///***********************************************************************
void DrawCtlText(PBMB hBM, long x1, long y1, long x2, long y2, BYTE *str, BYTE *underchar, DWORD fclr, DWORD bclr, long align) {

	long	fw, fh, w, h, lx, ly, len;

	len = strlen((char*)str);
	fw = 8*len-3;
	fh = 7;
	w = x2-x1;
	h = y2-y1;

	switch( align & CTL_TEST_UPDOWN ) {
	 case CTL_IS_TOP:
		break;
	 case CTL_IS_BOTTOM:
		y1 = y2-fh;
		break;
	 case CTL_IS_MIDDLE:
		y1 += (h-fh)/2;
		break;
	}
	switch( align & CTL_TEST_LEFTRIGHT ) {
	 case CTL_IS_LEFT:
		break;
	 case CTL_IS_RIGHT:
		x1 = x2-fw;
		break;
	 case CTL_IS_CENTER:
		x1 += (w-fw)/2;
		break;
	}
	RectBMB(hBM, x1, y1, x1+8*len, y1+10, bclr, CLR_NADA);
	Label85BMB(hBM, x1, y1, 12, str, len, fclr);

	if( underchar != NULL ) {	// if shortcut to be underlined
		lx = x1 + 8*(underchar-str);
		ly = y1 + 9;
		LineBMB(hBM, lx, ly, lx+5, ly, fclr);
	}
}

///***********************************************************************
WORD DrawHotText(PBMB hBM, long x1, long y1, long x2, long y2, BYTE *pText, DWORD clr, DWORD align, DWORD flags) {

	BYTE		*s, *d, *under, h;

	s = pText;	// copy text label, stripping '&' and setting ALT-shortcut key
	d = ScratBuf;
	h = 0;
	under = NULL;
	while( *s ) {
		if( *s=='&' ) {
			if( *++s!='&' ) {
				if( !*s ) break;	// just in case some idiot ends the text with an '&'
				under = d;
				h = toupper(*s);
			}
		}
		*d++ = *s++;
	}
	*d = 0;

	DrawCtlText(hBM, x1, y1, x2, y2, ScratBuf, under, clr, CLR_NADA, align);
	if( flags & CF_DISABLED) {
		--x1;
		--y1;
		--x2;
		--y2;
		DrawCtlText(hBM, x1, y1, x2, y2, ScratBuf, under, CLR_GRAYTEXT2, CLR_NADA, align);
	}
	return h;
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD DefWndProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_LBUTTDOWN:
		if( !(pWnd->flags & CF_DISABLED) ) {
			if( wndFocus!=pWnd && !(pWnd->flags & WF_NOFOCUS) ) ChangeFocus(pWnd);
		}
		return 0;
	 default:
		break;
	}
	return 0;
}

///***********************************************************************
DWORD DefCtlProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_CREATE:
		pWnd->parent = (WND *)arg1;
		pWnd->prevsib = (WND *)arg2;
		pWnd->nextsib = pWnd->child1 = (WND *)NULL;
		pWnd->kWnd = (KWND *)arg3;
		return (DWORD)pWnd;
	 case DLG_SETFOCUS:
	 case DLG_KILLFOCUS:
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		break;
	 case DLG_KEYDOWN:
		return 1;	// return not handled, "do it yourself"
	 default:
		break;
	}
	return DefWndProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD DefDialogProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	long	x1, y1, x2, y2;
	DLG 	*pC;
	WND		*pW, *pF, *pN;

	pC = (DLG *)pWnd;
	switch( msg ) {
	 case DLG_KEYDOWN:
		switch( arg1 ) {
		 case VK_MENU:
			AltDown = TRUE;
			return 0;
		 case VK_RETURN:
			if( NULL!=wndFocus && wndFocus->type==CTL_BUTT ) {
				pF = wndFocus;
DDPok:
				SendMsg(pF, WM_ACTIVATE, 0, 0, 0);
				return 0;
			}
			pF = pWnd->child1;
			while( pF ) {
				if( pF->flags & CF_DEFAULT ) goto DDPok;
				pF = pF->nextsib;
			}
			return 0;
		 case VK_TAB:
			if( ShiftDown ) {	// shift down
				if( NULL==wndFocus ) break;
				x1 = wndFocus->type;

				pF = wndFocus->prevsib;	// move to previous control
				if( x1 == CTL_RADIO ) {	// if we're on a RADIO, search for beginning of group
					while( pF && !(pF->flags & CF_GROUP) ) pF = pF->prevsib;
					if( pF ) pF = pF->prevsib;
				}
				do {
					while( pF && pF!=wndFocus ) {	// search backwards through entire dialog until wraps back around to current control
						if( pF->flags & CF_TABSTOP ) {	// for a control with CF_TABSTOP set
							if( !(pF->flags & CF_DISABLED) ) {
								if( pF->type==CTL_RADIO ) {
									pN = pF;
									while( pN && !(pN->flags & (CF_GROUP | CF_CHECKED)) ) pN = pN->prevsib;
									if( pN ) pF = pN;
								}
DDPfocus:
								ChangeFocus(pF);
								if( pF->type==CTL_RADIO ) SendMsg(pF, WM_ACTIVATE, 0, 0, 0);
								return 0;
							}
						}
						pF = pF->prevsib;
					}
					if( pF!=wndFocus ) {
						// find end of dialog ctls (wrap around from beginning to end)
						pF = pWnd->child1;
						while( pF->nextsib ) pF = pF->nextsib;
					}
				} while( pF!=wndFocus );
			} else {
				if( NULL==wndFocus ) break;
				pF = wndFocus->nextsib;
				do {
					while( pF && pF!=wndFocus ) {
						if( pF->flags & CF_TABSTOP ) {
							// if we didn't start on a radio button
							//	or we aren't currently on a radio button
							//	or we've reached the beginning of the next GROUP
							// AND the one we've reached is NOT disabled
							if( wndFocus->type != CTL_RADIO
							  || pF->type != CTL_RADIO
							  || (pF->flags & CF_GROUP) ) {
								pN = pF;
								while( !(pF->flags & CF_DISABLED) ) {
									if( pF->type != CTL_RADIO || (pF->flags & CF_CHECKED) ) goto DDPfocus;
									if( NULL==(pF=pF->nextsib) ) {
										pF = pN;
										goto DDPfocus;
									}
								}
								pF = pN;
							}
						}
						pF = pF->nextsib;
					}
					if( pF!=wndFocus ) pF = pWnd->child1;
				} while( pF!=wndFocus );
			}
			return 0;
		 case VK_ESCAPE:
			ActiveDlg = NULL;	// cancel dialog
			DrawAll();
			return 0;
		 case VK_RIGHT:
		 case VK_DOWN:
			if( SendMsg(wndFocus, msg, arg1, arg2, arg3) ) {	// give CTL first shot at these
				if( NULL==(pF=wndFocus) ) break;
				while( (pN=pF->nextsib) ) {
					if( pN->flags & CF_GROUP ) break;
					pF = pN;
					if( (pF->flags & (CF_TABSTOP | CF_DISABLED | WF_NOFOCUS))==CF_TABSTOP ) goto DDPfocus;
				}
				while( !(pF->flags & CF_GROUP) ) {	// backup to start of group
					if( !(pN=pF->prevsib) ) break;
					pF = pN;
				}
				while( (pF->flags & (CF_TABSTOP | CF_DISABLED | WF_NOFOCUS))!=CF_TABSTOP ) {
					if( !(pN=pF->nextsib) ) goto DDPfocus;
					pF = pN;
				}
				goto DDPfocus;
			}
			return 0;
		 case VK_LEFT:
		 case VK_UP:
			if( SendMsg(wndFocus, msg, arg1, arg2, arg3) ) {	// give CTL first shot at these
				if( NULL==(pF=wndFocus) ) break;
				if( !(pF->flags & CF_GROUP) ) {	// if not at start of group
					while( (pN=pF->prevsib) ) {	// search upwards for a tabstop
						pF = pN;
						if( (pF->flags & (CF_TABSTOP | CF_DISABLED | WF_NOFOCUS))==CF_TABSTOP ) goto DDPfocus;
					}
				}
				while( (pN=pF->nextsib) ) {
					if( pN->flags & CF_GROUP ) {
						while( (pF->flags & (CF_TABSTOP | CF_DISABLED | WF_NOFOCUS))!=CF_TABSTOP ) {
							if( !(pN=pF->prevsib) ) break;
							pF = pN;
						}
						break;
					}
					pF = pN;
				}
				while( (pF->flags & CF_DISABLED) && (pN=pF->prevsib) ) {
					pF = pN;
				}
				goto DDPfocus;
			}
			return 0;
		 default:
			break;
		}
		return SendMsg(wndFocus, msg, arg1, arg2, arg3);
	 case DLG_KEYUP:
		switch( arg1 ) {
		 case VK_MENU:
			AltDown = FALSE;
			return 0;
		 default:
			break;
		}
		return SendMsg(wndFocus, msg, arg1, arg2, arg3);
	 case DLG_DRAW:
		x1 = pWnd->xoff;
		y1 = pWnd->yoff;
		x2 = x1+pWnd->w;
		y2 = y1+pWnd->h;
		RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, CLR_DLGBACK, CLR_BLACK);
		DrawOutBox(pWnd->kWnd->pBM, x1+1, y1+1, x2-2, y2-2, CLR_DLGHI, CLR_DLGLO);
		NotifyChildren(pWnd, msg, arg1, arg2, arg3);
		break;
	 case DLG_TIMER:
		NotifyChildren(pWnd, msg, arg1, arg2, arg3);
		break;
	 case DLG_CHAR:
		return SendMsg(wndFocus, msg, arg1, arg2, arg3);
	 case DLG_LBUTTDOWN:
	 case DLG_LBUTTUP:
		if( NULL!=(pW=FindPtWnd(arg1, arg2)) ) {
			SendMsg(pW, msg, arg1, arg2, arg3);
		} else {
			ChangeFocus(pC->wndLCfocus);
		}
		break;
	 case DLG_PTRMOVE:
		return SendMsg(wndFocus, msg, arg1, arg2, arg3);
	 default:
		break;
	}
	return 0;
}

///*********************************************************************************
DWORD RadioButtonProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	WND		*pF;
	RADIO	*pC;
	DWORD	bclr, fclr;
	long	x1, x2, y1, y2, slop, ex, ey;
	BYTE	*s, *d, *under;

	pC = (RADIO *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		GetWndXY(pWnd, &ex, &ey);

		y2 = 0;
		slop = 5;	// slop is 1/2 width of diamond
		x1 = ex+slop;
		y1 = ey+y2;	// (x1,y1) point to top of diamond
		LineBMB(pWnd->kWnd->pBM, x1, y1, x1+slop, y1+slop, CLR_DLGHI);
		LineBMB(pWnd->kWnd->pBM, x1+slop, y1+slop, x1, y1+2*slop, CLR_DLGHI);
		LineBMB(pWnd->kWnd->pBM, x1, y1, x1-slop, y1+slop, CLR_DLGLO);
		LineBMB(pWnd->kWnd->pBM, x1-slop, y1+slop, x1, y1+2*slop, CLR_DLGLO);
		++y1;
		--slop;
		LineBMB(pWnd->kWnd->pBM, x1, y1, x1+slop, y1+slop, CLR_DLGBACK);
		LineBMB(pWnd->kWnd->pBM, x1+slop, y1+slop, x1, y1+2*slop, CLR_DLGBACK);
		LineBMB(pWnd->kWnd->pBM, x1, y1, x1-slop, y1+slop, CLR_BLACK);
		LineBMB(pWnd->kWnd->pBM, x1-slop, y1+slop, x1, y1+2*slop, CLR_BLACK);

		bclr = (pWnd->flags & CF_CHECKED) ? CLR_EDITTEXT : CLR_EDITBACK;
		for(; ++y1 && --slop; ) {
			LineBMB(pWnd->kWnd->pBM, x1, y1, x1+slop, y1+slop, bclr);
			LineBMB(pWnd->kWnd->pBM, x1+slop, y1+slop, x1, y1+2*slop, bclr);
			LineBMB(pWnd->kWnd->pBM, x1, y1+2*slop, x1-slop, y1+slop, bclr);
			LineBMB(pWnd->kWnd->pBM, x1-slop, y1+slop, x1, y1, bclr);
		}
		PointBMB(pWnd->kWnd->pBM, x1, y1, bclr);

		s = (BYTE*)(pC->pText);	// copy text label, stripping '&' and setting ALT-shortcut key
		d = ScratBuf;
		pC->altchar = 0;
		under = NULL;
		while( *s ) {
			if( *s=='&' ) {
				if( *++s!='&' ) {
					if( !*s ) break;	// just in case some idiot ends the text with an '&'
					under = d;
					pC->altchar = toupper(*s);
				}
			}
			*d++ = *s++;
		}
		*d = 0;

		pWnd->w = 8*strlen((char*)ScratBuf)+2*8;

		x1 = ex + 2*8;
		y1 = ey;
		x2 = ex + pWnd->w;
		y2 = ey+pWnd->h;
		RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, (pWnd==wndFocus) ? CLR_SELBACK : CLR_DLGBACK, CLR_NADA);
		y1 += (pWnd->h-9)/2;
		fclr = (pWnd->flags & CF_DISABLED) ? CLR_GRAYTEXT1 : CLR_DLGTEXT;
		if( pWnd==wndFocus ) fclr = CLR_SELTEXT;
		DrawCtlText(pWnd->kWnd->pBM, x1, y1, x2, y2, ScratBuf, under, fclr, CLR_NADA, CTL_TOP_LEFT);
		if( pWnd->flags & CF_DISABLED) {
			--x1;
			--y1;
			DrawCtlText(pWnd->kWnd->pBM, x1, y1, x2, y2, ScratBuf, under, CLR_GRAYTEXT2, CLR_NADA, CTL_TOP_LEFT);
		}
		return 0;
	 case DLG_ACTIVATE:
		if( pWnd->flags & CF_CHECKED ) return 0;	// if already selected
		// Deselect all radio buttons in the group
		// First, find start of group
		for(pF=pWnd; !(pF->flags & CF_GROUP) && pF->prevsib; pF=pF->prevsib) ;
		// Second, clear all CF_CHECKED flags in group
		do {
			if( pF!=pWnd && (pF->flags & CF_CHECKED) ) {
				pF->flags &= ~CF_CHECKED;
				SendMsg(pF, DLG_DRAW, 0, 0, 0);
			}
			pF = pF->nextsib;
		} while( pF && !(pF->flags & CF_GROUP) && pF->type==CTL_RADIO );
		// Third, set CF_CHECKED flag on this RadioButton
		pWnd->flags |= CF_CHECKED;	// select this one
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		return 0;
	 case DLG_LBUTTUP:
		if( !(pWnd->flags & CF_DISABLED) ) SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
		break;
	}
	return DefCtlProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD MatrixProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	WND		*pC;
	DWORD	f, ret;

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	if( ROMD_Model==2 ) {	// on the HP-87, both MATRIX ROMs must be added or removed together
			ret = CheckProc(pWnd, msg, arg1, arg2, arg3);
			f = pWnd->flags & CF_CHECKED;
			pC = FindWndID((pWnd->id==ID_ROMD_MAT)?ID_ROMD_MAT2:ID_ROMD_MAT);
			pC->flags &= ~CF_CHECKED;
			pC->flags |= f;
			SendMsg(pC, DLG_DRAW, 0, 0, 0);
			return ret;
	 	}
		break;
	}
	return CheckProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD APProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	WND		*pC;
	DWORD	f, ret;

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	if( ROMD_Model==2 ) {	// on the HP-87, both AP ROMs must be added or removed together
			ret = CheckProc(pWnd, msg, arg1, arg2, arg3);
			f = pWnd->flags & CF_CHECKED;
			pC = FindWndID((pWnd->id==ID_ROMD_AP)?ID_ROMD_AP2:ID_ROMD_AP);
			pC->flags &= ~CF_CHECKED;
			pC->flags |= f;
			SendMsg(pC, DLG_DRAW, 0, 0, 0);
			return ret;
	 	}
		break;
	}
	return CheckProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD CheckProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	CHECK	*pC;
	DWORD	fclr;
	long	x1, x2, y1, y2, slop, ex, ey;
	BYTE	*s, *d, *under;

	pC = (CHECK *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		GetWndXY(pWnd, &ex, &ey);

		y2 = 0;
		slop = 10;
		x1 = ex;
		y1 = ey+(pWnd->h-slop)/2;
		x2 = x1+slop-1;
		y2 = y1+slop-1;
		RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, CLR_EDITBACK, CLR_NADA);
		DrawOutBox(pWnd->kWnd->pBM, x1, y1, x2, y2, CLR_DLGLO, CLR_DLGHI);

		if( pWnd->flags & CF_CHECKED ) {
			fclr = (pWnd->flags & CF_DISABLED) ? CLR_GRAYTEXT2 : CLR_EDITTEXT;
			slop = pWnd->h/14+2;
			x1 += slop;
			y1 += slop;
			x2 -= slop-1;
			y2 -= slop-1;
			for(slop=pWnd->h/10; slop; --slop) {
				LineBMB(pWnd->kWnd->pBM, x1+slop, y1, x2, y2-slop, fclr);
				LineBMB(pWnd->kWnd->pBM, x1, y1+slop, x2-slop, y2, fclr);
			}
			LineBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, fclr);
			for(slop=pWnd->h/10; slop; --slop) {
				LineBMB(pWnd->kWnd->pBM, x2-slop, y1, x1, y2-slop, fclr);
				LineBMB(pWnd->kWnd->pBM, x2, y1+slop, x1+slop, y2, fclr);
			}
			LineBMB(pWnd->kWnd->pBM, x2, y1, x1, y2, fclr);
		}
		s = (BYTE*)pC->pText;	// copy text label, stripping '&' and setting ALT-shortcut key
		d = ScratBuf;
		pC->altchar = 0;
		under = NULL;
		while( *s ) {
			if( *s=='&' ) {
				if( *++s!='&' ) {
					if( !*s ) break;	// just in case some idiot ends the text with an '&'
					under = d;
					pC->altchar = toupper(*s);
				}
			}
			*d++ = *s++;
		}
		*d = 0;

		pWnd->w = 8*strlen((char*)ScratBuf)+2*8;

		x1 = ex + 2*8;
		y1 = ey;
		x2 = ex + pWnd->w;
		y2 = ey+pWnd->h;
		RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, (pWnd==wndFocus) ? CLR_SELBACK : CLR_DLGBACK, CLR_NADA);
		y1 += (pWnd->h-9)/2 + 1;
		fclr = (pWnd->flags & CF_DISABLED) ? CLR_GRAYTEXT1 : CLR_DLGTEXT;
		if( pWnd==wndFocus ) fclr = CLR_SELTEXT;
		DrawCtlText(pWnd->kWnd->pBM, x1, y1, x2, y2, ScratBuf, under, fclr, CLR_NADA, CTL_TOP_LEFT);
		if( pWnd->flags & CF_DISABLED) {
			--x1;
			--y1;
			DrawCtlText(pWnd->kWnd->pBM, x1, y1, x2, y2, ScratBuf, under, CLR_GRAYTEXT2, CLR_NADA, CTL_TOP_LEFT);
		}
		return 0;
	 case DLG_ACTIVATE:
		pWnd->flags ^= CF_CHECKED;	// toggle check
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		return 0;
	 case DLG_LBUTTUP:
		if( !(pWnd->flags & CF_DISABLED) ) SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
		break;
	 case DLG_CHAR:
		if( arg1==' ' ) return SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
		break;
	}
	return DefCtlProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD ButtonProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	long	x1, y1, x2, y2;
	DWORD	bclr, fclr;
	BUTT	*pW;

	pW = (BUTT *)pWnd;
	switch( msg ) {
	 case DLG_CHAR:
		if( arg1==' ' ) return SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
		break;
	 case DLG_LBUTTDOWN:
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		return 0;
	 case DLG_LBUTTUP:
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		return SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
	 case DLG_DRAW:
		GetWndXY(pWnd, &x1, &y1);
		x2 = x1+pW->w;
		y2 = y1+pW->h;
		bclr = (wndFocus==pWnd)?CLR_SELBACK:CLR_DLGBACK;
		if( pW->flags & CF_DEFAULT ) {
			RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, bclr, CLR_BLACK);
			++x1;
			++y1;
		} else RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, bclr, CLR_NADA);
		--x2;
		--y2;
		LineBMB(pWnd->kWnd->pBM, x1, y2, x2, y2, CLR_BLACK);
		LineBMB(pWnd->kWnd->pBM, x2, y2, x2, y1, CLR_BLACK);
		--x2;
		--y2;

		if( pW->flags & CF_DOWN ) {	// invert box color if button down
			bclr = CLR_DLGHI;
			fclr = CLR_DLGLO;
		} else {
			bclr = CLR_DLGLO;
			fclr = CLR_DLGHI;
		}
		DrawOutBox(pWnd->kWnd->pBM, x1, y1, x2, y2, fclr, bclr);	// draw button outline

		if( pW->pText ) {
			++x1;
			++y1;
			if( pW->flags & CF_DOWN ) {	// move text up-left 1 pixel if button down
				++x1;
				++y1;
				++x2;
				++y2;
			}
			fclr = (pW->flags & CF_DISABLED)?CLR_GRAYTEXT1:CLR_DLGTEXT;
			if( pWnd==wndFocus ) fclr = CLR_SELTEXT;

			pW->altchar = DrawHotText(pWnd->kWnd->pBM, x1, y1, x2, y2, (BYTE*)pW->pText, fclr, CTL_MIDDLE_CENTER, pW->flags);
		}
		break;
	 default:
		break;
	}
	return DefCtlProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD TextProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	TEXT	*pW;
	long	x, y, align;

	pW = (TEXT *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		GetWndXY(pWnd, &x, &y);

		align = (pWnd->flags & CF_CENTER)? CTL_TOP_CENTER : ((pWnd->flags & CF_RIGHT) ? CTL_TOP_RIGHT : CTL_TOP_LEFT);
		pW->altchar = DrawHotText(pWnd->kWnd->pBM, x, y, x+pW->w, y+pW->h, (BYTE*)pW->pText, CLR_DLGTEXT, align, pWnd->flags);
		return 0;
	 case DLG_SETFOCUS:
	 case DLG_LBUTTDOWN:
		// TEXT CTL never has CF_TABSTOP (it has BETTER NOT!), so move to next CF_TABSTOP CTL using same code as GroupBox
		return GroupboxProc(pWnd, msg, arg1, arg2, arg3);
	 default:
		break;
	}
	return DefCtlProc(pWnd, msg, arg1, arg2, arg3);
}

///***************************************************************************
DWORD GroupboxProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	GROUPBOX	*pC;
	long	x1, x2, y1, y2, ex, ey;

	pC = (GROUPBOX *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		GetWndXY(pWnd, &ex, &ey);

		x1 = ex+1;
		y1 = ey+1+(pC->pText ? 4 : 0);
		x2 = ex + pC->w;
		y2 = ey + pC->h;
		RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, CLR_NADA, CLR_DLGHI);
		--x1;
		--x2;
		--y1;
		--y2;
		RectBMB(pWnd->kWnd->pBM, x1, y1, x2, y2, CLR_NADA, CLR_DLGLO);
		PointBMB(pWnd->kWnd->pBM, x1, y2, CLR_DLGHI);
		PointBMB(pWnd->kWnd->pBM, x2, y1, CLR_DLGHI);

		if( pC->pText ) DrawCtlText(pWnd->kWnd->pBM, x1+8, y1-4, x2, y2, (BYTE*)pC->pText, NULL, CLR_DLGTEXT, CLR_DLGBACK, CTL_TOP_LEFT);

		return 0;
	 case DLG_ACTIVATE:
	 case DLG_LBUTTUP:
		return 0;
	 case DLG_SETFOCUS:
	 case DLG_LBUTTDOWN:
		for(pWnd=pWnd->nextsib; pWnd; pWnd=pWnd->nextsib) {
			if( pWnd->flags & CF_TABSTOP ) {
				ChangeFocus(pWnd);
				return 0;
			}
		}
		for(pWnd=ActiveDlg->child1; TRUE ; pWnd=pWnd->nextsib) {
			if( pWnd->flags & CF_TABSTOP ) {
				ChangeFocus(pWnd);
				return 0;
			}
		}
		return 0;
	}
	return DefCtlProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
void SetEditLineView(WND *pWnd) {

	long		x1, w, x, y;
	BYTE		c;
	EDIT		*pC;

	pC = (EDIT *)pWnd;

	GetWndXY(pWnd, &x, &y);
	++x;

	strcpy((char*)ScratBuf, pC->Text);

	x1 = x-1+4;

	if( pC->caret < pC->winS ) pC->winS = pC->caret;
	if( pC->caret > pC->winS ) {
		c = ScratBuf[pC->caret];
		ScratBuf[pC->caret] = 0;
		while( (x+pC->w-2-x1-1) < (long)(8*strlen((char*)ScratBuf+pC->winS)) ) ++pC->winS;
		ScratBuf[pC->caret] = c;
	}
	while( pC->winS ) {
		w = 8*strlen((char*)ScratBuf+pC->winS-1);
		if( w > (x+pC->w-2-x1-1)  ) break;
		--pC->winS;
	}
}

///***********************************************************************
long GetClosestCaret(WND *pWnd, long x) {

	EDIT		*pC;
	long		x1, i, md, mc, d, ex, ey;
	BYTE		c;

	pC = (EDIT *)pWnd;

	GetWndXY(pWnd, &ex, &ey);

	++ex;

	if( x < ex ) return pC->winS;

	strcpy((char*)ScratBuf, pC->Text);
	x1 = ex-1+4;
	for(i=mc=pC->winS, md=32000; TRUE; ) {
		d = x - x1;
		if( d < 0 ) d = -d;
		if( d > md ) break;
		md = d;
		mc = i;
		if( ScratBuf[i] == 0 ) break;
		c = ScratBuf[i+1];
		ScratBuf[i+1] = 0;
		x1 += 8*strlen((char*)ScratBuf+i);
		ScratBuf[++i] = c;
		if( x1 >= ex+pC->w-2 ) break;
	}
	return mc;
}

///***********************************************************************
void EditLineTypeChar(WND *pWnd, BYTE c) {

	EDIT		*pC;
	long		ss, se;

	pC = (EDIT *)pWnd;
	if( strlen(pC->Text) < pC->textlim  ||  pC->selS != pC->selE ) {
		if( pC->selS != pC->selE ) {
			if( (ss=pC->selS) > (se=pC->selE) ) {
				ss = se;
				se = pC->selS;
			}
			strcpy(pC->Text + ss, pC->Text + se);
			pC->caret = ss;
			pC->selS = pC->selE = 0;
		}
		memmove(pC->Text + pC->caret+1, pC->Text + pC->caret, strlen(pC->Text + pC->caret)+1);
		pC->Text[pC->caret] = c;
		++pC->caret;
		SetEditLineView(pWnd);
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
	}
}

///***********************************************************************
DWORD EditProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pC;
	long		x1, y1, ss, se, ex, ey, lbdn;
	DWORD		bclr, fclr;
	BYTE		c;

	pC = (EDIT *)pWnd;
	lbdn = FALSE;
	switch( msg ) {
	 case DLG_CREATE:
		pC->buflen = 256;
		pC->Text[0] = 0;
		pC->textlim = 255;
		pC->altchar = 0;
		pC->winS = pC->selS = pC->selE = pC->caret = pC->shiftcaret = 0;
		break;
	 case DLG_SETTEXT:	// set text buffer contents to (char *)arg1 (re-allocate buffer to make it bigger if necessary)
		x1 = strlen((char *)arg1);
		strcpy(pC->Text, (char *)arg1);
		pC->winS = pC->selS = pC->selE = pC->shiftcaret = pC->caret = 0;
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		return 0;
	 case DLG_DRAW:
		GetWndXY(pWnd, &ex, &ey);
		ClipBMB(pWnd->kWnd->pBM, ex, ey, pC->w, pC->h);
		bclr = CLR_EDITBACK;
		fclr = CLR_EDITTEXT;

		RectBMB(pWnd->kWnd->pBM, ex, ey, ex+pC->w, ey+pC->h, bclr, fclr);
		++ex;
		++ey;

		strcpy((char*)ScratBuf, pC->Text);

		x1 = ex - 1 + 4;
		y1 = ey+(pC->h-2-7)/2;
		if( pC->selS != pC->selE ) {
			if( pC->selS < pC->selE ) {
				ss = pC->selS;
				se = pC->selE;
			} else {
				ss = pC->selE;
				se = pC->selS;
			}
			if( ss < (long)pC->winS ) ss = pC->winS;
			if( se < ss ) se = ss;
			if( ss!=se ) {
				if( ss>(long)pC->winS ) {
					c = ScratBuf[ss];
					ScratBuf[ss] = 0;
					DrawCtlText(pWnd->kWnd->pBM, x1, y1, ex+pC->w-2, ey+pC->h-2, ScratBuf+pC->winS, NULL, fclr, CLR_NADA, CTL_TOP_LEFT);
					x1 += 8*strlen((char*)ScratBuf+pC->winS);
					ScratBuf[ss] = c;
				}
				c = ScratBuf[se];
				ScratBuf[se] = 0;
				DrawCtlText(pWnd->kWnd->pBM, x1, y1, ex+pC->w-2, ey+pC->h-2, ScratBuf+ss, NULL, CLR_SELTEXT, CLR_SELBACK, CTL_TOP_LEFT);
				x1 += 8*strlen((char*)ScratBuf+ss);
				ScratBuf[se] = c;
				if( c ) DrawCtlText(pWnd->kWnd->pBM, x1, y1, ex+pC->w-2, ey+pC->h-2, ScratBuf+se, NULL, fclr, CLR_NADA, CTL_TOP_LEFT);
			} else DrawCtlText(pWnd->kWnd->pBM, x1, y1, ex+pC->w-2, ey+pC->h-2, ScratBuf+pC->winS, NULL, fclr, CLR_NADA, CTL_TOP_LEFT);
		} else DrawCtlText(pWnd->kWnd->pBM, x1, y1, ex+pC->w-2, ey+pC->h-2, ScratBuf+pC->winS, NULL, fclr, CLR_NADA, CTL_TOP_LEFT);

		if( pC->cursorOn ) {
			c = ScratBuf[pC->caret];
			ScratBuf[pC->caret] = 0;
			ex += 8*strlen((char*)ScratBuf+pC->winS);
			ScratBuf[pC->caret] = c;
			ey += 1;
			LineBMB(pWnd->kWnd->pBM, ex, ey, ex, ey+pC->h-5, CLR_EDITCURSOR);
		}

		ClipBMB(pWnd->kWnd->pBM, -1,-1,-1,-1);
		return 0;
	 case DLG_CHAR:
		if( arg1 >= ' ' ) {
			EditLineTypeChar(pWnd, (BYTE)arg1);
		}
		return 0;
	 case DLG_SETFOCUS:
		pC->lastBlinkTime = KGetTicks();
		pC->cursorOn = TRUE;
		DefCtlProc(pWnd, msg, arg1, arg2, arg3);
		pC->selE = pC->caret = strlen(pC->Text);
		pC->selS = 0;
		SetEditLineView(pWnd);
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
		return 0;
	 case DLG_TIMER:
		if( wndFocus==pWnd ) {
			if( pC->cursorOn ) {
				if( arg1 >= pC->lastBlinkTime + 350 ) {
					pC->cursorOn = FALSE;
					pC->lastBlinkTime = arg1;
					SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
				}
			} else {
				if( arg1 >= pC->lastBlinkTime + 150 ) {
					pC->cursorOn = TRUE;
					pC->lastBlinkTime = arg1;
					SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
				}
			}
		}
		return 0;
	 case DLG_KILLFOCUS:
		pC->cursorOn = FALSE;
		pC->selS = pC->selE = pC->caret = pC->winS = 0;
		break;
	 case DLG_KEYDOWN:
		switch( arg1 ) {
		 case VK_DOWN:
		 case VK_RIGHT:
			if( pC->selS==pC->selE ) pC->selS = pC->selE = pC->caret;
			else if( !ShiftDown ) pC->caret = max(pC->selS, pC->selE);
			if( pC->Text[pC->caret] ) ++pC->caret;
			goto ELPlfrt;
		 case VK_UP:
		 case VK_LEFT:
			if( pC->selS==pC->selE ) pC->selS = pC->selE = pC->caret;
			else if( !ShiftDown ) pC->caret = min(pC->selS, pC->selE);
			if( pC->caret ) --pC->caret;
ELPlfrt:
			if( !ShiftDown && !lbdn ) pC->selS = pC->selE = 0;
			else pC->selE = pC->caret;
			SetEditLineView(pWnd);
			SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
			return 0;
		 case VK_HOME:
			if( pC->selS==pC->selE ) pC->selS = pC->selE = pC->caret;
			pC->caret = 0;
			goto ELPlfrt;
		 case VK_END:
			if( pC->selS==pC->selE ) pC->selS = pC->selE = pC->caret;
			pC->caret = strlen(pC->Text);
			goto ELPlfrt;
		 case VK_BACK:
			if( pC->selS==pC->selE ) {
				if( !pC->caret ) return 0;
				--pC->caret;
			}

			// @@@@@ FALLS THROUGH!!!! @@@@@

		 case VK_DELETE:
ELPdel:
			if( pC->selS==pC->selE ) {
				if( !pC->Text[pC->caret] ) return 0;
				ss = pC->caret;
				se = pC->caret+1;
			} else if( pC->selS < pC->selE ) {
				ss = pC->selS;
				se = pC->selE;
			} else {
				ss = pC->selE;
				se = pC->selS;
			}
			strcpy(pC->Text + ss, pC->Text + se);
			goto ELPlfrt;
		 case 'X':
			if( CtlDown ) {
				SendMsg(pWnd, DLG_KEYDOWN, 'C', arg2, arg3);	// copy to clipboard first
				goto ELPdel;
			}
			return 0;
		 case 'V':
			if( CtlDown ) {
				if( ClipBoard[0] ) {
					for(ss=0; (c=ClipBoard[ss]); ss++) {
						if( c >= ' ' ) EditLineTypeChar(pWnd, c);
					}
				}
			}
			return 0;
		 case 'C':
			if( CtlDown ) {
				if( pC->selS < pC->selE ) {
					ss = pC->selS;
					se = pC->selE;
				} else {
					ss = pC->selE;
					se = pC->selS;
				}
				if( ss==se ) ++se;
				for(x1=0; ss<se; ) ClipBoard[x1++] = pC->Text[ss++];
				ClipBoard[x1] = 0;
			}
			return 0;
		}
		return 0;
	 case DLG_LBUTTDOWN:
		DefWndProc(pWnd, msg, arg1, arg2, arg3);	// so we get focus if we didn't have it
		pC->caret = pC->selS = pC->selE = GetClosestCaret(pWnd, arg1);
		lbdn = TRUE;
		goto ELPlfrt;
	 case DLG_PTRMOVE:
		if( arg3 & MK_LBUTTON ) {
			pC->caret = GetClosestCaret(pWnd, arg1);
			lbdn = TRUE;
			goto ELPlfrt;
		}
		return 0;
	}
	return DefCtlProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD CEDcloseProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD CEDdelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pE;
	DISASMREC	*pRec;
	long		r;
	DWORD		addr;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pE = (EDIT *)FindWndID(ID_CED_ADDREDIT);
		if( base==8 ) r = sscanf(pE->Text, "%o", (unsigned int *)(&addr));
		else if( base==10 ) r = sscanf(pE->Text, "%d", (int*)(&addr));
		else if( base==16 ) r = sscanf(pE->Text, "0x%x", (unsigned int *)(&addr));
		else r = 0;
		if( r ) {
			addr = (addr<<16) | 0xFFFF;
			pRec = FindRecAddress(addr, ROM[060000]);
			if( pRec!=NULL ) DeleteRec(pRec);
		}
		ActiveDlg = NULL;
		DrawAll();
		DownCursor();
		if( CurRec ) {
			if( CurRec->type!=REC_REM ) CreateCodeEditDlg();
			else CreateCommentDlg();
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD CEDsaveProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pE;
	RADIO		*pR;
	long		r;
	DWORD		addr, datover;
	BYTE		typ;
	DISASMREC	*ptmp;
	char		temp[256], lab[32];

	switch( msg ) {
	 case DLG_ACTIVATE:
		// save DISASMREC, destroy dialog, move to next line, restart dialog
		pE = (EDIT *)FindWndID(ID_CED_ADDREDIT);
		if( base==8 ) r = sscanf(pE->Text, "%o", (unsigned int*)&addr);
		else if( base==10 ) r = sscanf(pE->Text, "%d", (int*)&addr);
		else if( base==16 ) r = sscanf(pE->Text, "0x%x", (unsigned int*)&addr);
		else r = 0;
		if( r ) {
			strcpy(temp, ((EDIT *)FindWndID(ID_CED_REMEDIT))->Text);
			strcpy(lab,  ((EDIT *)FindWndID(ID_CED_LABELEDIT))->Text);
			if( strlen(lab)>8 ) lab[8] = 0;

			pE = (EDIT *)FindWndID(ID_CED_SIZEEDIT);
			if( pE->Text[0]==0 ) datover = 0;
			else if( base==8 ) r = sscanf(pE->Text, "%o", (unsigned int*)&datover);
			else if( base==10 ) r = sscanf(pE->Text, "%d", (int*)&datover);
			else if( base==16 ) r = sscanf(pE->Text, "0x%x", (unsigned int*)&datover);
			else r = 0;
			if( !r ) datover = 0;

			for(typ=0, pR=(RADIO *)FindWndID(ID_CED_CODRADIO); pR && !(pR->flags & CF_CHECKED); ++typ, pR=(RADIO *)FindWndID(ID_CED_CODRADIO+typ)) ;
			if( typ>9 ) typ = 0;
			LastType = typ;
			if( typ!=9 || !(ROM[addr] & 0x80) ) {	// if not DRP/ARP type OR is pointing at valid ARP/DRP instruction
				addr = (addr<<16) | 0xFFFF;	// convert to "encoded" address, to blend with full-line comments
				if( NULL==(ptmp=FindRecAddress(addr, ROM[060000])) ) {
					AddRec(typ, lab, addr, temp, (WORD)datover);
				} else {
					ptmp->type = typ;
					strcpy((char *)ptmp->label, lab);
					if( ptmp->Comment != NULL ) free(ptmp->Comment);
					if( temp[0] ) {
						ptmp->Comment = (char *)malloc(strlen(temp)+1);
						strcpy(ptmp->Comment, temp);
					} else ptmp->Comment = NULL;
					ptmp->datover = (WORD)datover;
				}
			}
		}
		CfgFlags &= ~CFG_REOPEN_DLG;
		if( GetCheckID(ID_CED_KEEPOPEN) ) CfgFlags |= CFG_REOPEN_DLG;
		else LastType = 0;

		ActiveDlg = NULL;
		DrawDebug();
		if( CfgFlags & CFG_REOPEN_DLG ) {
			DownCursor();
			CurRec = LineRecs[CursLine];
			if( CurRec && CurRec->type==REC_REM ) CreateCommentDlg();
			else CreateCodeEditDlg();
		} else DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT CEDremTEXT = {ID_CED_REMTEXT, CTL_TEXT,8,8, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_LEFT | WF_NOFOCUS,&TextProc,0,&KWnds[0],"Co&mment:"};
EDIT CEDremEDIT = {ID_CED_REMEDIT, CTL_EDIT,8,20, 286, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_FOCUS | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT CEDlabelTEXT = {ID_CED_LABELTEXT, CTL_TEXT,16,54, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&TextProc,0,&KWnds[0],"&Label:"};
EDIT CEDlabelEDIT = {ID_CED_LABELEDIT, CTL_EDIT,92,48, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT CEDsizeTEXT = {ID_CED_SIZETEXT, CTL_TEXT,16,78, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&TextProc,0,&KWnds[0],"&Size:"};
EDIT CEDsizeEDIT = {ID_CED_SIZEEDIT, CTL_EDIT,92,72, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT CEDaddrTEXT = {ID_CED_ADDRTEXT, CTL_TEXT,16,102, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&TextProc,0,&KWnds[0],"&Address:"};
EDIT CEDaddrEDIT = {ID_CED_ADDREDIT, CTL_EDIT,92,96, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
BUTT CEDsaveBUTT = {ID_CED_SAVE, CTL_BUTT,92, 151, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&CEDsaveProc,0,&KWnds[0],"OK"};
BUTT CEDcloseBUTT = {ID_CED_CLOSE, CTL_BUTT,92, 175, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
BUTT CEDdelBUTT = {ID_CED_DEL, CTL_BUTT,8, 175, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDdelProc,0,&KWnds[0],"Delete"};
GROUPBOX CEDtypeGROUP = {ID_CED_TYPEGROUP, CTL_GROUP,170,44, 124, 152,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Type "};
RADIO CEDcodRADIO = {ID_CED_CODRADIO, CTL_RADIO,178,56, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RadioButtonProc,0,&KWnds[0],"&Code"};
RADIO CEDbytRADIO = {ID_CED_BYTRADIO, CTL_RADIO,178,68, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&BYT"};
RADIO CEDascRADIO = {ID_CED_ASCRADIO, CTL_RADIO,178,80, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"A&SC"};
RADIO CEDaspRADIO = {ID_CED_ASPRADIO, CTL_RADIO,178,92, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"AS&P"};
RADIO CEDbszRADIO = {ID_CED_BSZRADIO, CTL_RADIO,178,104, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"BS&Z"};
RADIO CEDdefRADIO = {ID_CED_DEFRADIO, CTL_RADIO,178,116, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&DEF"};
RADIO CEDvalRADIO = {ID_CED_VALRADIO, CTL_RADIO,178,128, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&VAL"};
RADIO CEDramRADIO = {ID_CED_RAMRADIO, CTL_RADIO,178,140, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&RAM"};
RADIO CEDioRADIO = {ID_CED_IORADIO, CTL_RADIO,178,152, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&I/O"};
RADIO CEDdrpRADIO = {ID_CED_DRPRADIO, CTL_RADIO,178,164, 108, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"DRP/ARP"};
CHECK CEDkeepopenCHECK = {ID_CED_KEEPOPEN, CTL_CHECK,8,200, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"On OK do dialog for next line"};
DLG CodeEditDlg = {ID_CED, CTL_DLG,2, 2, 302, 218,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *CED_CTLs[] = {&CEDremTEXT, &CEDremEDIT,&CEDlabelTEXT, &CEDlabelEDIT,&CEDsizeTEXT, &CEDsizeEDIT,&CEDaddrTEXT, &CEDaddrEDIT,&CEDtypeGROUP,&CEDcodRADIO,
	&CEDbytRADIO,&CEDascRADIO,&CEDaspRADIO,&CEDbszRADIO,&CEDdefRADIO,&CEDvalRADIO,&CEDramRADIO,&CEDioRADIO,&CEDdrpRADIO,&CEDsaveBUTT, &CEDcloseBUTT, &CEDdelBUTT,&CEDkeepopenCHECK
};

///*********************************************************************
void CreateCodeEditDlg() {

	EDIT	*pE;

	CreateDlg(&CodeEditDlg, CED_CTLs, sizeof(CED_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	SetCheckID(ID_CED_CODRADIO+LastType, TRUE);
	SetCheckID(ID_CED_KEEPOPEN, CfgFlags & CFG_REOPEN_DLG);

// GEEKDO: set TextLim on EDIT fields!!!

	// if we have a DISASMREC for the current line, init the dialog with it
	if( base==8 ) sprintf((char*)ScratBuf, "%6.6o", (unsigned int)(LineIndex[CursLine]>>16));
	else if( base==10 ) sprintf((char*)ScratBuf, "%d", (int)(LineIndex[CursLine]>>16));
	else if( base==16 ) sprintf((char*)ScratBuf, "0x%4.4X", (unsigned int)(LineIndex[CursLine]>>16));
	strcpy(((EDIT *)FindWndID(ID_CED_ADDREDIT))->Text, (char*)ScratBuf);

	if( CurRec != NULL ) {
		pE = (EDIT *)FindWndID(ID_CED_LABELEDIT);
		strcpy(pE->Text, (char*)CurRec->label);

		if( base==8 ) sprintf((char*)ScratBuf, "0%o", CurRec->datover);
		else if( base==10 ) sprintf((char*)ScratBuf, "%d", CurRec->datover);
		else if( base==16 ) sprintf((char*)ScratBuf, "0x%4.4X", CurRec->datover);
		strcpy(((EDIT *)FindWndID(ID_CED_SIZEEDIT))->Text, (char*)ScratBuf);

		SetCheckID(ID_CED_CODRADIO  + CurRec->type, TRUE);
		pE = (EDIT *)FindWndID(ID_CED_REMEDIT);
		if( CurRec->Comment!=NULL ) strcpy(pE->Text, CurRec->Comment);
		else pE->Text[0] = 0;
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD VADcheck(DWORD r) {

	WND		*pW;
	DWORD	retval;

	pW = FindWndID(ID_VAD_GRAPHICS+r);
	if( !(RomLoad & (1<<r)) ) {
		pW->flags |= CF_DISABLED;
		retval = 0;
	} else {
		pW->flags &= ~CF_DISABLED;
		retval = ROM[060000]==RomList[r];
		SetCheckID(ID_VAD_GRAPHICS+r, retval);
	}
	return retval;
}

///*********************************************************************
DWORD VADviewProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pE;
	long		i, r;
	WORD		addr;

	switch( msg ) {
	 case DLG_ACTIVATE:
		// get ROM and map in
		r = 0;
		if( !GetCheckID(ID_VAD_SYS) ) {
			for(i=0; i<RomListCnt; i++) if( GetCheckID(ID_VAD_GRAPHICS+i) ) break;
			if( i<RomListCnt ) r = RomList[i];
		}
		SelectROM(r);
		// save address, set DisStart to it, DrawAll
		pE = (EDIT *)FindWndID(ID_VAD_ADDREDIT);
		if( base==8 ) r = sscanf(pE->Text, "%o", (unsigned int*)&addr);
		else if( base==10 ) r = sscanf(pE->Text, "%d", (int*)&addr);
		else if( base==16 ) r = sscanf(pE->Text, "0x%x", (unsigned int*)&addr);
		else r = 0;
		if( r ) {
			if( (DWORD)addr > 0x00010000-TraceLines ) addr = (WORD)(DWORD)(0x00010000-TraceLines);
			DisStart = (addr<<16) | 0x0000FFFF;
			CursLine = 0;
		}
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
GROUPBOX VADromGROUP = {ID_ROMD_ROMGROUP, CTL_GROUP,8,8, 286, 246,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Option ROM "};
RADIO VAD0RADIO = {ID_VAD_SYS, CTL_RADIO,16,20, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RadioButtonProc,0,&KWnds[0],"(000) System"};
RADIO VADgraphRADIO = {ID_VAD_GRAPHICS, CTL_RADIO,16,32, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(001) HP-87 Graphics"};
RADIO VADpdRADIO = {ID_VAD_PD, CTL_RADIO,16,44, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(010) Program Development"};
RADIO VADmiksamRADIO = {ID_VAD_MIKSAM, CTL_RADIO,16,56, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(016) MIKSAM (87)"};
RADIO VADlangRADIO = {ID_VAD_LANG, CTL_RADIO,16,68, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(030) Language (87)"};
RADIO VADextRADIO = {ID_VAD_EXT, CTL_RADIO,16,80, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(047) E. Kaser's ExtRom (87)"};
RADIO VADasmRADIO = {ID_VAD_ASM, CTL_RADIO,16,92, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(050) Assembler"};
RADIO VADstructRADIO = {ID_VAD_STRUCT, CTL_RADIO,16,104, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(070) A. Koppel's SYSEXT (87)"};
RADIO VADforRADIO = {ID_VAD_FOR, CTL_RADIO,16,116, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(250) Forth"};
RADIO VADmatRADIO = {ID_VAD_MAT, CTL_RADIO,16,128, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(260) Matrix"};
RADIO VADmat2RADIO = {ID_VAD_MAT2, CTL_RADIO,16,140, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(261) Matrix 2 (87)"};
RADIO VADioRADIO = {ID_VAD_IO, CTL_RADIO,16,152, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(300) I/O"};
RADIO VADemsRADIO = {ID_VAD_EMS, CTL_RADIO,16,164, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(317) Extended Mass Storage"};
RADIO VADmsRADIO = {ID_VAD_MS, CTL_RADIO,16,176, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(320) Mass Storage"};
RADIO VADedRADIO = {ID_VAD_ED, CTL_RADIO,16,188, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(321) Electronic Disk"};
RADIO VADservRADIO = {ID_VAD_SERV, CTL_RADIO,16,200, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(340) Service (85)"};
RADIO VADap2RADIO = {ID_VAD_AP2, CTL_RADIO,16,212, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(347) Adv Programming 2 (87)"};
RADIO VADapRADIO = {ID_VAD_AP, CTL_RADIO,16,224, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(350) Advanced Programming"};
RADIO VADppRADIO = {ID_VAD_PP, CTL_RADIO,16,236, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"(360) Plotter/Printer"};
TEXT VADaddrTEXT = {ID_VAD_ADDRTEXT, CTL_TEXT,16,266, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"&Address:"};
EDIT VADaddrEDIT = {ID_VAD_ADDREDIT, CTL_EDIT,84,262, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0],};
BUTT VADviewBUTT = {ID_VAD_VIEW, CTL_BUTT,166,262, 60, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&VADviewProc,0,&KWnds[0],"View"};
BUTT VADcloseBUTT = {ID_VAD_CANCEL, CTL_BUTT,230,262, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
DLG ViewAddressDlg = {ID_VAD, CTL_DLG,2, 2, 302, 290,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *VAD_CTLs[] = {&VADromGROUP,&VAD0RADIO, &VADgraphRADIO, &VADpdRADIO, &VADmiksamRADIO,&VADlangRADIO, &VADextRADIO, &VADasmRADIO, &VADstructRADIO,
	&VADforRADIO, &VADmatRADIO, &VADmat2RADIO, &VADioRADIO,&VADemsRADIO, &VADmsRADIO, &VADedRADIO, &VADservRADIO,&VADap2RADIO, &VADapRADIO, &VADppRADIO,
	&VADaddrTEXT, &VADaddrEDIT,&VADviewBUTT, &VADcloseBUTT
};

///*********************************************************************
void CreateViewAddressDlg() {

	DWORD	r, i;

	CreateDlg(&ViewAddressDlg, VAD_CTLs, sizeof(VAD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	r = 0;
	for(i=0; i<RomListCnt; i++) r |= VADcheck(i);
	if( !r ) SetCheckID(ID_VAD_SYS, 1);	// select ROM 0 if it's mapped in or NONE are mapped in

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD COMDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pE;

	switch( msg ) {
	 case DLG_ACTIVATE:
		// save comment, DrawAll
		if( CurRec != NULL ) {	// shouldn't be, but just in case...
			pE = (EDIT *)FindWndID(ID_COMD_REMEDIT);
			if( CurRec->Comment != NULL ) free(CurRec->Comment);
			if( pE->Text[0] ) {
				CurRec->Comment = (char *)malloc(strlen(pE->Text)+1);
				strcpy(CurRec->Comment, pE->Text);
			} else CurRec->Comment = NULL;
		}

		CfgFlags &= ~CFG_REOPEN_DLG;
		if( GetCheckID(ID_COMD_KEEPOPEN) ) CfgFlags |= CFG_REOPEN_DLG;

		ActiveDlg = NULL;
		if( CfgFlags & CFG_REOPEN_DLG ) {
			DownCursor();
			CurRec = LineRecs[CursLine];
			if( CurRec ) {
				if( CurRec->type!=REC_REM ) CreateCodeEditDlg();
				else CreateCommentDlg();
			}
		} else DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD COMDdelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
		if( CurRec != NULL ) DeleteRec(CurRec);
		ActiveDlg = NULL;
		if( CfgFlags & CFG_REOPEN_DLG ) {
			DownCursor();
			CurRec = LineRecs[CursLine];
			if( CurRec ) {
				if( CurRec->type!=REC_REM ) CreateCodeEditDlg();
				else CreateCommentDlg();
			}
		} else DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT COMDremTEXT = {ID_COMD_REMTEXT, CTL_TEXT,8,8, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_LEFT | WF_NOFOCUS,&TextProc,0,&KWnds[0],"Co&mment:"};
EDIT COMDremEDIT = {ID_COMD_REMEDIT, CTL_EDIT,8,20, 286, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_FOCUS | CF_GROUP,&EditProc,0,&KWnds[0],};
BUTT COMDokBUTT = {ID_COMD_OK, CTL_BUTT,234,190, 60, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&COMDokProc,0,&KWnds[0],"OK"};
BUTT COMDcancelBUTT = {ID_COMD_CANCEL, CTL_BUTT,166,190, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
BUTT COMDdelBUTT = {ID_COMD_DEL, CTL_BUTT,8,190, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&COMDdelProc,0,&KWnds[0],"Delete"};
CHECK COMDkeepopenCHECK = {ID_COMD_KEEPOPEN, CTL_CHECK,8,174, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"On OK do dialog for next line"};
DLG CommentDlg = {ID_COMD, CTL_DLG,2, 2, 302, 218,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *COMD_CTLs[] = {&COMDremTEXT, &COMDremEDIT,&COMDkeepopenCHECK,&COMDokBUTT, &COMDcancelBUTT, &COMDdelBUTT};

///***********************************************************************
void CreateCommentDlg() {

	EDIT	*pE;

	CreateDlg(&CommentDlg, COMD_CTLs, sizeof(COMD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	SetCheckID(ID_COMD_KEEPOPEN, CfgFlags & CFG_REOPEN_DLG);

	if( CurRec != NULL && CurRec->type==REC_REM ) {
		if( NULL!=(pE=(EDIT *)FindWndID(ID_COMD_REMEDIT)) ) {
			if( CurRec->Comment != NULL ) strcpy(pE->Text, CurRec->Comment);
		}
	}
	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*************************************
/// NOTE: While we use MAX_DEVICES in
/// many places, the DEVICES dialog is
/// designed for a maximum of 32 devices.
/// So if MAX_DEVICES is ever changed,
/// then the DEVICES dialog would need
/// to be changed as well.
///*************************************
DLG		*DevSaveDlg;
DEV		DEVD_DEV[MAX_DEVICES];
int		NextDevID;
BOOL	NeedSort=TRUE;
char	SaveEditStr[256];	// for detecting change in SC/CA edit CTL at DLG_KILLFOCUS
char	*DEVD_names[]={
	"5.25\" DISK (82901)",
	"3.5\" DISK (9121)",
	"8\" DISK (9895)",
	"5 MB DISK (9135)",
	"",
	"",
	"",
	"",
	"",
	"",
	"PRINTER (to file)",
	"PRINTER (actual)",
};

///*********************************************************************
void SortDevicesDlg() {

	int			i, j;
	DEV			d;
	TEXT		*pT;
	EDIT		*pE;

	// sort by SC then CA
	// i = next destination slot
	// j = next source slot
	for(i=0; i<MAX_DEVICES; i++) {
		for(j=i+1; j<MAX_DEVICES; j++) {
			if( DEVD_DEV[j].devType!=DEVTYPE_NONE ) {	// we've found an existing device in the list
				if( (DEVD_DEV[i].devType==DEVTYPE_NONE && DEVD_DEV[i].state==DEVSTATE_DELETED)
				 || (DEVD_DEV[j].wsc < DEVD_DEV[i].wsc || (DEVD_DEV[j].wsc==DEVD_DEV[i].wsc && DEVD_DEV[j].wca < DEVD_DEV[i].wca)) ) {
					memcpy(&d, &DEVD_DEV[i], sizeof(DEV));
					memcpy(&DEVD_DEV[i], &DEVD_DEV[j], sizeof(DEV));
					memcpy(&DEVD_DEV[j], &d, sizeof(DEV));
				}
			}
		}
	}
	for(i=0; i<MAX_DEVICES; i++) {
		pT = (TEXT *)FindWndID(ID_DEVD_DEV00+i);
		if( DEVD_DEV[i].devType==DEVTYPE_NONE ) {
			pT->pText = "";
		} else {
			pT->pText = DEVD_names[DEVD_DEV[i].devType];
			pE = (EDIT *)FindWndID(ID_DEVD_SC00+i);
			sprintf(pE->Text, "%d", DEVD_DEV[i].wsc+3);
			pE = (EDIT *)FindWndID(ID_DEVD_CA00+i);
			sprintf(pE->Text, "%d", DEVD_DEV[i].wca);
		}
	}
	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
	NeedSort = FALSE;
}

///*********************************************************************
DWORD DEVDdelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	TEXT	*pT;
	int		i;

	if( msg!=DLG_CREATE ) {
		i = pWnd->id - ID_DEVD_DEL00;	// get relative index of device in dialog list
	 	pT = (TEXT *)FindWndID(ID_DEVD_DEV00+i);
	}
	switch( msg ) {
	 case DLG_DRAW:
		if( pT->pText[0]==0 ) return 0;	// don't draw if no name here
		break;
	 case DLG_ACTIVATE:
	 	if( DEVD_DEV[i].devType!=DEVTYPE_NONE ) {
			DEVD_DEV[i].devType = DEVTYPE_NONE;
			// if the device was in the .ini file, then change to DELETED, else it was ADDED and then DELETED, so set back to UNUSED
			DEVD_DEV[i].state = (DEVD_DEV[i].state==DEVSTATE_BOOT) ? DEVSTATE_DELETED : DEVSTATE_UNUSED;
			SortDevicesDlg();
			DrawAll();
	 	}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DEVDaddProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	int			d, i, j, calim;

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	d = pWnd->id - ID_DEVD_ADD_DISK525;	// which item we're adding (d now equals a DEVTYPE_ value)

	 	for(i=0; i<MAX_DEVICES; i++) if( DEVD_DEV[i].devType == DEVTYPE_NONE ) break;
	 	if( i<MAX_DEVICES ) {
			// find an empty SC/CA for this device to default to
			calim = (d<=DEV_MAX_DISK_TYPE)?8:31;
			for(DEVD_DEV[i].wsc = 0; DEVD_DEV[i].wsc<8; DEVD_DEV[i].wsc++) {
				for(DEVD_DEV[i].wca=0; DEVD_DEV[i].wca<calim; DEVD_DEV[i].wca++) {
					if( DEVD_DEV[i].wca!=TLA85 ) {	// don't allow Series 80 CPU's LAD/TAD address
						for(j=0; j<MAX_DEVICES; j++) if( DEVD_DEV[j].devType!=DEVTYPE_NONE && DEVD_DEV[j].wca==DEVD_DEV[i].wca && DEVD_DEV[j].wsc==DEVD_DEV[i].wsc ) break;
						if( j>=MAX_DEVICES ) {
							DEVD_DEV[i].devType = d;
							DEVD_DEV[i].ioType = IOTYPE_HPIB;
							DEVD_DEV[i].state = DEVSTATE_NEW;
							DEVD_DEV[i].devID = NextDevID++;	// give it its unique ID tracker
							DEVD_DEV[i].ca = DEVD_DEV[i].wca;
							DEVD_DEV[i].sc = DEVD_DEV[i].wsc;
							DEVD_DEV[i].initProc = devProcs[d];
							goto gotslot;
						}
					}
				}
			}
			DEVD_DEV[i].devType = DEVTYPE_NONE;	// should never happen (no SC/CA open), but just in case
	 	}
gotslot:
	 	SortDevicesDlg();
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD DEVDtextProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	TEXT	*pC;

	pC = (TEXT *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		if( pC->pText[0]==0 ) return 0;	// don't draw if no name here
		break;
	 default:
		break;
	}
	return TextProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD DEVDeditProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pC;
	TEXT		*pT;
	int			i, j, sc, scca;

	if( msg!=DLG_CREATE ) {
		pC = (EDIT *)pWnd;
		if( pC->id > ID_DEVD_SC31 ) i = pC->id - ID_DEVD_CA00;
		else i = pC->id - ID_DEVD_SC00;
		pT = (TEXT *)FindWndID(ID_DEVD_DEV00+i);
	}
	switch( msg ) {
	 case DLG_CREATE:
		pC = (EDIT *)EditProc(pWnd, msg, arg1, arg2, arg3);
		pC->buflen = 3;
//		pC->Text[0] = 0;
		pC->textlim = 2;
		return (DWORD)pC;
	 case DLG_DRAW:
		if( pT->pText[0]==0 ) return 0;	// don't draw if no name here
		break;
	 case DLG_SETFOCUS:
	 	if( DEVD_DEV[i].devType==DEVTYPE_NONE ) {
			ChangeFocus(FindWndID(ID_DEVD_OK));
			return 0;
	 	}
	 	strcpy(SaveEditStr, pC->Text);	// save for DLG_KILLFOCUS comparision
		break;
	 case DLG_KILLFOCUS:
	 	if( strcmp(SaveEditStr, pC->Text) ) {	// if contents changed
			if( pC->id > ID_DEVD_SC31 ) {	// CA
				DEVD_DEV[i].wca = atoi(pC->Text);
			} else {						// SC
				sc = atoi(pC->Text)-3;
				if( sc>=0 && sc<8 ) DEVD_DEV[i].wsc = sc;
			}
			NeedSort = TRUE;
	 	}
		break;
	 case DLG_TIMER:
		if( NeedSort ) {
			if( wndFocus!=NULL && wndFocus->id>=ID_DEVD_SC00 && wndFocus->id<=ID_DEVD_CA31 ) {	// focus is in EDIT box
				pC = (EDIT*)wndFocus;
				if( pC->id > ID_DEVD_SC31 ) {
					i = DEVD_DEV[pC->id - ID_DEVD_CA00].devID;
					scca = 1;
				} else {
					i = DEVD_DEV[pC->id - ID_DEVD_SC00].devID;
					scca = 0;
				}
				SortDevicesDlg();
				DrawAll();
				for(j=0; j<MAX_DEVICES; j++) if( DEVD_DEV[j].devID == i ) {
					ChangeFocus(FindWndID((scca?ID_DEVD_CA00:ID_DEVD_SC00)+j));
					break;
				}
			} else {	// focus is NOT in EDIT box
				SortDevicesDlg();
				DrawAll();
			}
		}
	 	break;
	 default:
		break;
	}
	return EditProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DEVDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	int		i;
	DWORD	scca[8];

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	if( wndFocus!=NULL && wndFocus->type==CTL_EDIT ) SendMsg(wndFocus, DLG_KILLFOCUS, 0, 0, 0);	// make sure possible edit change is saved
	 	/// check if all SC/CA's are unique and that no CA==21
	 	for(i=0; i<8; i++) scca[i] = (1<<21);	// set COMPUTER's CA, clear all others
		for(i=0; i<MAX_DEVICES; i++) {
			if( DEVD_DEV[i].devType!=DEVTYPE_NONE ) {
				if( scca[DEVD_DEV[i].wsc] & (1<<DEVD_DEV[i].wca) ) {
					if( DEVD_DEV[i].wca==21 ) SystemMessage("Sorry, CA=21 is taken by the Series 80 HPIB Controller (HPIB card).");
					else {
						sprintf((char*)ScratBuf, "Sorry, you can't have more than one device\nat the same SC (%d) and CA (%d).", DEVD_DEV[i].wsc+3, DEVD_DEV[i].wca);
						SystemMessage((char*)ScratBuf);
					}
					return 0;
				}
				scca[DEVD_DEV[i].wsc] |= (1<<DEVD_DEV[i].wca);
			}
		}
	 	/// okay, kill the dialog and return to the SERIES 80 OPTIONS (ROMD) dialog
	 	SendMsg(wndFocus, DLG_KILLFOCUS, 0, 0, 0);	// to update current EDIT value (if any)
		wndFocus = NULL;
		ActiveDlg = DevSaveDlg;
		ChangeFocus(ActiveDlg->child1);
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DEVDcancelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
		ActiveDlg = DevSaveDlg;
		DrawAll();
//		CreateDiskDlg(DiskSC, DiskTLA, DiskDlgDrive, DiskDevType);
		return 0;
	 case DLG_CHAR:
		if( arg1==27 ) {
			return SendMsg(pWnd, DLG_ACTIVATE, 0, 0, 0);
		}
		break;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
GROUPBOX DEVDdevGROUP = {ID_DEVD_DEVGROUP, CTL_GROUP, 8,  8, 286, 412,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS,&GroupboxProc,0,&KWnds[0]," Current devices "};

TEXT DEVDSCTEXT = {ID_DEVD_DEVSC,	CTL_TEXT,	166, 20,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_CENTER | WF_NOFOCUS,&TextProc,0,&KWnds[0],"SC"};
TEXT DEVDCATEXT = {ID_DEVD_DEVCA,	CTL_TEXT,	194, 20,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_CENTER | WF_NOFOCUS,&TextProc,0,&KWnds[0],"CA"};

TEXT DEVD00TEXT = {ID_DEVD_DEV00,	CTL_TEXT,	 16, 32, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],"Diskette drive"};
EDIT DEVD00SC	= {ID_DEVD_SC00, 	CTL_EDIT,	166, 30,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD00CA	= {ID_DEVD_CA00, 	CTL_EDIT,	194, 30,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD00DEL 	= {ID_DEVD_DEL00,	CTL_BUTT,	222, 30,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD01TEXT = {ID_DEVD_DEV01,	CTL_TEXT,	 16, 44, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],"Print to FILE"};
EDIT DEVD01SC	= {ID_DEVD_SC01, 	CTL_EDIT,	166, 42,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD01CA	= {ID_DEVD_CA01, 	CTL_EDIT,	194, 42,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD01DEL 	= {ID_DEVD_DEL01,	CTL_BUTT,	222, 42,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD02TEXT = {ID_DEVD_DEV02,	CTL_TEXT,	 16, 56, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],"Print to PRINTER"};
EDIT DEVD02SC	= {ID_DEVD_SC02, 	CTL_EDIT,	166, 54,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD02CA	= {ID_DEVD_CA02, 	CTL_EDIT,	194, 54,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD02DEL 	= {ID_DEVD_DEL02,	CTL_BUTT,	222, 54,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD03TEXT = {ID_DEVD_DEV03,	CTL_TEXT,	 16, 68, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD03SC	= {ID_DEVD_SC03, 	CTL_EDIT,	166, 66,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD03CA	= {ID_DEVD_CA03, 	CTL_EDIT,	194, 66,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD03DEL 	= {ID_DEVD_DEL03,	CTL_BUTT,	222, 66,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD04TEXT = {ID_DEVD_DEV04,	CTL_TEXT,	 16, 80, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD04SC	= {ID_DEVD_SC04, 	CTL_EDIT,	166, 78,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD04CA	= {ID_DEVD_CA04, 	CTL_EDIT,	194, 78,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD04DEL 	= {ID_DEVD_DEL04,	CTL_BUTT,	222, 78,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD05TEXT = {ID_DEVD_DEV05,	CTL_TEXT,	 16, 92, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD05SC	= {ID_DEVD_SC05, 	CTL_EDIT,	166, 90,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD05CA	= {ID_DEVD_CA05, 	CTL_EDIT,	194, 90,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD05DEL 	= {ID_DEVD_DEL05,	CTL_BUTT,	222, 90,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD06TEXT = {ID_DEVD_DEV06,	CTL_TEXT,	 16,104, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD06SC	= {ID_DEVD_SC06, 	CTL_EDIT,	166,102,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD06CA	= {ID_DEVD_CA06, 	CTL_EDIT,	194,102,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD06DEL 	= {ID_DEVD_DEL06,	CTL_BUTT,	222,102,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD07TEXT = {ID_DEVD_DEV07,	CTL_TEXT,	 16,116, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD07SC	= {ID_DEVD_SC07, 	CTL_EDIT,	166,114,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD07CA	= {ID_DEVD_CA07, 	CTL_EDIT,	194,114,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD07DEL 	= {ID_DEVD_DEL07,	CTL_BUTT,	222,114,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD08TEXT = {ID_DEVD_DEV08,	CTL_TEXT,	 16,128, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD08SC	= {ID_DEVD_SC08, 	CTL_EDIT,	166,126,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD08CA	= {ID_DEVD_CA08, 	CTL_EDIT,	194,126,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD08DEL 	= {ID_DEVD_DEL08,	CTL_BUTT,	222,126,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD09TEXT = {ID_DEVD_DEV09,	CTL_TEXT,	 16,140, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD09SC	= {ID_DEVD_SC09, 	CTL_EDIT,	166,138,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD09CA	= {ID_DEVD_CA09, 	CTL_EDIT,	194,138,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD09DEL 	= {ID_DEVD_DEL09,	CTL_BUTT,	222,138,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD10TEXT = {ID_DEVD_DEV10,	CTL_TEXT,	 16,152, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD10SC	= {ID_DEVD_SC10, 	CTL_EDIT,	166,150,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD10CA	= {ID_DEVD_CA10, 	CTL_EDIT,	194,150,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD10DEL 	= {ID_DEVD_DEL10,	CTL_BUTT,	222,150,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD11TEXT = {ID_DEVD_DEV11,	CTL_TEXT,	 16,164, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD11SC	= {ID_DEVD_SC11, 	CTL_EDIT,	166,162,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD11CA	= {ID_DEVD_CA11, 	CTL_EDIT,	194,162,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD11DEL 	= {ID_DEVD_DEL11,	CTL_BUTT,	222,162,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD12TEXT = {ID_DEVD_DEV12,	CTL_TEXT,	 16,176, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD12SC	= {ID_DEVD_SC12, 	CTL_EDIT,	166,174,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD12CA	= {ID_DEVD_CA12, 	CTL_EDIT,	194,174,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD12DEL 	= {ID_DEVD_DEL12,	CTL_BUTT,	222,174,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD13TEXT = {ID_DEVD_DEV13,	CTL_TEXT,	 16,188, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD13SC	= {ID_DEVD_SC13, 	CTL_EDIT,	166,186,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD13CA	= {ID_DEVD_CA13, 	CTL_EDIT,	194,186,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD13DEL 	= {ID_DEVD_DEL13,	CTL_BUTT,	222,186,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD14TEXT = {ID_DEVD_DEV14,	CTL_TEXT,	 16,200, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD14SC	= {ID_DEVD_SC14, 	CTL_EDIT,	166,198,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD14CA	= {ID_DEVD_CA14, 	CTL_EDIT,	194,198,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD14DEL 	= {ID_DEVD_DEL14,	CTL_BUTT,	222,198,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD15TEXT = {ID_DEVD_DEV15,	CTL_TEXT,	 16,212, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD15SC	= {ID_DEVD_SC15, 	CTL_EDIT,	166,210,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD15CA	= {ID_DEVD_CA15, 	CTL_EDIT,	194,210,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD15DEL 	= {ID_DEVD_DEL15,	CTL_BUTT,	222,210,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD16TEXT = {ID_DEVD_DEV16,	CTL_TEXT,	 16,224, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD16SC	= {ID_DEVD_SC16, 	CTL_EDIT,	166,222,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD16CA	= {ID_DEVD_CA16, 	CTL_EDIT,	194,222,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD16DEL 	= {ID_DEVD_DEL16,	CTL_BUTT,	222,222,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD17TEXT = {ID_DEVD_DEV17,	CTL_TEXT,	 16,236, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD17SC	= {ID_DEVD_SC17, 	CTL_EDIT,	166,234,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD17CA	= {ID_DEVD_CA17, 	CTL_EDIT,	194,234,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD17DEL 	= {ID_DEVD_DEL17,	CTL_BUTT,	222,234,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD18TEXT = {ID_DEVD_DEV18,	CTL_TEXT,	 16,248, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD18SC	= {ID_DEVD_SC18, 	CTL_EDIT,	166,246,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD18CA	= {ID_DEVD_CA18, 	CTL_EDIT,	194,246,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD18DEL 	= {ID_DEVD_DEL18,	CTL_BUTT,	222,246,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD19TEXT = {ID_DEVD_DEV19,	CTL_TEXT,	 16,260, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD19SC	= {ID_DEVD_SC19, 	CTL_EDIT,	166,258,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD19CA	= {ID_DEVD_CA19, 	CTL_EDIT,	194,258,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD19DEL 	= {ID_DEVD_DEL19,	CTL_BUTT,	222,258,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD20TEXT = {ID_DEVD_DEV20,	CTL_TEXT,	 16,272, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD20SC	= {ID_DEVD_SC20, 	CTL_EDIT,	166,270,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD20CA	= {ID_DEVD_CA20, 	CTL_EDIT,	194,270,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD20DEL 	= {ID_DEVD_DEL20,	CTL_BUTT,	222,270,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD21TEXT = {ID_DEVD_DEV21,	CTL_TEXT,	 16,284, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD21SC	= {ID_DEVD_SC21, 	CTL_EDIT,	166,282,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD21CA	= {ID_DEVD_CA21, 	CTL_EDIT,	194,282,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD21DEL 	= {ID_DEVD_DEL21,	CTL_BUTT,	222,282,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD22TEXT = {ID_DEVD_DEV22,	CTL_TEXT,	 16,296, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD22SC	= {ID_DEVD_SC22, 	CTL_EDIT,	166,294,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD22CA	= {ID_DEVD_CA22, 	CTL_EDIT,	194,294,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD22DEL 	= {ID_DEVD_DEL22,	CTL_BUTT,	222,294,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD23TEXT = {ID_DEVD_DEV23,	CTL_TEXT,	 16,308, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD23SC	= {ID_DEVD_SC23, 	CTL_EDIT,	166,306,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD23CA	= {ID_DEVD_CA23, 	CTL_EDIT,	194,306,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD23DEL 	= {ID_DEVD_DEL23,	CTL_BUTT,	222,306,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD24TEXT = {ID_DEVD_DEV24,	CTL_TEXT,	 16,320, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD24SC	= {ID_DEVD_SC24, 	CTL_EDIT,	166,318,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD24CA	= {ID_DEVD_CA24, 	CTL_EDIT,	194,318,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD24DEL 	= {ID_DEVD_DEL24,	CTL_BUTT,	222,318,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD25TEXT = {ID_DEVD_DEV25,	CTL_TEXT,	 16,332, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD25SC	= {ID_DEVD_SC25, 	CTL_EDIT,	166,330,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD25CA	= {ID_DEVD_CA25, 	CTL_EDIT,	194,330,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD25DEL 	= {ID_DEVD_DEL25,	CTL_BUTT,	222,330,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD26TEXT = {ID_DEVD_DEV26,	CTL_TEXT,	 16,344, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD26SC	= {ID_DEVD_SC26, 	CTL_EDIT,	166,342,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD26CA	= {ID_DEVD_CA26, 	CTL_EDIT,	194,342,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD26DEL 	= {ID_DEVD_DEL26,	CTL_BUTT,	222,342,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD27TEXT = {ID_DEVD_DEV27,	CTL_TEXT,	 16,356, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD27SC	= {ID_DEVD_SC27, 	CTL_EDIT,	166,354,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD27CA	= {ID_DEVD_CA27, 	CTL_EDIT,	194,354,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD27DEL 	= {ID_DEVD_DEL27,	CTL_BUTT,	222,354,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD28TEXT = {ID_DEVD_DEV28,	CTL_TEXT,	 16,368, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD28SC	= {ID_DEVD_SC28, 	CTL_EDIT,	166,366,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD28CA	= {ID_DEVD_CA28, 	CTL_EDIT,	194,366,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD28DEL 	= {ID_DEVD_DEL28,	CTL_BUTT,	222,366,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD29TEXT = {ID_DEVD_DEV29,	CTL_TEXT,	 16,380, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD29SC	= {ID_DEVD_SC29, 	CTL_EDIT,	166,378,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD29CA	= {ID_DEVD_CA29, 	CTL_EDIT,	194,378,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD29DEL 	= {ID_DEVD_DEL29,	CTL_BUTT,	222,378,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD30TEXT = {ID_DEVD_DEV30,	CTL_TEXT,	 16,392, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD30SC	= {ID_DEVD_SC30, 	CTL_EDIT,	166,390,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD30CA	= {ID_DEVD_CA30, 	CTL_EDIT,	194,390,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD30DEL 	= {ID_DEVD_DEL30,	CTL_BUTT,	222,390,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

TEXT DEVD31TEXT = {ID_DEVD_DEV31,	CTL_TEXT,	 16,404, 144,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS,&DEVDtextProc,0,&KWnds[0],""};
EDIT DEVD31SC	= {ID_DEVD_SC31, 	CTL_EDIT,	166,402,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
EDIT DEVD31CA	= {ID_DEVD_CA31, 	CTL_EDIT,	194,402,  24,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDeditProc,0,&KWnds[0]};
BUTT DEVD31DEL 	= {ID_DEVD_DEL31,	CTL_BUTT,	222,402,  64,  12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDdelProc,0,&KWnds[0],"Delete"};

BUTT DEVDADD35	= {ID_DEVD_ADD_DISK35,	CTL_BUTT,  8,424, 92,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDaddProc,0,&KWnds[0],"+3.5 disk"};
BUTT DEVDADD525	= {ID_DEVD_ADD_DISK525,	CTL_BUTT,  8,448, 92,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDaddProc,0,&KWnds[0],"+5.25 disk"};
BUTT DEVDADD8	= {ID_DEVD_ADD_DISK8,	CTL_BUTT,  8,472, 92,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDaddProc,0,&KWnds[0],"+8\" disk"};

BUTT DEVDADD5MB	= {ID_DEVD_ADD_DISK5MB,	CTL_BUTT,105,424, 92,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDaddProc,0,&KWnds[0],"+5 MB disk"};

BUTT DEVDADDFP	= {ID_DEVD_ADD_FILEPRT,	CTL_BUTT,202,424, 92,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDaddProc,0,&KWnds[0],"+File PRT"};
BUTT DEVDADDHP	= {ID_DEVD_ADD_HARDPRT,	CTL_BUTT,202,448, 92,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDaddProc,0,&KWnds[0],"+Real PRT"};

/// GEEKDO: When ESC pressed, exits clear out of OPTIONS dialog. Replace DlgProc and process ESC.

BUTT DEVDokBUTT 	= {ID_DEVD_OK,		CTL_BUTT,	  8,514,  72,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT,&DEVDokProc,0,&KWnds[0],"Close"};
//BUTT DEVDcancelBUTT = {ID_DEVD_CANCEL,	CTL_BUTT,	155,514,  64,  20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DEVDcancelProc,0,&KWnds[0],"Cancel"};
DLG DevicesDlg 		= {ID_DEVD,			CTL_DLG,	  2,  2, 302, 542,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *DEVD_CTLs[] = {&DEVDdevGROUP,
	&DEVDSCTEXT, &DEVDCATEXT,
	&DEVD00TEXT, &DEVD00SC, &DEVD00CA, &DEVD00DEL,
	&DEVD01TEXT, &DEVD01SC, &DEVD01CA, &DEVD01DEL,
	&DEVD02TEXT, &DEVD02SC, &DEVD02CA, &DEVD02DEL,
	&DEVD03TEXT, &DEVD03SC, &DEVD03CA, &DEVD03DEL,
	&DEVD04TEXT, &DEVD04SC, &DEVD04CA, &DEVD04DEL,
	&DEVD05TEXT, &DEVD05SC, &DEVD05CA, &DEVD05DEL,
	&DEVD06TEXT, &DEVD06SC, &DEVD06CA, &DEVD06DEL,
	&DEVD07TEXT, &DEVD07SC, &DEVD07CA, &DEVD07DEL,
	&DEVD08TEXT, &DEVD08SC, &DEVD08CA, &DEVD08DEL,
	&DEVD09TEXT, &DEVD09SC, &DEVD09CA, &DEVD09DEL,
	&DEVD10TEXT, &DEVD10SC, &DEVD10CA, &DEVD10DEL,
	&DEVD11TEXT, &DEVD11SC, &DEVD11CA, &DEVD11DEL,
	&DEVD12TEXT, &DEVD12SC, &DEVD12CA, &DEVD12DEL,
	&DEVD13TEXT, &DEVD13SC, &DEVD13CA, &DEVD13DEL,
	&DEVD14TEXT, &DEVD14SC, &DEVD14CA, &DEVD14DEL,
	&DEVD15TEXT, &DEVD15SC, &DEVD15CA, &DEVD15DEL,
	&DEVD16TEXT, &DEVD16SC, &DEVD16CA, &DEVD16DEL,
	&DEVD17TEXT, &DEVD17SC, &DEVD17CA, &DEVD17DEL,
	&DEVD18TEXT, &DEVD18SC, &DEVD18CA, &DEVD18DEL,
	&DEVD19TEXT, &DEVD19SC, &DEVD19CA, &DEVD19DEL,
	&DEVD20TEXT, &DEVD20SC, &DEVD20CA, &DEVD20DEL,
	&DEVD21TEXT, &DEVD21SC, &DEVD21CA, &DEVD21DEL,
	&DEVD22TEXT, &DEVD22SC, &DEVD22CA, &DEVD22DEL,
	&DEVD23TEXT, &DEVD23SC, &DEVD23CA, &DEVD23DEL,
	&DEVD24TEXT, &DEVD24SC, &DEVD24CA, &DEVD24DEL,
	&DEVD25TEXT, &DEVD25SC, &DEVD25CA, &DEVD25DEL,
	&DEVD26TEXT, &DEVD26SC, &DEVD26CA, &DEVD26DEL,
	&DEVD27TEXT, &DEVD27SC, &DEVD27CA, &DEVD27DEL,
	&DEVD28TEXT, &DEVD28SC, &DEVD28CA, &DEVD28DEL,
	&DEVD29TEXT, &DEVD29SC, &DEVD29CA, &DEVD29DEL,
	&DEVD30TEXT, &DEVD30SC, &DEVD30CA, &DEVD30DEL,
	&DEVD31TEXT, &DEVD31SC, &DEVD31CA, &DEVD31DEL,

	&DEVDADD525, &DEVDADD35, &DEVDADD8,
	&DEVDADD5MB,
	&DEVDADDFP, &DEVDADDHP,

	&DEVDokBUTT
};

///*********************************************************************
void CreateDevicesDlg() {

	int		i;
	EDIT	*pE;
	TEXT	*pT;
//	int		numctl=sizeof(DEVD_CTLs)/sizeof(DEVD_CTLs[0]);

	DevSaveDlg = ActiveDlg;

	CreateDlg(&DevicesDlg, DEVD_CTLs, sizeof(DEVD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	// initialize values for working copy of devices
	for(i=0; i<MAX_DEVICES; i++) {
		if( DEVD_DEV[i].devType != DEVTYPE_NONE ) {
			pT = (TEXT *)FindWndID(ID_DEVD_DEV00+i);
			pT->pText = DEVD_names[DEVD_DEV[i].devType];
			pE = (EDIT *)FindWndID(ID_DEVD_SC00+i);
			sprintf(pE->Text, "%d", DEVD_DEV[i].wsc+3);
			pE = (EDIT *)FindWndID(ID_DEVD_CA00+i);
			sprintf(pE->Text, "%d", DEVD_DEV[i].wca);
		}
	}
	SortDevicesDlg();
	ChangeFocus(FindWndID(ID_DEVD_SC00));
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD ROMDdevicesProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

//	RADIO		*pR;
//	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
/*		for(i=0; i<MAX_DISKS+1; i++) if( GetCheckID(ID_DSKD_DISK0RADIO+i) ) break;

		if( i>0 && i<=MAX_DISKS ) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
			if( pR != NULL && pR->pText[0] ) {
				ActiveDlg = NULL;	// "destroy" tape dialog
				DrawAll();
				CreateRenameDiskDlg(pR->pText);	// rename this file
			}
		}
*/
		CreateDevicesDlg();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************
void SetROMctl(DWORD romindex) {

	WND		*pW;
	DWORD	id, mask;

	id = ROMctlID[romindex];
	mask = (1<<romindex);
	pW  = FindWndID(id);
	pW->flags &= ~CF_DISABLED;	// enable the ROM checkbox by default
	if( RomMUST[ROMD_Model] & mask ) ROMD_RomLoads[ROMD_Model] |= mask;	// make sure it's checked if it's built-in to this model
	if( RomCANT[ROMD_Model] & mask ) ROMD_RomLoads[ROMD_Model] &= ~mask;	// make sure it's NOT checked if it doesn't work in this model
	SetCheckID(id, ROMD_RomLoads[ROMD_Model] & (1<<romindex));
	if( (RomCANT[ROMD_Model] & mask) || (RomMUST[ROMD_Model] & mask) ) pW->flags |= CF_DISABLED;	// disable the control if it's illegal this ROM to be added or removed from this model
	SendMsg(pW, DLG_DRAW, 0, 0, 0);
}

///*****************************************
void SetROMDctls() {

	DWORD	i;
	WND	*pWnd;

	for(i=0; i<RomListCnt; i++) SetROMctl(i);

	// set appropriate MODEL checked
	pWnd = FindWndID(ID_MODEL_85A);
	if( pWnd ) pWnd->flags = (pWnd->flags & ~CF_CHECKED) | (ROMD_Model==0 ? CF_CHECKED : 0);
	SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
	pWnd = FindWndID(ID_MODEL_85B);
	if( pWnd ) pWnd->flags = (pWnd->flags & ~CF_CHECKED) | (ROMD_Model==1 ? CF_CHECKED : 0);
	SendMsg(pWnd, DLG_DRAW, 0, 0, 0);
	pWnd = FindWndID(ID_MODEL_87);
	if( pWnd ) pWnd->flags = (pWnd->flags & ~CF_CHECKED) | (ROMD_Model==2 ? CF_CHECKED : 0);
	SendMsg(pWnd, DLG_DRAW, 0, 0, 0);

	// if 85B or 87, force 32K CPU RAM and disable 16K, else enable 16K
	if( ROMD_Model>0 ) {
		ROMD_RamSizes[ROMD_Model] = 32;
		FindWndID(ID_ROMD_16)->flags |= CF_DISABLED;
	} else FindWndID(ID_ROMD_16)->flags &= ~CF_DISABLED;
	SetCheckID(ID_ROMD_16, ROMD_RamSizes[ROMD_Model]==16);
	SetCheckID(ID_ROMD_32, ROMD_RamSizes[ROMD_Model]==32);
	SendMsgID(ID_ROMD_16, DLG_DRAW, 0, 0, 0);
	SendMsgID(ID_ROMD_32, DLG_DRAW, 0, 0, 0);

	for(i=0; i<7; i++) {
		SetCheckID(ID_ROMD_0E+i, ROMD_RamSizeExt[ROMD_Model]==i);
		SendMsgID(ID_ROMD_0E+i, DLG_DRAW, 0, 0, 0);
	}
}

///*****************************************
void GetROMDctls() {

	DWORD	i;

	ROMD_RomLoads[ROMD_Model] = 0;
	for(i=0; i<RomListCnt; i++) if( GetCheckID(ROMctlID[i]) ) ROMD_RomLoads[ROMD_Model] |= (1<<i);
	ROMD_RamSizes[ROMD_Model] = GetCheckID(ID_ROMD_16) ? 16 : 32;
	for(i=0; i<7; i++) {
		if( GetCheckID(ID_ROMD_0E+i) ) {
			ROMD_RamSizeExt[ROMD_Model] = i;
			break;
		}
	}
}

///*****************************************
DWORD ModelRadioButtonProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
//	 case DLG_SETFOCUS:
		RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
	 	if( pWnd->id - ID_MODEL_85A != ROMD_Model ) {	// changing models
			GetROMDctls();	// get current values into temp vars for current model
			ROMD_Model = pWnd->id - ID_MODEL_85A;
			SetROMDctls();	// change the current values to the temp vars for the new model
	 	}
		return 0;
/*	 case DLG_LBUTTDOWN:
		// if already selected, consider this a double-click and load the tape on the upclick
		TapeDoubleID = ( pWnd->flags & CF_CHECKED ) ? pWnd->id : 0;
		break;
	 case DLG_LBUTTUP:
		if( pWnd->id == TapeDoubleID ) {
			SendMsg(FindWndID(ID_TAPD_CLOSE), DLG_ACTIVATE, 0, 0, 0);
			return 0;
		}
		break;
*/
	}
	return RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD ROMDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	int		i;//, c;

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	/// figure out if devices have changed
		///	Any modifications (via the DEVICES dialog) to the existing devices (sc,ca) have been made to (wsc,wca), leaving (sc,ca) untouched for SHUTDOWN code.
		///	The iR_WRITE_INI routines will write out their DEV= entry using (wsc,wca), so subsequent iR_READ_INI will put them at their new location.
/*		for(i=c=0; i<MAX_DEVICES; i++) {
			if( DEVD_DEV[i].state==DEVSTATE_NEW || DEVD_DEV[i].state==DEVSTATE_DELETED ) {
				c = 1;
			}
		}
*/
		memcpy(&Devices, &DEVD_DEV, sizeof(Devices));

	 	GetROMDctls();

		Model = ROMD_Model;
		for(i=0; i<3; i++) {
			RomLoads[i] = ROMD_RomLoads[i];
			RamSizeExt[i] = ROMD_RamSizeExt[i];
			RamSizes[i] = ROMD_RamSizes[i];
		}
		RomLoad = RomLoads[Model];
		RamSize = RamSizes[Model];

		ActiveDlg = NULL;

		if( !SetupDispVars() ) {
			SystemMessage("Failed to load IMAGES\\*.BMP files");
			KillProgram();
			return 0;
		}
		HP85PWO(FALSE);
//		if( c ) {
			DlgNoCap = TRUE;	// tell KWnds[] to adjust for TitleBar
			HP85OnShutdown();
			DlgNoCap = FALSE;
			HP85OnStartup();
//		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT ROMDwarn1TEXT = {ID_ROMD_NOCARE, CTL_TEXT,8,8, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"WARNING: Changing these settings"};
TEXT ROMDwarn2TEXT = {ID_ROMD_NOCARE, CTL_TEXT,8,20, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"will reset/reboot the emulated"};
TEXT ROMDwarn3TEXT = {ID_ROMD_NOCARE, CTL_TEXT,8,32, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"85/87 causing the loss of all"};
TEXT ROMDwarn4TEXT = {ID_ROMD_NOCARE, CTL_TEXT,8,44, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"programs and data in 85/87 RAM."};
GROUPBOX ROMDmodelGROUP = {ID_MODEL_GROUP, CTL_GROUP,8, 70, 286, 36,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Series 80 Model to emulate "};
RADIO ROMD85aRADIO = {ID_MODEL_85A, CTL_RADIO,16, 84, 87, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&ModelRadioButtonProc,0,&KWnds[0],"HP-85A"};
RADIO ROMD85bRADIO = {ID_MODEL_85B, CTL_RADIO,103, 84, 87, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&ModelRadioButtonProc,0,&KWnds[0],"HP-85B"};
RADIO ROMD87RADIO = {ID_MODEL_87, CTL_RADIO,190, 84, 87, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&ModelRadioButtonProc,0,&KWnds[0],"HP-87"};
GROUPBOX ROMDromGROUP = {ID_ROMD_ROMGROUP, CTL_GROUP,8,114, 286, 240,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Option ROMs present"};
CHECK ROMDgraphCHECK = {ID_ROMD_GRAPHICS, CTL_CHECK,16,128, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"(001) HP-87 Graphics"};
CHECK ROMDpdCHECK = {ID_ROMD_PD, CTL_CHECK,16,140, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"(010) Program Development"};
CHECK ROMDmiksamCHECK = {ID_ROMD_MIKSAM, CTL_CHECK,16,152, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"(016) MIKSAM (87)"};
CHECK ROMDlangCHECK = {ID_ROMD_LANG, CTL_CHECK,16,164, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"(030) Langage (87)"};
CHECK ROMDextCHECK = {ID_ROMD_EXT, CTL_CHECK,16,176, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(047) E. Kaser's ExtRom (87)"};
CHECK ROMDasmCHECK = {ID_ROMD_ASM, CTL_CHECK,16,188, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(050) Assembler"};
CHECK ROMDstructCHECK = {ID_ROMD_STRUCT, CTL_CHECK,16,200, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(070) A. Koppel's SYSEXT (87)"};
CHECK ROMDforCHECK = {ID_ROMD_FOR, CTL_CHECK,16,212, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(250) Forth"};
CHECK ROMDmatCHECK = {ID_ROMD_MAT, CTL_CHECK,16,224, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&MatrixProc,0,&KWnds[0],"(260) Matrix"};
CHECK ROMDmat2CHECK = {ID_ROMD_MAT2, CTL_CHECK,16,236, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&MatrixProc,0,&KWnds[0],"(261) Matrix 2 (87)"};
CHECK ROMDioCHECK = {ID_ROMD_IO, CTL_CHECK,16,248, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(300) I/O"};
CHECK ROMDemsCHECK = {ID_ROMD_EMS, CTL_CHECK,16,260, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(317) Extended Mass Storage"};
CHECK ROMDmsCHECK = {ID_ROMD_MS, CTL_CHECK,16,272, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(320) Mass Storage"};
CHECK ROMDedCHECK = {ID_ROMD_ED, CTL_CHECK,16,284, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(321) Electronic Disk"};
CHECK ROMDservCHECK = {ID_ROMD_SERV, CTL_CHECK,16,296, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(340) Service (85)"};
CHECK ROMDap2CHECK = {ID_ROMD_AP2, CTL_CHECK,16,308, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&APProc,0,&KWnds[0],"(347) Adv Programming 2 (87)"};
CHECK ROMDapCHECK = {ID_ROMD_AP, CTL_CHECK,16,320, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&APProc,0,&KWnds[0],"(350) Advanced Programming"};
CHECK ROMDppCHECK = {ID_ROMD_PP, CTL_CHECK,16,332, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"(360) Plotter/Printer"};
GROUPBOX ROMDramGROUP = {ID_ROMD_RAMGROUP, CTL_GROUP,8,362, 286, 146,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," RAM present "};
TEXT ROMDcpuramTEXT = {ID_ROMD_NOCARE, CTL_TEXT,16, 376, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"CPU RAM"};
RADIO ROMD16RADIO = {ID_ROMD_16, CTL_RADIO,24, 388, 127, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RadioButtonProc,0,&KWnds[0],"16 Kbytes"};
RADIO ROMD32RADIO = {ID_ROMD_32, CTL_RADIO,151,388, 127, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"32 Kbytes"};
TEXT ROMDextramTEXT = {ID_ROMD_NOCARE, CTL_TEXT,16, 406, 286, 12,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"EXTENDED RAM"};
RADIO ROMD0eRADIO = {ID_ROMD_0E, CTL_RADIO,24, 418, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RadioButtonProc,0,&KWnds[0],"None"};
RADIO ROMD32eRADIO = {ID_ROMD_32E, CTL_RADIO,24, 430, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"32 Kbytes"};
RADIO ROMD64eRADIO = {ID_ROMD_64E, CTL_RADIO,24, 442, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"64 Kbytes"};
RADIO ROMD128eRADIO = {ID_ROMD_128E, CTL_RADIO,24, 454, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"128 Kbytes"};
RADIO ROMD256eRADIO = {ID_ROMD_256E, CTL_RADIO,24, 466, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"256 Kbytes"};
RADIO ROMD512eRADIO = {ID_ROMD_512E, CTL_RADIO,24, 478, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"512 Kbytes"};
RADIO ROMD1024eRADIO = {ID_ROMD_1024E, CTL_RADIO,24, 490, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"1024 Kbytes"};
BUTT ROMDokBUTT = {ID_ROMD_OK, CTL_BUTT,8,514, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&ROMDokProc,0,&KWnds[0],"OK"};
BUTT ROMDcancelBUTT = {ID_ROMD_CANCEL, CTL_BUTT,84,514, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
BUTT ROMDdevsBUTT = {ID_ROMD_DEVICES, CTL_BUTT,222,514, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&ROMDdevicesProc,0,&KWnds[0],"Devices"};
DLG RomDlg = {ID_ROMD, CTL_DLG,2, 2, 302, 542,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *ROMD_CTLs[] = {&ROMDwarn1TEXT, &ROMDwarn2TEXT, &ROMDwarn3TEXT, &ROMDwarn4TEXT,&ROMDmodelGROUP, &ROMD85aRADIO, &ROMD85bRADIO, &ROMD87RADIO,&ROMDromGROUP,
	&ROMDgraphCHECK, &ROMDpdCHECK, &ROMDmiksamCHECK, &ROMDlangCHECK, &ROMDextCHECK, &ROMDasmCHECK, &ROMDstructCHECK, &ROMDforCHECK,&ROMDmatCHECK,
	&ROMDmat2CHECK, &ROMDioCHECK, &ROMDemsCHECK, &ROMDmsCHECK, &ROMDedCHECK, &ROMDservCHECK, &ROMDap2CHECK, &ROMDapCHECK,&ROMDppCHECK,&ROMDramGROUP,
	&ROMDcpuramTEXT, &ROMD16RADIO, &ROMD32RADIO,&ROMDextramTEXT, &ROMD0eRADIO, &ROMD32eRADIO, &ROMD64eRADIO, &ROMD128eRADIO, &ROMD256eRADIO, &ROMD512eRADIO,
	&ROMD1024eRADIO,&ROMDokBUTT, &ROMDcancelBUTT, &ROMDdevsBUTT
};

///*********************************************************************
void CreateRomDlg() {

	int		i, j;

	// copy actual values to working arrays in case of CANCEL
	ROMD_Model = Model;
	for(i=0; i<3; i++) {
		ROMD_RomLoads[i] = RomLoads[i];
		ROMD_RamSizes[i] = RamSizes[i];
		ROMD_RamSizeExt[i] = RamSizeExt[i];
	}

	CreateDlg(&RomDlg, ROMD_CTLs, sizeof(ROMD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	// copy current devices into 'working' DEVD_DEV[] (in case of CANCEL)
	for(i=j=0; i<MAX_DEVICES; i++) {
		DEVD_DEV[i].devType = DEVTYPE_NONE;
		for(; j<MAX_DEVICES; j++) {
			if( Devices[j].devType!=DEVTYPE_NONE ) {
				memcpy(&DEVD_DEV[i], &Devices[j], sizeof(DEV));
				DEVD_DEV[i].wsc = DEVD_DEV[i].sc;	// copy current SC to 'write' (possibly new) SC
				DEVD_DEV[i].wca = DEVD_DEV[i].ca;	// copy current CA to 'write' (possibly new) CA
				++j;
				break;
			}
		}
	}

	SetROMDctls();
	ChangeFocus(FindWndID(ID_MODEL_85A+Model));

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD OPTDdefaultProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
		SetCheckID(ID_OPTD_DOUBLE, FALSE);
		SetCheckID(ID_OPTD_AUTORUN, FALSE);
		SetCheckID(ID_OPTD_NATURAL, FALSE);

		SetCheckID(ID_OPTD_8RADIO, TRUE);
		SetCheckID(ID_OPTD_10RADIO, FALSE);
		SetCheckID(ID_OPTD_16RADIO, FALSE);

		sprintf(((EDIT *)FindWndID(ID_OPTD_MAINCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MAINBACK);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MAINTEXTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MAINTEXT);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MENUSELBACKCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MENUSELBACK);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MENUSELTEXTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MENUSELTEXT);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MENUBACKCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MENUBACK);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MENUTEXTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MENUTEXT);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MENUHICLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MENUHI);
		sprintf(((EDIT *)FindWndID(ID_OPTD_MENULOCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_MENULO);
		sprintf(((EDIT *)FindWndID(ID_OPTD_PRINTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)CLR_PRINT);
		SendMsg(FindWndID(ID_OPTD), DLG_DRAW, 0, 0, 0);
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD OPTDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	int				i;
	unsigned int	clr;
	DWORD			oldflags;

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	oldflags = CfgFlags;
		CfgFlags &= ~(CFG_BIGCRT | CFG_AUTORUN | CFG_NATURAL_SPEED | CFG_3INCH_DRIVE);

		if( GetCheckID(ID_OPTD_DOUBLE) ) CfgFlags |= CFG_BIGCRT;
		if( !GetCheckID(ID_OPTD_AUTORUN) ) CfgFlags |= CFG_AUTORUN;
		if( GetCheckID(ID_OPTD_NATURAL) ) CfgFlags |= CFG_NATURAL_SPEED;
		if( GetCheckID(ID_OPTD_3INCH) ) CfgFlags |= CFG_3INCH_DRIVE;

		if( GetCheckID(ID_OPTD_16RADIO) ) base = 16;
		else if( GetCheckID(ID_OPTD_10RADIO) ) base = 10;
		else base = 8;

		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MAINCLR_EDIT))->Text, "%x", &clr) ) MainWndClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MAINTEXTCLR_EDIT))->Text, "%x", &clr) ) MainWndTextClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MENUSELBACKCLR_EDIT))->Text, "%x", &clr) ) MenuSelBackClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MENUSELTEXTCLR_EDIT))->Text, "%x", &clr) ) MenuSelTextClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MENUBACKCLR_EDIT))->Text, "%x", &clr) ) MenuBackClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MENUTEXTCLR_EDIT))->Text, "%x", &clr) ) MenuTextClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MENUHICLR_EDIT))->Text, "%x", &clr) ) MenuHiClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_MENULOCLR_EDIT))->Text, "%x", &clr) ) MenuLoClr = (clr & 0x00FFFFFF);
		if( 1==sscanf(((EDIT *)FindWndID(ID_OPTD_PRINTCLR_EDIT))->Text, "%x", &clr) ) PrintClr = (clr & 0x00FFFFFF);

		ActiveDlg = NULL;

		if( !SetupDispVars() ) KillProgram();
		else DrawAll();
		if( (oldflags ^ CfgFlags) & CFG_BIGCRT ) {
			for(clr=0; clr<8; clr++) {
				if( IOcards[clr].initProc != NULL && IOcards[clr].type==IOTYPE_HPIB )  {	// if card is present and is HPIB card
					for(i=0; i<31; i++) {
						AdjustDisks(clr, i);
						DrawDisks(clr, i);
					}
				}
			}
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
GROUPBOX OPTDoptGROUP = {ID_OPTD_OPTGROUP, CTL_GROUP,					  8,  8, 306, 56,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Emulator Options "};
CHECK OPTDdoubleCHECK = {ID_OPTD_DOUBLE, CTL_CHECK,						 16, 20, 290, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&CheckProc,0,&KWnds[0],"Double-&size emulated machine"};
CHECK OPTDautorunCHECK = {ID_OPTD_AUTORUN, CTL_CHECK,					 16, 32, 290, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"Start in debug mode"};
CHECK OPTDnaturalCHECK = {ID_OPTD_NATURAL, CTL_CHECK,					 16, 44, 290, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CheckProc,0,&KWnds[0],"Run at natural (slower) speed"};
GROUPBOX OPTDbaseGROUP = {ID_OPTD_BASEGROUP, CTL_GROUP,					  8, 72, 306, 56,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Displayed Number Base "};
RADIO OPTD8RADIO = {ID_OPTD_8RADIO, CTL_RADIO,							 24, 84, 290, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RadioButtonProc,0,&KWnds[0],"&OCTAL (base 8)"};
RADIO OPTD10RADIO = {ID_OPTD_10RADIO, CTL_RADIO,						 24, 96, 290, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&DECIMAL (base 10)"};
RADIO OPTD16RADIO = {ID_OPTD_16RADIO, CTL_RADIO,						 24,108, 290, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RadioButtonProc,0,&KWnds[0],"&HEXIDECIMAL (base 16)"};
GROUPBOX OPTDmainClrGROUP = {ID_OPTD_COLORS_GROUP, CTL_GROUP,			  8,134, 306,170,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," RGB Colors (00BBGGRR) "};
TEXT OPTmainClrTEXT = {ID_OPTD_MAINCLR_TEXT, CTL_TEXT,					 16,150, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Main window:"};
EDIT OPTDmainClrEDIT = {ID_OPTD_MAINCLR_EDIT, CTL_EDIT,					170,146, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmainTextClrTEXT = {ID_OPTD_MAINTEXTCLR_TEXT, CTL_TEXT,			 16,166, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Main window text:"};
EDIT OPTDmainTextClrEDIT = {ID_OPTD_MAINTEXTCLR_EDIT, CTL_EDIT,			170,162, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmenuBackClrTEXT = {ID_OPTD_MENUBACKCLR_TEXT, CTL_TEXT,			 16,182, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Menu background:"};
EDIT OPTDmenuBackClrEDIT = {ID_OPTD_MENUBACKCLR_EDIT, CTL_EDIT,			170,178, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmenuTextClrTEXT = {ID_OPTD_MENUTEXTCLR_TEXT, CTL_TEXT,			 16,198, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Menu normal text:"};
EDIT OPTDmenuTextClrEDIT = {ID_OPTD_MENUTEXTCLR_EDIT, CTL_EDIT,			170,194, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmenuSelBackClrTEXT = {ID_OPTD_MENUSELBACKCLR_TEXT, CTL_TEXT,	 16,214, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Menu selected back:"};
EDIT OPTDmenuSelBackClrEDIT = {ID_OPTD_MENUSELBACKCLR_EDIT, CTL_EDIT,	170,210, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmenuSelTextClrTEXT = {ID_OPTD_MENUSELTEXTCLR_TEXT, CTL_TEXT,	 16,230, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Menu selected text:"};
EDIT OPTDmenuSelTextClrEDIT = {ID_OPTD_MENUSELTEXTCLR_EDIT, CTL_EDIT,	170,226, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmenuHiClrTEXT = {ID_OPTD_MENUHICLR_TEXT, CTL_TEXT,				 16,246, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Menu high-light:"};
EDIT OPTDmenuHiClrEDIT = {ID_OPTD_MENUHICLR_EDIT, CTL_EDIT,				170,242, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTmenuLoClrTEXT = {ID_OPTD_MENULOCLR_TEXT, CTL_TEXT,				 16,262, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Menu low-light:"};
EDIT OPTDmenuLoClrEDIT = {ID_OPTD_MENULOCLR_EDIT, CTL_EDIT,				170,258, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
TEXT OPTprintClrTEXT = {ID_OPTD_PRINTCLR_TEXT, CTL_TEXT,				 16,278, 150, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Thermal print:"};
EDIT OPTDprintClrEDIT = {ID_OPTD_PRINTCLR_EDIT, CTL_EDIT,				170,274, 128, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&EditProc,0,&KWnds[0]};
BUTT OPTDokBUTT = {ID_OPTD_OK, CTL_BUTT,								 43,314,  64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&OPTDokProc,0,&KWnds[0],"OK"};
BUTT OPTDcancelBUTT = {ID_OPTD_CANCEL, CTL_BUTT,						111,314,  64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
BUTT OPTDdefaultBUTT = {ID_OPTD_DEFAULT, CTL_BUTT,						187,314,  80, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&OPTDdefaultProc,0,&KWnds[0],"Defaults"};
DLG OptionsDlg = {ID_OPTD, CTL_DLG,										  2,  2, 322,340,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};

void *OPTD_CTLs[] = {&OPTDoptGROUP,&OPTDdoubleCHECK,&OPTDautorunCHECK,&OPTDnaturalCHECK,&OPTDbaseGROUP, &OPTD8RADIO, &OPTD10RADIO, &OPTD16RADIO,
	&OPTDmainClrGROUP, &OPTmainClrTEXT, &OPTDmainClrEDIT, &OPTmainTextClrTEXT, &OPTDmainTextClrEDIT,&OPTmenuBackClrTEXT, &OPTDmenuBackClrEDIT, &OPTmenuTextClrTEXT,
	&OPTDmenuTextClrEDIT, &OPTmenuSelBackClrTEXT, &OPTDmenuSelBackClrEDIT,&OPTmenuSelTextClrTEXT, &OPTDmenuSelTextClrEDIT, &OPTmenuHiClrTEXT, &OPTDmenuHiClrEDIT,
	&OPTmenuLoClrTEXT, &OPTDmenuLoClrEDIT, &OPTprintClrTEXT, &OPTDprintClrEDIT,&OPTDokBUTT, &OPTDcancelBUTT, & OPTDdefaultBUTT
};

///*********************************************************************
void CreateOptionsDlg() {

	CreateDlg(&OptionsDlg, OPTD_CTLs, sizeof(OPTD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	SetCheckID(ID_OPTD_DOUBLE, CfgFlags & CFG_BIGCRT);
	SetCheckID(ID_OPTD_AUTORUN, !(CfgFlags & CFG_AUTORUN));
	SetCheckID(ID_OPTD_NATURAL, CfgFlags & CFG_NATURAL_SPEED);
	SetCheckID(ID_OPTD_3INCH, CfgFlags & CFG_3INCH_DRIVE);

	SetCheckID(ID_OPTD_8RADIO, base==8);
	SetCheckID(ID_OPTD_10RADIO, base==10);
	SetCheckID(ID_OPTD_16RADIO, base==16);

	sprintf(((EDIT *)FindWndID(ID_OPTD_MAINCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MainWndClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MAINTEXTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MainWndTextClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MENUSELBACKCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MenuSelBackClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MENUSELTEXTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MenuSelTextClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MENUBACKCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MenuBackClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MENUTEXTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MenuTextClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MENUHICLR_EDIT))->Text, "0x%8.8X", (unsigned int)MenuHiClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_MENULOCLR_EDIT))->Text, "0x%8.8X", (unsigned int)MenuLoClr);
	sprintf(((EDIT *)FindWndID(ID_OPTD_PRINTCLR_EDIT))->Text, "0x%8.8X", (unsigned int)PrintClr);
// NOTE: if you add any new options here, you must also set them to their defaults in OPTDdefaultProc() below and in OPTDokProc()

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*****************************************************************************
///*****************************************************************************
///*****************************************************************************
///*****************************************************************************
///*********************************************************************
DWORD EVDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pE;
	long		r, s;
	DWORD		v;

	switch( msg ) {
	 case DLG_ACTIVATE:
//		if( GetCheckID(ID_VAD_SYS) ) SelectROM(0000);

		// save address, set DisStart to it, DrawAll
		pE = (EDIT *)FindWndID(ID_EVD_VALEDIT);
		if( base==8 ) r = sscanf(pE->Text, "%lo", &v);
		else if( base==10 ) r = sscanf(pE->Text, "%ld", &v);
		else if( base==16 ) r = sscanf(pE->Text, "0x%lx", &v);
		else r = 0;
		if( r ) {
			if( EVDinput<0200 ) {	// CPU register
				s = EVDinput & 0100;
				EVDinput &= 077;
				if( s ) {	// if shift was down (ie, was multibyte register)
					if( EVDinput<040 ) {
						if( EVDinput & 1 ) v = Reg[EVDinput];
						else {
							Reg[EVDinput+0] = (BYTE)(v & 0x00FF);
							Reg[EVDinput+1] = (BYTE)((v>>8)&0x00FF);
						}
					} else if( (EVDinput&7)<4 ) {
						Reg[EVDinput+0] = (BYTE)(v & 0x00FF);
						Reg[EVDinput+1] = (BYTE)((v>>8)&0x00FF);
					} else if( (EVDinput&7)==4 ) {
						Reg[EVDinput+0] = (BYTE)(v & 0x00FF);
						Reg[EVDinput+1] = (BYTE)((v>>8)&0x00FF);
						Reg[EVDinput+2] = (BYTE)((v>>16)&0x00FF);
						Reg[EVDinput+3] = (BYTE)((v>>24)&0x00FF);
					} else if( (EVDinput&7)==5 ) {
						Reg[EVDinput+0] = (BYTE)(v & 0x00FF);
						Reg[EVDinput+1] = (BYTE)((v>>8)&0x00FF);
						Reg[EVDinput+2] = (BYTE)((v>>16)&0x00FF);
					} else if( (EVDinput&7)==6 ) {
						Reg[EVDinput+0] = (BYTE)(v & 0x00FF);
						Reg[EVDinput+1] = (BYTE)((v>>8)&0x00FF);
					} else Reg[EVDinput] = (BYTE)v;
				} else Reg[EVDinput] = (BYTE)v;
			} else if( EVDinput==0200 ) Drp = (WORD)(v&0x3F);
			else if( EVDinput==0201 ) Arp =  (WORD)(v&0x3F);
			else if( EVDinput==0202 ) E =  (BYTE)(v&0x0F);
		}
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT EVDvalTEXT = {ID_EVD_VALTEXT, CTL_TEXT,16,164, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"&Value:"};
EDIT EVDvalEDIT = {ID_EVD_VALEDIT, CTL_EDIT,84,160, 72, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0]};
BUTT EVDokBUTT = {ID_EVD_OK, CTL_BUTT,166,160, 60, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&EVDokProc,0,&KWnds[0],"OK"};
BUTT EVDcancelBUTT = {ID_EVD_CANCEL, CTL_BUTT,230,160, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
DLG EditValDlg = {ID_EVD, CTL_DLG,2, 2, 302, 194,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *EVD_CTLs[] = {&EVDvalTEXT, &EVDvalEDIT,&EVDokBUTT, &EVDcancelBUTT};

///*****************************************************************************
DWORD EditReg(DWORD r, DWORD v) {

	char	tmp[8];

	if( base==8 ) sprintf(tmp, "R%o", (unsigned int)r);
	else if( base==10 ) sprintf(tmp, "R%d", (int)r);
	else if( base==16 ) sprintf(tmp, "R%X", (unsigned int)r);

	EVDinput = r;	// save for storing edited value

	CreateDlg(&EditValDlg, EVD_CTLs, sizeof(EVD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

// GEEKDO: add base formatting for 10 and 16 and 1-4 bytes word-size
	sprintf(((EDIT *)FindWndID(ID_EVD_VALEDIT))->Text, "%o", (unsigned int)v);

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);

	return 0;
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************************
DWORD TAPDwpProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	long	i;
	RADIO	*pR;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pWnd->flags ^= CF_CHECKED;	// toggle check
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);

		for(i=0; i<MAX_TAPES+1; i++) if( GetCheckID(ID_TAPD_TAPE0RADIO+i) ) break;
		if( i>0 && i<=MAX_TAPES ) {
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO+i);
			if( pR ) SetTapeWriteProtect(pR->pText, pWnd->flags & CF_CHECKED);
		}
		return 0;
	}
	return CheckProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
void SetWriteProtect() {

	long	i;
	RADIO	*pR;

	for(i=0; i<MAX_TAPES+1; i++) if( GetCheckID(ID_TAPD_TAPE0RADIO+i) ) break;
	if( i>0 && i<=MAX_TAPES ) {
		pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO+i);
		if( pR ) SetCheckID(ID_TAPD_WP, GetTapeWriteProtect(pR->pText));
	}
}

///***********************************************************************
DWORD RenOldProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_CHAR:
		// can't type into or modify OLD tape name!
		return 0;
	 case DLG_KEYDOWN:
		switch( arg1 ) {
		 case VK_BACK:
		 case VK_DELETE:
		 case 'X':
		 case 'V':
		 case 'C':
			// can't edit or modify OLD tape name!
			return 0;
		}
		break;;
	}
	return EditProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD RENDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT	*pO, *pN;
	long	f;
	char	ctape[64];

	switch( msg ) {
	 case DLG_ACTIVATE:
		pO = (EDIT *)FindWndID(ID_REND_OLDEDIT);
		pN = (EDIT *)FindWndID(ID_REND_NEWEDIT);

		if( pN->Text[0]==0 ) {		// don't have a name
			SystemMessage("Must CANCEL or enter a name!");
			return 0;
		}
		if( strcmp(pN->Text, pO->Text) ) {	// different old and new names, actually something to do!
			sprintf((char*)ScratBuf, "TAPES\\%s", pN->Text);
			if( -1!=(f=Kopen((char*)ScratBuf, O_RDBIN)) ) {	// that tape name already exists
				Kclose(f);
				SystemMessage("Sorry, that tape name is already used!");
				return 0;
			}
			if( pO->Text[0]==0 ) {	// no old name, just naming a new tape
				if( CurTape[0] ) {		// there's an old tape loaded
					strcpy(ctape, CurTape);	// save old tape name
					EjectTape();			// eject it
					NewTape();				// create and load a new tape
					strcpy(CurTape, pN->Text);	// name it
					WriteTape();			// write it to disk
					strcpy(CurTape, ctape);	// restore old tape name
					LoadTape();				// reload it
				} else {				// no tape loaded
					NewTape();				// create and load a new tape
					strcpy(CurTape, pN->Text);	// name it
					WriteTape();			// write it to disk
					EjectTape();			// eject it
					LoadTape();				// load it
				}
			} else {				// renaming old tape
				sprintf((char*)ScratBuf, "TAPES\\%s", pO->Text);
				sprintf((char*)ScratBuf+256, "TAPES\\%s", pN->Text);
				Krename((char*)ScratBuf, (char*)ScratBuf+256);
				if( !strcmp(CurTape, pO->Text) ) strcpy(CurTape, pN->Text);
			}
		}
		ActiveDlg = NULL;
		DrawAll();
		CreateTapeDlg();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD RENDcancelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
		ActiveDlg = NULL;
		DrawAll();
		CreateTapeDlg();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT RENDoldTEXT = {ID_REND_OLDTEXT, CTL_TEXT,8,12, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Old name:"};
EDIT RENDoldEDIT = {ID_REND_OLDEDIT, CTL_EDIT,84, 8, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RenOldProc,0,&KWnds[0],};
TEXT RENDnewTEXT = {ID_REND_NEWTEXT, CTL_TEXT,8,30, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"New name:"};
EDIT RENDnewEDIT = {ID_REND_NEWEDIT, CTL_EDIT,84,26, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0],};
BUTT RENDokBUTT = {ID_REND_OK, CTL_BUTT,83,46, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&RENDokProc,0,&KWnds[0],"OK"};
BUTT RENDcancelBUTT = {ID_REND_CANCEL, CTL_BUTT,155,46, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RENDcancelProc,0,&KWnds[0],"Cancel"};
DLG RenameTapeDlg = {ID_REND, CTL_DLG,2, 2, 302, 92,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *REND_CTLs[] = {&RENDoldTEXT, &RENDoldEDIT,&RENDnewTEXT, &RENDnewEDIT,&RENDokBUTT, &RENDcancelBUTT};

///*********************************************************************
void CreateRenameTapeDlg(char *pOld) {

	EDIT	*pE;

	CreateDlg(&RenameTapeDlg, REND_CTLs, sizeof(REND_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	pE = (EDIT *)FindWndID(ID_REND_OLDEDIT);
	if( pE != NULL ) strcpy(pE->Text, pOld);

	pE = (EDIT *)FindWndID(ID_REND_NEWEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
	}

// GEEKDO: set TextLim on EDIT fields!!!

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
DWORD TAPDcloseProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO	*pR;
	long	i;

	switch( msg ) {
	 case DLG_ACTIVATE:

		for(i=0; i<MAX_TAPES+1; i++) if( GetCheckID(ID_TAPD_TAPE0RADIO+i) ) break;

		if( i==0 || i>MAX_TAPES ) {	// Eject cartridge
			EjectTape();
			CurTape[0] = 0;
		} else {					// Insert cartridge
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO + i);
			if( pR && strcmp(CurTape, pR->pText) ) {	// different tape than what's currently in
				if( (IO_TAPSTS & 1) ) EjectTape();	// Eject current tape
				strcpy(CurTape, pR->pText);
				LoadTape();
			}
		}
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD TAPDdeleteProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO		*pR;
	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<MAX_TAPES+1; i++) if( GetCheckID(ID_TAPD_TAPE0RADIO+i) ) break;

		if( i>0 && i<=MAX_TAPES ) {
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO+i);
			if( pR != NULL && pR->pText[0] ) {
				if( !strcmp(CurTape, pR->pText) ) {
					EjectTape();	// if deleting the currently loaded tape, eject it first
					CurTape[0] = 0;
				}

				sprintf((char*)ScratBuf, "TAPES\\%s", pR->pText);
				Kdelete((char*)ScratBuf);

				ActiveDlg = NULL;	// "destroy" tape dialog
				CreateTapeDlg();	// recreate it without deleted tape
			}
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD TAPDnewProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO		*pR;
	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<MAX_TAPES+1; i++) {
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO+i);
			if( pR && pR->pText[0]==0 ) break;
		}
		if( i>0 && i<=MAX_TAPES ) {
			ActiveDlg = NULL;
			DrawAll();
			CreateRenameTapeDlg("");
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD TAPDrenameProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO		*pR;
	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<MAX_TAPES+1; i++) if( GetCheckID(ID_TAPD_TAPE0RADIO+i) ) break;

		if( i>0 && i<=MAX_TAPES ) {
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO+i);
			if( pR != NULL && pR->pText[0] ) {
				ActiveDlg = NULL;	// "destroy" tape dialog
				DrawAll();
				CreateRenameTapeDlg(pR->pText);	// rename this file
			}
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD TAPDradioProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO	*pR;

	pR = (RADIO *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		if( pR->pText[0]==0 ) return 0;	// don't draw if no tape name here
		break;
	 case DLG_ACTIVATE:
	 case DLG_SETFOCUS:
		RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
		SetWriteProtect();
		return 0;
	 case DLG_LBUTTDOWN:
		// if already selected, consider this a double-click and load the tape on the upclick
		TapeDoubleID = ( pWnd->flags & CF_CHECKED ) ? pWnd->id : 0;
		break;
	 case DLG_LBUTTUP:
		if( pWnd->id == TapeDoubleID ) {
			SendMsg(FindWndID(ID_TAPD_CLOSE), DLG_ACTIVATE, 0, 0, 0);
			return 0;
		}
		break;
	}
	return RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
GROUPBOX TAPDtapeGROUP = {ID_TAPD_TAPEGROUP, CTL_GROUP,8,8, 286, 390,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Current Tape "};
RADIO TAPDtape0RADIO = {ID_TAPD_TAPE0RADIO, CTL_RADIO,16, 20, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&TAPDradioProc,0,&KWnds[0],"NO TAPE IN DRIVE"};
RADIO TAPDtape1RADIO = {ID_TAPD_TAPE1RADIO, CTL_RADIO,21, 34, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[0]};
RADIO TAPDtape2RADIO = {ID_TAPD_TAPE2RADIO, CTL_RADIO,21, 46, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[1]};
RADIO TAPDtape3RADIO = {ID_TAPD_TAPE3RADIO, CTL_RADIO,21, 58, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[2]};
RADIO TAPDtape4RADIO = {ID_TAPD_TAPE4RADIO, CTL_RADIO,21, 70, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[3]};
RADIO TAPDtape5RADIO = {ID_TAPD_TAPE5RADIO, CTL_RADIO,21, 82, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[4]};
RADIO TAPDtape6RADIO = {ID_TAPD_TAPE6RADIO, CTL_RADIO,21, 94, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[5]};
RADIO TAPDtape7RADIO = {ID_TAPD_TAPE7RADIO, CTL_RADIO,21,106, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[6]};
RADIO TAPDtape8RADIO = {ID_TAPD_TAPE8RADIO, CTL_RADIO,21,118, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[7]};
RADIO TAPDtape9RADIO = {ID_TAPD_TAPE9RADIO, CTL_RADIO,21,130, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[8]};
RADIO TAPDtape10RADIO= {ID_TAPD_TAPE10RADIO,CTL_RADIO,21,142, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[9]};
RADIO TAPDtape11RADIO= {ID_TAPD_TAPE11RADIO,CTL_RADIO,21,154, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[10]};
RADIO TAPDtape12RADIO= {ID_TAPD_TAPE12RADIO,CTL_RADIO,21,166, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[11]};
RADIO TAPDtape13RADIO= {ID_TAPD_TAPE13RADIO,CTL_RADIO,21,178, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[12]};
RADIO TAPDtape14RADIO= {ID_TAPD_TAPE14RADIO,CTL_RADIO,21,190, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[13]};
RADIO TAPDtape15RADIO= {ID_TAPD_TAPE15RADIO,CTL_RADIO,21,202, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[14]};
RADIO TAPDtape16RADIO= {ID_TAPD_TAPE16RADIO,CTL_RADIO,21,214, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[15]};
RADIO TAPDtape17RADIO= {ID_TAPD_TAPE17RADIO,CTL_RADIO,21,226, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[16]};
RADIO TAPDtape18RADIO= {ID_TAPD_TAPE18RADIO,CTL_RADIO,21,238, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[17]};
RADIO TAPDtape19RADIO= {ID_TAPD_TAPE19RADIO,CTL_RADIO,21,250, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[18]};
RADIO TAPDtape20RADIO= {ID_TAPD_TAPE20RADIO,CTL_RADIO,21,262, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[19]};
RADIO TAPDtape21RADIO= {ID_TAPD_TAPE21RADIO,CTL_RADIO,21,274, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[20]};
RADIO TAPDtape22RADIO= {ID_TAPD_TAPE22RADIO,CTL_RADIO,21,286, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[21]};
RADIO TAPDtape23RADIO= {ID_TAPD_TAPE23RADIO,CTL_RADIO,21,298, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[22]};
RADIO TAPDtape24RADIO= {ID_TAPD_TAPE24RADIO,CTL_RADIO,21,310, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[23]};
RADIO TAPDtape25RADIO= {ID_TAPD_TAPE25RADIO,CTL_RADIO,21,322, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[24]};
RADIO TAPDtape26RADIO= {ID_TAPD_TAPE26RADIO,CTL_RADIO,21,334, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[25]};
RADIO TAPDtape27RADIO= {ID_TAPD_TAPE27RADIO,CTL_RADIO,21,346, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[26]};
RADIO TAPDtape28RADIO= {ID_TAPD_TAPE28RADIO,CTL_RADIO,21,358, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[27]};
RADIO TAPDtape29RADIO= {ID_TAPD_TAPE29RADIO,CTL_RADIO,21,370, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[28]};
RADIO TAPDtape30RADIO= {ID_TAPD_TAPE30RADIO,CTL_RADIO,21,382, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDradioProc,0,&KWnds[0],TapeNames[29]};

CHECK TAPDwpCHECK = {ID_TAPD_WP, CTL_CHECK,8,404, 8, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&TAPDwpProc,0,&KWnds[0],"Write-Protect"};
BUTT TAPDdeleteBUTT = {ID_TAPD_DELETE, CTL_BUTT,8,420, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&TAPDdeleteProc,0,&KWnds[0],"Delete"};
BUTT TAPDrenameBUTT = {ID_TAPD_RENAME, CTL_BUTT,94,420, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDrenameProc,0,&KWnds[0],"Rename"};
BUTT TAPDnewBUTT = {ID_TAPD_NEW, CTL_BUTT,162,420, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&TAPDnewProc,0,&KWnds[0],"New"};
BUTT TAPDcloseBUTT = {ID_TAPD_CLOSE, CTL_BUTT,230,420, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_FOCUS,&TAPDcloseProc,0,&KWnds[0],"Close"};
DLG TapeDlg = {ID_TAPD, CTL_DLG,2, 2, 302, 448,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *TAPD_CTLs[] = {&TAPDtapeGROUP,&TAPDtape0RADIO,&TAPDtape1RADIO,&TAPDtape2RADIO,&TAPDtape3RADIO,&TAPDtape4RADIO,&TAPDtape5RADIO,&TAPDtape6RADIO,&TAPDtape7RADIO,
	&TAPDtape8RADIO,&TAPDtape9RADIO,  &TAPDtape10RADIO,&TAPDtape11RADIO,&TAPDtape12RADIO,&TAPDtape13RADIO,&TAPDtape14RADIO,&TAPDtape15RADIO,&TAPDtape16RADIO,
	&TAPDtape17RADIO,&TAPDtape18RADIO,&TAPDtape19RADIO,&TAPDtape20RADIO,&TAPDtape21RADIO,&TAPDtape22RADIO,&TAPDtape23RADIO,&TAPDtape24RADIO,&TAPDtape25RADIO,
	&TAPDtape26RADIO,&TAPDtape27RADIO,&TAPDtape28RADIO,&TAPDtape29RADIO,&TAPDtape30RADIO,&TAPDwpCHECK,&TAPDdeleteBUTT, &TAPDrenameBUTT, &TAPDnewBUTT, &TAPDcloseBUTT
};

///*********************************************************************
void CreateTapeDlg() {

	long	i, j, hFF, r;
	BYTE	attrib;
	RADIO	*pR, *pI, *pC;

	CreateDlg(&TapeDlg, TAPD_CTLs, sizeof(TAPD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	for(i=0; i<MAX_TAPES+1; i++) {
		pR = (RADIO *)FindWndID(ID_TAPD_TAPE0RADIO+i);
		if( pR != NULL ) {
			if( i==0 ) {
				pR->flags |= CF_CHECKED;
			} else {
				pR->pText[0] = 0;
				pR->flags |= WF_NOFOCUS;
				pR->flags &= ~CF_CHECKED;
			}
		}
	}
	if( -1 != (hFF=Kfindfirst("TAPES\\*", (BYTE *)FilePath, &attrib)) ) {	// find first file in TAPES directory
		r = 0;
		while( attrib && 0==(r=Kfindnext(hFF, (BYTE*)FilePath, &attrib)) ) ;	// skip any non-normal files (sub-dirs)
		for(i=0; r==0 && i<MAX_TAPES; i++) {
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE1RADIO+i);
			if( pR != NULL ) {
				pR->flags &= ~WF_NOFOCUS;
				// pR points to next available radio button
				// search for proper sort order (case insensitive) location
				// move any above it up to make room, then insert new one
				for(j=0; j<i; j++) {
					pI = (RADIO *)FindWndID(ID_TAPD_TAPE1RADIO+j);
					if( stricmp(FilePath, pI->pText)<0 ) {
						while( pR->id > pI->id ) {
							pC = (RADIO *)FindWndID(pR->id-1);	// find entry just before the pR
							strcpy(pR->pText, pC->pText);		// copy it up to pR
							pR = pC;							// move pR down
						}
						break;
					}
				}
				strcpy(pR->pText, FilePath);
			}
			while( 0==(r=Kfindnext(hFF, (BYTE*)FilePath, &attrib)) && attrib ) ;	// find next normal file
		}
		Kfindclose(hFF);
	}
	// now, if a tape is in the drive, search for its entry and select it
	if( CurTape[0] ) {
		for(i=0; i<MAX_TAPES; i++) {
			pR = (RADIO *)FindWndID(ID_TAPD_TAPE1RADIO+i);
			if( !strcmp(CurTape, pR->pText) ) {
				SetCheckID(pR->id, TRUE);
				break;
			}
		}
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
DWORD RENDDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT	*pO, *pN;
	long	f;
	char	cdisk[64];

	switch( msg ) {
	 case DLG_ACTIVATE:
		pO = (EDIT *)FindWndID(ID_RENDD_OLDEDIT);
		pN = (EDIT *)FindWndID(ID_RENDD_NEWEDIT);

		if( pN->Text[0]==0 ) {		// don't have a name
			SystemMessage("Must CANCEL or enter a name!");
			return 0;
		}
		if( strcmp(pN->Text, pO->Text) ) {	// different old and new names, actually something to do!
			sprintf((char*)ScratBuf, "DISKS%d\\%s", (int)DiskDlgFolder, pN->Text);
			if( -1!=(f=Kopen((char*)ScratBuf, O_RDBIN)) ) {	// that tape name already exists
				Kclose(f);
				SystemMessage("Sorry, that disk name is already used!");
				return 0;
			}
			if( pO->Text[0]==0 ) {	// no old name, just naming a new disk
				SetDiskFolder(DiskSC, DiskTLA, DiskDlgDrive, DiskDlgFolder);
				if( pDiskName[0] ) {		// there's an old tape loaded
					strcpy(cdisk, (char*)pDiskName);	// save old name
					EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);	// eject it
					NewDisk(DiskSC, DiskTLA, DiskDlgDrive, (BYTE*)pN->Text);	// create and load a new disk
					EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);			// write it to disk
					LoadDisk(DiskSC, DiskTLA, DiskDlgDrive, (BYTE*)cdisk);		// reload old disk
				} else {				// no tape loaded
					NewDisk(DiskSC, DiskTLA, DiskDlgDrive, (BYTE*)pN->Text);	// create and load a disk tape
					EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);			// eject it
					LoadDisk(DiskSC, DiskTLA, DiskDlgDrive, (BYTE *)pN->Text);	// load it
				}
			} else {				// renaming old disk
				sprintf((char*)ScratBuf, "DISKS%d\\%s", (int)DiskDlgFolder, pO->Text);
				sprintf((char*)ScratBuf+256, "DISKS%d\\%s", (int)DiskDlgFolder, pN->Text);
				Krename((char*)ScratBuf, (char*)ScratBuf+256);
				if( !strcmp((char*)pDiskName, pO->Text) ) strcpy((char*)pDiskName, pN->Text);
			}
		}
		ActiveDlg = NULL;
		DrawAll();
		CreateDiskDlg(DiskSC, DiskTLA, DiskDlgDrive, DiskDevType);
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD RENDDcancelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	switch( msg ) {
	 case DLG_ACTIVATE:
		ActiveDlg = NULL;
		DrawAll();
		CreateDiskDlg(DiskSC, DiskTLA, DiskDlgDrive, DiskDevType);
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT RENDDoldTEXT = {ID_RENDD_OLDTEXT, CTL_TEXT,8,12, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Old name:"};
EDIT RENDDoldEDIT = {ID_RENDD_OLDEDIT, CTL_EDIT,84, 8, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&RenOldProc,0,&KWnds[0]};
TEXT RENDDnewTEXT = {ID_RENDD_NEWTEXT, CTL_TEXT,8,30, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"New name:"};
EDIT RENDDnewEDIT = {ID_RENDD_NEWEDIT, CTL_EDIT,84,26, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0]};
BUTT RENDDokBUTT = {ID_RENDD_OK, CTL_BUTT,83,46, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&RENDDokProc,0,&KWnds[0],"OK"};
BUTT RENDDcancelBUTT = {ID_RENDD_CANCEL, CTL_BUTT,155,46, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&RENDDcancelProc,0,&KWnds[0],"Cancel"};
DLG RenameDiskDlg = {ID_RENDD, CTL_DLG,2, 2, 302, 92,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *RENDD_CTLs[] = {&RENDDoldTEXT, &RENDDoldEDIT,&RENDDnewTEXT, &RENDDnewEDIT,&RENDDokBUTT, &RENDDcancelBUTT};

///*********************************************************************
void CreateRenameDiskDlg(char *pOld) {

	EDIT	*pE;

	CreateDlg(&RenameDiskDlg, RENDD_CTLs, sizeof(RENDD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	pE = (EDIT *)FindWndID(ID_RENDD_OLDEDIT);
	if( pE != NULL ) strcpy(pE->Text, pOld);

	pE = (EDIT *)FindWndID(ID_RENDD_NEWEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
	}

// GEEKDO: set TextLim on EDIT fields!!!

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
///*********************************************************************
void EnableNewDisk() {

	int		i;

	for(i=1; i<MAX_DISKS+1; i++) if( ((RADIO*)(FindWndID(ID_DSKD_DISK0RADIO+i)))->pText[0]==0 ) break;
	if( i < MAX_DISKS+1 ) FindWndID(ID_DSKD_NEW)->flags &= ~CF_DISABLED;
	else FindWndID(ID_DSKD_NEW)->flags |= CF_DISABLED;
	SendMsg(FindWndID(ID_DSKD_NEW), DLG_DRAW, 0, 0, 0);
}

///*********************************************************************
void InitDiskCtls() {

	int		sci, di, tli;
	char	*pName;
	long	i, j, hFF, r, dsize1, fsize, dsize2;
	BYTE	attrib;
	RADIO	*pR, *pI, *pC;

	SetCheckID(ID_DSKD_FOLDER0+DiskDlgFolder, TRUE);

	for(i=0; i<MAX_DISKS+1; i++) {
		pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
		if( pR != NULL ) {
			if( i==0 ) {
				pR->flags |= CF_CHECKED;
			} else {
				pR->pText[0] = 0;
				pR->flags |= WF_NOFOCUS;
				pR->flags &= ~CF_CHECKED;
				pR->flags &= ~CF_DISABLED;
			}
		}
	}
	dsize1 = DiskSize[DiskSC][DiskTLA];
	dsize2 = dsize1 + 256*(DiskParms[DiskDevType].cylsects*2);	// in case files from elsewhere that don't 'spare' 4 tracks are used
//sprintf((char*)ScratBuf, "dsize1=%ld dsize2=%ld", dsize1, dsize2);
//SystemMessage(ScratBuf);
	sprintf((char*)ScratBuf, "DISKS%d\\*", (int)DiskDlgFolder);
	if( -1 != (hFF=Kfindfirst((char*)ScratBuf, (BYTE*)FilePath, &attrib)) ) {	// find first file in DISKS directory
		r = 0;
		while( (attrib && 0==(r=Kfindnext(hFF, (BYTE*)FilePath, &attrib))) ) ;	// skip any non-normal files (sub-dirs)
		for(i=0; r==0 && i<MAX_DISKS; i++) {
			sprintf((char*)ScratBuf+1000, "DISKS%d\\%s", (int)DiskDlgFolder, FilePath);
			fsize = KFileSize((char*)ScratBuf+1000);
//sprintf((char*)ScratBuf+7000, "%s fsize=%ld", FilePath, fsize);
//SystemMessage(ScratBuf+7000);
			if( dsize1==fsize || dsize2==fsize ) {
				pR = (RADIO *)FindWndID(ID_DSKD_DISK1RADIO+i);
				if( pR != NULL ) {
					pR->flags &= ~WF_NOFOCUS;
					// pR points to next available radio button
					// search for proper sort order (case insensitive) location
					// move any above it up to make room, then insert new one
					for(j=0; j<i; j++) {
						pI = (RADIO *)FindWndID(ID_DSKD_DISK1RADIO+j);
						if( stricmp(FilePath, pI->pText)<0 ) {
							while( pR->id > pI->id ) {
								pC = (RADIO *)FindWndID(pR->id-1);	// find entry just before the pR
								strcpy(pR->pText, pC->pText);		// copy it up to pR
								pR = pC;							// move pR down
							}
							break;
						}
					}
					strcpy(pR->pText, FilePath);
				}
			} else --i;
			while( 0==(r=Kfindnext(hFF, (BYTE*)FilePath, &attrib)) && attrib ) ;	// find next normal file
		}
		Kfindclose(hFF);
	}
	// now, if a disk is in the drive, search for its entry and select it
	if( pDiskName[0] ) {
		for(i=0; i<MAX_DISKS; i++) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK1RADIO+i);
			if( !strcmp((char*)pDiskName, pR->pText) ) {
				SetCheckID(pR->id, TRUE);
				break;
			}
		}
	}
	// now disable radio button for diskette that's in the OTHER drives
	for(sci=0; sci<8; sci++) {
		for(tli=0; tli<8; tli++) {
			for(di=0; di<2; di++) {
				pName = GetDiskName(sci, tli, di);
				if( pName!=NULL && *pName!=0 && (sci!=DiskSC || tli!=DiskTLA || di!=DiskDlgDrive) ) {
					for(i=0; i<MAX_DISKS; i++) {
						pR = (RADIO *)FindWndID(ID_DSKD_DISK1RADIO+i);
						if( pR ) {
							if( !strcmp(pName, pR->pText) ) pR->flags |= CF_DISABLED;
						}
					}

				}
			}
		}
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
	EnableNewDisk();
}

///*********************************************************************
DWORD DSKDcloseProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO	*pR;
	long	i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<10; i++) if( GetCheckID(ID_DSKD_FOLDER0+i) ) break;
		SetDiskFolder(DiskSC, DiskTLA, DiskDlgDrive, i);

		for(i=0; i<MAX_DISKS+1; i++) if( GetCheckID(ID_DSKD_DISK0RADIO+i) ) break;

		if( i==0 || i>MAX_DISKS ) {	// Eject disk
			EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);
		} else {					// Insert disk
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO + i);
			if( pR && strcmp((char*)pDiskName, pR->pText) ) {	// different disk than what's currently in
				EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);	// Eject current tape
				strcpy((char*)pDiskName, pR->pText);
				LoadDisk(DiskSC, DiskTLA, DiskDlgDrive, pDiskName);
			}
		}
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DSKDfolderProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	int		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
	 	RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
	 	i = pWnd->id - ID_DSKD_FOLDER0;
	 	if( i!=DiskDlgFolder ) {
			EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);
			DiskDlgFolder = i;
			SetDiskFolder(DiskSC, DiskTLA, DiskDlgDrive, DiskDlgFolder);
			InitDiskCtls();
	 	}
		return TRUE;
	 default:
		break;
	}
	return RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DSKDdeleteProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO		*pR;
	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<MAX_DISKS+1; i++) if( GetCheckID(ID_DSKD_DISK0RADIO+i) ) break;

		if( i>0 && i<=MAX_DISKS ) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
			if( pR != NULL && pR->pText[0] ) {
				if( !strcmp((char*)pDiskName, pR->pText) ) {
					EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);	// if deleting the currently loaded disk, eject it first
				}

				sprintf((char*)ScratBuf, "DISKS%d\\%s", (int)DiskDlgFolder, pR->pText);
				Kdelete((char*)ScratBuf);

				ActiveDlg = NULL;	// "destroy" disk dialog
				CreateDiskDlg(DiskSC, DiskTLA, DiskDlgDrive, DiskDevType);	// recreate it without deleted disk
			}
		}
		EnableNewDisk();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DSKDnewProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO		*pR;
	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<MAX_DISKS+1; i++) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
			if( pR && pR->pText[0]==0 ) break;
		}
		if( i>0 && i<=MAX_DISKS ) {
			ActiveDlg = NULL;
			DrawAll();
			CreateRenameDiskDlg("");
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
DWORD DSKDrenameProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	RADIO		*pR;
	long		i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		for(i=0; i<MAX_DISKS+1; i++) if( GetCheckID(ID_DSKD_DISK0RADIO+i) ) break;

		if( i>0 && i<=MAX_DISKS ) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
			if( pR != NULL && pR->pText[0] ) {
				ActiveDlg = NULL;	// "destroy" tape dialog
				DrawAll();
				CreateRenameDiskDlg(pR->pText);	// rename this file
			}
		}
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD DSKDwpProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	int		i, hFile, sc, sci, d, di, tla, tli;
	RADIO	*pR;
	char	*pN;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pWnd->flags ^= CF_CHECKED;	// toggle check
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);

		for(i=0; i<MAX_DISKS+1; i++) if( GetCheckID(ID_DSKD_DISK0RADIO+i) ) break;
		if( i>0 && i<=MAX_DISKS ) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
			if( pR ) {
				sc = d = 8;
				for(sci=0; sci<8; sci++) {
					for(tli=0; tli<31; tli++) {
						for(di=0; di<2; di++) {
							if( IsDisk(sci, tli, di) ) {
								pN = GetDiskName(sci, tli, di);
								if( pN!=NULL && !strcmp(pN, pR->pText) ) {
									sc = sci;
									d  = di;
									tla= tli;
								}
							}
						}
					}
				}
				if( sc<8 ) {	// disk is INSERTED right now in a drive
					// save the new WriteProtect status in the disk file
					WriteDiskByte(sc, tla, d, 255, (pWnd->flags & CF_CHECKED) ? 1 : 0);
					WriteDisk(sc, tla, d);
				} else if( pR->pText[0] ) {
					sprintf((char*)ScratBuf, "DISKS%d\\", (int)DiskDlgFolder);
					strcat((char*)ScratBuf, pR->pText);

					if( -1 != (hFile=Kopen((char*)ScratBuf, O_WRBIN)) ) {
						Kseek(hFile, 255, SEEK_SET);
						Kread(hFile, (BYTE *)ScratBuf, 1);
						Kseek(hFile, 255, SEEK_SET);
						ScratBuf[0] = (pWnd->flags & CF_CHECKED) ? 1 : 0;
						Kwrite(hFile, (BYTE *)ScratBuf, 1);
						Kclose(hFile);
					}
				}
			}
		}
		return 0;
	}
	return CheckProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
DWORD DSKDradioProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	long	i, hFile;
	RADIO	*pR;

	pR = (RADIO *)pWnd;
	switch( msg ) {
	 case DLG_DRAW:
		if( pR->pText[0]==0 ) return 0;	// don't draw if no tape name here
		break;
	 case DLG_ACTIVATE:
	 case DLG_SETFOCUS:
		RadioButtonProc(pWnd, msg, arg1, arg2, arg3);

		// SetWriteProtect();
		for(i=0; i<MAX_DISKS+1; i++) if( GetCheckID(ID_DSKD_DISK0RADIO+i) ) break;
		if( i>0 && i<=MAX_DISKS ) {
			pR = (RADIO *)FindWndID(ID_DSKD_DISK0RADIO+i);
			if( pR && pR->pText[0] ) {
				sprintf((char*)ScratBuf, "DISKS%d\\", (int)DiskDlgFolder);
				strcat((char*)ScratBuf, pR->pText);
				if( -1 != (hFile=Kopen((char*)ScratBuf, O_RDBIN)) ) {
					Kseek(hFile, 255, SEEK_SET);
					Kread(hFile, (BYTE *)ScratBuf, 1);
					Kclose(hFile);
					SetCheckID(ID_DSKD_WP, ScratBuf[0]);
				}
			}
		}
		return 0;
	 case DLG_LBUTTDOWN:
		// if already selected, consider this a double-click and load the tape on the upclick
		DiskDoubleID = ( pWnd->flags & CF_CHECKED ) ? pWnd->id : 0;
		break;
	 case DLG_LBUTTUP:
		if( pWnd->id == DiskDoubleID ) {
			SendMsg(FindWndID(ID_DSKD_CLOSE), DLG_ACTIVATE, 0, 0, 0);
			return 0;
		}
		break;
	}
	return RadioButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************
GROUPBOX DSKDdiskGROUP = {ID_DSKD_DISKGROUP, CTL_GROUP,8,8, 286, 390,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Current Diskette "};
RADIO DSKDdisk0RADIO = {ID_DSKD_DISK0RADIO, CTL_RADIO,16, 20, 270, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&DSKDradioProc,0,&KWnds[0],"NO DISK IN DRIVE"};
RADIO DSKDdisk1RADIO = {ID_DSKD_DISK1RADIO, CTL_RADIO,21, 34, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[0]};
RADIO DSKDdisk2RADIO = {ID_DSKD_DISK2RADIO, CTL_RADIO,21, 46, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[1]};
RADIO DSKDdisk3RADIO = {ID_DSKD_DISK3RADIO, CTL_RADIO,21, 58, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[2]};
RADIO DSKDdisk4RADIO = {ID_DSKD_DISK4RADIO, CTL_RADIO,21, 70, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[3]};
RADIO DSKDdisk5RADIO = {ID_DSKD_DISK5RADIO, CTL_RADIO,21, 82, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[4]};
RADIO DSKDdisk6RADIO = {ID_DSKD_DISK6RADIO, CTL_RADIO,21, 94, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[5]};
RADIO DSKDdisk7RADIO = {ID_DSKD_DISK7RADIO, CTL_RADIO,21,106, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[6]};
RADIO DSKDdisk8RADIO = {ID_DSKD_DISK8RADIO, CTL_RADIO,21,118, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[7]};
RADIO DSKDdisk9RADIO = {ID_DSKD_DISK9RADIO, CTL_RADIO,21,130, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[8]};
RADIO DSKDdisk10RADIO= {ID_DSKD_DISK10RADIO,CTL_RADIO,21,142, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[9]};
RADIO DSKDdisk11RADIO= {ID_DSKD_DISK11RADIO,CTL_RADIO,21,154, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[10]};
RADIO DSKDdisk12RADIO= {ID_DSKD_DISK12RADIO,CTL_RADIO,21,166, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[11]};
RADIO DSKDdisk13RADIO= {ID_DSKD_DISK13RADIO,CTL_RADIO,21,178, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[12]};
RADIO DSKDdisk14RADIO= {ID_DSKD_DISK14RADIO,CTL_RADIO,21,190, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[13]};
RADIO DSKDdisk15RADIO= {ID_DSKD_DISK15RADIO,CTL_RADIO,21,202, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[14]};
RADIO DSKDdisk16RADIO= {ID_DSKD_DISK16RADIO,CTL_RADIO,21,214, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[15]};
RADIO DSKDdisk17RADIO= {ID_DSKD_DISK17RADIO,CTL_RADIO,21,226, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[16]};
RADIO DSKDdisk18RADIO= {ID_DSKD_DISK18RADIO,CTL_RADIO,21,238, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[17]};
RADIO DSKDdisk19RADIO= {ID_DSKD_DISK19RADIO,CTL_RADIO,21,250, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[18]};
RADIO DSKDdisk20RADIO= {ID_DSKD_DISK20RADIO,CTL_RADIO,21,262, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[19]};
RADIO DSKDdisk21RADIO= {ID_DSKD_DISK21RADIO,CTL_RADIO,21,274, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[20]};
RADIO DSKDdisk22RADIO= {ID_DSKD_DISK22RADIO,CTL_RADIO,21,286, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[21]};
RADIO DSKDdisk23RADIO= {ID_DSKD_DISK23RADIO,CTL_RADIO,21,298, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[22]};
RADIO DSKDdisk24RADIO= {ID_DSKD_DISK24RADIO,CTL_RADIO,21,310, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[23]};
RADIO DSKDdisk25RADIO= {ID_DSKD_DISK25RADIO,CTL_RADIO,21,322, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[24]};
RADIO DSKDdisk26RADIO= {ID_DSKD_DISK26RADIO,CTL_RADIO,21,334, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[25]};
RADIO DSKDdisk27RADIO= {ID_DSKD_DISK27RADIO,CTL_RADIO,21,346, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[26]};
RADIO DSKDdisk28RADIO= {ID_DSKD_DISK28RADIO,CTL_RADIO,21,358, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[27]};
RADIO DSKDdisk29RADIO= {ID_DSKD_DISK29RADIO,CTL_RADIO,21,370, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[28]};
RADIO DSKDdisk30RADIO= {ID_DSKD_DISK30RADIO,CTL_RADIO,21,382, 265, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDradioProc,0,&KWnds[0],DiskNames[29]};

GROUPBOX DSKDfolderGROUP = {ID_DSKD_FOLDERGROUP, CTL_GROUP,8,406, 286, 34,NULL, NULL, NULL, NULL,WF_CTL | WF_NOFOCUS | CF_GROUP,&GroupboxProc,0,&KWnds[0]," Disk FOLDER "};
BUTT DSKDfolder0BUTT = {ID_DSKD_FOLDER0, CTL_RADIO,12,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&DSKDfolderProc,0,&KWnds[0],"0"};
BUTT DSKDfolder1BUTT = {ID_DSKD_FOLDER1, CTL_RADIO,40,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"1"};
BUTT DSKDfolder2BUTT = {ID_DSKD_FOLDER2, CTL_RADIO,68,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"2"};
BUTT DSKDfolder3BUTT = {ID_DSKD_FOLDER3, CTL_RADIO,96,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"3"};
BUTT DSKDfolder4BUTT = {ID_DSKD_FOLDER4, CTL_RADIO,124,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"4"};
BUTT DSKDfolder5BUTT = {ID_DSKD_FOLDER5, CTL_RADIO,152,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"5"};
BUTT DSKDfolder6BUTT = {ID_DSKD_FOLDER6, CTL_RADIO,180,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"6"};
BUTT DSKDfolder7BUTT = {ID_DSKD_FOLDER7, CTL_RADIO,208,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"7"};
BUTT DSKDfolder8BUTT = {ID_DSKD_FOLDER8, CTL_RADIO,236,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"8"};
BUTT DSKDfolder9BUTT = {ID_DSKD_FOLDER9, CTL_RADIO,264,422, 24, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDfolderProc,0,&KWnds[0],"9"};

CHECK DSKDwpCHECK = {ID_DSKD_WP, CTL_CHECK,8,444, 8, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&DSKDwpProc,0,&KWnds[0],"Write-Protect"};

BUTT DSKDdeleteBUTT = {ID_DSKD_DELETE, CTL_BUTT,8,460, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&DSKDdeleteProc,0,&KWnds[0],"Delete"};
BUTT DSKDrenameBUTT = {ID_DSKD_RENAME, CTL_BUTT,94,460, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDrenameProc,0,&KWnds[0],"Rename"};
BUTT DSKDnewBUTT = {ID_DSKD_NEW, CTL_BUTT,162,460, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&DSKDnewProc,0,&KWnds[0],"New"};
BUTT DSKDcloseBUTT = {ID_DSKD_CLOSE, CTL_BUTT,230,460, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_FOCUS,&DSKDcloseProc,0,&KWnds[0],"Close"};

DLG DiskDlg = {ID_DSKD, CTL_DLG,2, 2, 302, 488,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};

void *DSKD_CTLs[] = {&DSKDdiskGROUP,&DSKDdisk0RADIO,&DSKDdisk1RADIO,&DSKDdisk2RADIO,&DSKDdisk3RADIO,&DSKDdisk4RADIO,&DSKDdisk5RADIO,&DSKDdisk6RADIO,&DSKDdisk7RADIO,
	&DSKDdisk8RADIO,&DSKDdisk9RADIO,&DSKDdisk10RADIO,&DSKDdisk11RADIO,&DSKDdisk12RADIO,&DSKDdisk13RADIO,&DSKDdisk14RADIO,&DSKDdisk15RADIO,&DSKDdisk16RADIO,&DSKDdisk17RADIO,
	&DSKDdisk18RADIO,&DSKDdisk19RADIO,&DSKDdisk20RADIO,&DSKDdisk21RADIO,&DSKDdisk22RADIO,&DSKDdisk23RADIO,&DSKDdisk24RADIO,&DSKDdisk25RADIO,&DSKDdisk26RADIO,&DSKDdisk27RADIO,
	&DSKDdisk28RADIO,&DSKDdisk29RADIO,&DSKDdisk30RADIO,&DSKDfolderGROUP,&DSKDfolder0BUTT,&DSKDfolder1BUTT,&DSKDfolder2BUTT,&DSKDfolder3BUTT,&DSKDfolder4BUTT,
	&DSKDfolder5BUTT,&DSKDfolder6BUTT,&DSKDfolder7BUTT,&DSKDfolder8BUTT,&DSKDfolder9BUTT,&DSKDwpCHECK,&DSKDdeleteBUTT,&DSKDrenameBUTT,&DSKDnewBUTT,&DSKDcloseBUTT
};

///*********************************************************************
void CreateDiskDlg(int sc, int tla, int d, int devtype) {

	char	cdisk[64];

	DiskSC = sc;
	DiskTLA = tla;
	DiskDlgDrive = d;
	DiskDevType = devtype;
	pDiskName = (BYTE*)GetDiskName(DiskSC, DiskTLA, DiskDlgDrive);
	DiskDlgFolder = GetDiskFolder(DiskSC, DiskTLA, DiskDlgDrive);
	strcpy(cdisk, (char*)pDiskName);	// save old name
	EjectDisk(DiskSC, DiskTLA, DiskDlgDrive);	// eject it
	LoadDisk(DiskSC, DiskTLA, DiskDlgDrive, (BYTE*)cdisk);		// reload old disk

	CreateDlg(&DiskDlg, DSKD_CTLs, sizeof(DSKD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	InitDiskCtls();
}

///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
DWORD IMPDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT	*pOS, *pLIF;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pOS = (EDIT *)FindWndID(ID_IMPD_OSEDIT);
		pLIF= (EDIT *)FindWndID(ID_IMPD_LIFEDIT);

		if( pLIF->Text[0]==0 ) {		// don't have a name
			SystemMessage("Must CANCEL or enter a LIF name!");
			return 0;
		}
		ImportFile(DiskSC, DiskTLA, 0, pOS->Text, pLIF->Text);
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
DWORD LIFEditProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pL, *pO;
	BYTE		*pT, *pLT;

	switch( msg ) {
	 case DLG_SETFOCUS:
		pL = (EDIT *)pWnd;
		pO = (EDIT *)FindWndID(ID_IMPD_OSEDIT);

		// set default LIF file name to match OS file name (stripping '.')
		if( pO->Text[0] ) {
			for(pT=(BYTE*)pO->Text+strlen(pO->Text)-1; pT>(BYTE*)pO->Text && *pT!='\\' && *pT!=':' && *pT!='/'; --pT) ;
			if( *pT=='\\' || *pT==':' || *pT=='/' ) ++pT;
			for(pLT=(BYTE*)pL->Text; *pT; ++pT) if( *pT != (BYTE)'.' ) *pLT++ = *pT;
			*pLT = 0;
		}
		break;
	}
	return EditProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT IMPDtitleTEXT = {ID_IMPD_TITLETEXT, CTL_TEXT,107, 8, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_CENTER | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"IMPORT FILE"};
TEXT IMPDosTEXT = {ID_IMPD_OSTEXT, CTL_TEXT,8,32, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"OS name:"};
EDIT IMPDosEDIT = {ID_IMPD_OSEDIT, CTL_EDIT,84,28, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0]};
TEXT IMPDlifTEXT = {ID_IMPD_LIFTEXT, CTL_TEXT,8,50, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"LIF name:"};
EDIT IMPDlifEDIT = {ID_IMPD_LIFEDIT, CTL_EDIT,84,46, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&LIFEditProc,0,&KWnds[0]};
BUTT IMPDokBUTT = {ID_IMPD_OK, CTL_BUTT,83,66, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&IMPDokProc,0,&KWnds[0],"OK"};
BUTT IMPDcancelBUTT = {ID_IMPD_CANCEL, CTL_BUTT,155,66, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
DLG ImportDlg = {ID_IMPD, CTL_DLG,2, 2, 302, 92,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *IMPD_CTLs[] = {&IMPDtitleTEXT,&IMPDosTEXT, &IMPDosEDIT,&IMPDlifTEXT, &IMPDlifEDIT,&IMPDokBUTT, &IMPDcancelBUTT};

///*********************************************************************************
void CreateImportDlg() {

	EDIT	*pE;

	CreateDlg(&ImportDlg, IMPD_CTLs, sizeof(IMPD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	pE = (EDIT *)FindWndID(ID_IMPD_OSEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
	}

	pE = (EDIT *)FindWndID(ID_IMPD_LIFEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
		pE->textlim = 10;
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
/// Import ALL files from a native FOLDER into the current :D700 LIF disk
DWORD IMPDallokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT	*pOS;
	char	lifN[16], osN[256], *p;
	long	i, hFF;
	BYTE	attrib;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pOS = (EDIT *)FindWndID(ID_IMPD_OSEDIT);

		sprintf((char*)ScratBuf, "%s\\*", pOS->Text);
		if( -1 != (hFF=Kfindfirst((char*)ScratBuf, (BYTE*)FilePath, &attrib)) ) {	// find first file in DISKS directory
			do {
				if( !attrib ) {	// if not sub-folder
					sprintf(osN, "%s\\%s", pOS->Text, FilePath);
					for(p=osN+strlen(osN); p>osN && *(p-1)!='\\'; --p) ;	// find beginning of just file name
					for(i=0; i<10 && *p; ++p) if( *p!='.' ) lifN[i++] = *p;	// copy the file name to the LIF name, stripping any '.' characters
					lifN[i] = 0;
					ImportFile(DiskSC, DiskTLA, 0, osN, lifN);
				}
			} while( 0==Kfindnext(hFF, (BYTE*)FilePath, &attrib) );

			Kfindclose(hFF);
		}

		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT IMPDalltitleTEXT = {ID_IMPD_TITLETEXT, CTL_TEXT,107, 8, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_CENTER | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"IMPORT FILES"};
TEXT IMPDallosTEXT = {ID_IMPD_OSTEXT, CTL_TEXT,8,32, 80, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"OS folder:"};
EDIT IMPDallosEDIT = {ID_IMPD_OSEDIT, CTL_EDIT,92,28, 196, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0]};
BUTT IMPDallokBUTT = {ID_IMPD_OK, CTL_BUTT,83,50, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&IMPDallokProc,0,&KWnds[0],"OK"};
BUTT IMPDallcancelBUTT = {ID_IMPD_CANCEL, CTL_BUTT,155,50, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
DLG ImportAllDlg = {ID_IMPD, CTL_DLG,2, 2, 302, 76,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *IMPD_ALL_CTLs[] = {&IMPDalltitleTEXT,&IMPDallosTEXT, &IMPDallosEDIT,&IMPDallokBUTT, &IMPDallcancelBUTT};

///*********************************************************************************
void CreateImportAllDlg() {

	EDIT	*pE;

	CreateDlg(&ImportAllDlg, IMPD_ALL_CTLs, sizeof(IMPD_ALL_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	pE = (EDIT *)FindWndID(ID_IMPD_OSEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
DWORD EXPDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT	*pOS, *pLIF;
	long	i;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pOS = (EDIT *)FindWndID(ID_EXPD_OSEDIT);
		pLIF= (EDIT *)FindWndID(ID_EXPD_LIFEDIT);

		if( pOS->Text[0]==0 ) {		// don't have a name
			SystemMessage("Must CANCEL or enter an OS name!");
			return 0;
		}
		// tail-fill target LIF filename with blanks
		for(i=strlen(pLIF->Text); i<10; i++) pLIF->Text[i] = ' ';
		pLIF->Text[i] = 0;

// scan LIF diskette directory for LIF file name

		ExportLIF(DiskSC, DiskTLA, 0, pLIF->Text, pOS->Text);
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
DWORD OSEditProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT		*pL, *pO;

	switch( msg ) {
	 case DLG_SETFOCUS:
		pO = (EDIT *)pWnd;
		pL = (EDIT *)FindWndID(ID_EXPD_LIFEDIT);

		// set default OS file name to match LIF file name
		strcpy(pO->Text, pL->Text);
		// do normal edit SETFOCUS stuff
		EditProc(pWnd, msg, arg1, arg2, arg3);
		// get rid of selection, home cursor, and redraw
		pO->selE = pO->caret = pO->selS = 0;
		SetEditLineView(pWnd);
		SendMsg(pWnd, DLG_DRAW, 0, 0, 0);

		return 0;
	}
	return EditProc(pWnd, msg, arg1, arg2, arg3);
}

///***********************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT EXPDtitleTEXT = {ID_EXPD_TITLETEXT, CTL_TEXT,107, 8, 72, 12,	NULL, NULL, NULL, NULL,WF_CTL | CF_CENTER | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"EXPORT FILE"};
TEXT EXPDlifTEXT = {ID_EXPD_LIFTEXT, CTL_TEXT,8,32, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"LIF name:"};
EDIT EXPDlifEDIT = {ID_EXPD_LIFEDIT, CTL_EDIT,84,28, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0]};
TEXT EXPDosTEXT = {ID_EXPD_OSTEXT, CTL_TEXT,8,50, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"OS name:"};
EDIT EXPDosEDIT = {ID_EXPD_OSEDIT, CTL_EDIT,84,46, 172, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP,&OSEditProc,0,&KWnds[0]};
BUTT EXPDokBUTT = {ID_EXPD_OK, CTL_BUTT,83,66, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&EXPDokProc,0,&KWnds[0],"OK"};
BUTT EXPDcancelBUTT = {ID_EXPD_CANCEL, CTL_BUTT,155,66, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
DLG ExportDlg = {ID_EXPD, CTL_DLG,2, 2, 302, 92,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *EXPD_CTLs[] = {&EXPDtitleTEXT,&EXPDlifTEXT, &EXPDlifEDIT,&EXPDosTEXT, &EXPDosEDIT,&EXPDokBUTT, &EXPDcancelBUTT};

///*********************************************************************************
void CreateExportDlg() {

	EDIT	*pE;

	CreateDlg(&ExportDlg, EXPD_CTLs, sizeof(EXPD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	pE = (EDIT *)FindWndID(ID_EXPD_OSEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
	}

	pE = (EDIT *)FindWndID(ID_EXPD_LIFEDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
		pE->textlim = 10;
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
///*********************************************************************************
/// Import ALL files from a native FOLDER into the current :D700 LIF disk
DWORD TYPDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3) {

	EDIT	*pOS;
	long	f;
//	char	osN[256], *p;
//	long	i, hFF;

	switch( msg ) {
	 case DLG_ACTIVATE:
		pOS = (EDIT *)FindWndID(ID_TYPD_EDIT);

		//pOS->Text
		if( -1!=(f=KopenAbs(pOS->Text, O_RDBIN)) ) {
			KeyMemLen = Kseek(f, 0, SEEK_END);
			if( NULL != (pKeyMem = (BYTE *)malloc(KeyMemLen+1)) ) {	// one extra for NUL on last line in case no CR/LF on last line
				Kseek(f, 0, SEEK_SET);
				Kread(f, pKeyMem, KeyMemLen);
				pKeyMem[KeyMemLen] = 0;
				pKeyPtr = pKeyMem;
			}
			Kclose(f);
		}
		ActiveDlg = NULL;
		DrawAll();
		return 0;
	 default:
		break;
	}
	return ButtonProc(pWnd, msg, arg1, arg2, arg3);
}

///*********************************************************************************
/// id, type, xoff, yoff, w, h, prev, next, child1, parent, flags, proc, altchar, kWnd, pText
TEXT TYPDtitleTEXT = {ID_TYPD_TITLETEXT, CTL_TEXT,107, 8, 72, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_CENTER | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"Type ASCII file"};
TEXT TYPD_TEXT = {ID_TYPD_TEXT, CTL_TEXT,8,32, 80, 12,NULL, NULL, NULL, NULL,WF_CTL | CF_RIGHT | WF_NOFOCUS | CF_GROUP,&TextProc,0,&KWnds[0],"OS file:"};
EDIT TYPD_EDIT = {ID_TYPD_EDIT, CTL_EDIT,92,28, 196, 16,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_GROUP | CF_FOCUS,&EditProc,0,&KWnds[0],};
BUTT TYPDokBUTT = {ID_TYPD_OK, CTL_BUTT,83,50, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP | CF_DEFAULT | CF_GROUP,&TYPDokProc,0,&KWnds[0],"OK"};
BUTT TYPDcancelBUTT = {ID_TYPD_CANCEL, CTL_BUTT,155,50, 64, 20,NULL, NULL, NULL, NULL,WF_CTL | CF_TABSTOP,&CEDcloseProc,0,&KWnds[0],"Cancel"};
DLG TypeASCIIDlg = {ID_TYPD, CTL_DLG,2, 2, 302, 76,NULL, NULL, NULL, NULL,0,DefDialogProc,0,&KWnds[0],NULL};
void *TYPD_CTLs[] = {&TYPDtitleTEXT,&TYPD_TEXT, &TYPD_EDIT,&TYPDokBUTT, &TYPDcancelBUTT};

///*********************************************************************************
void CreateTypeASCIIDlg() {

	EDIT	*pE;

	CreateDlg(&TypeASCIIDlg, TYPD_CTLs, sizeof(TYPD_CTLs)/sizeof(void *), FALSE, &KWnds[0]);

	pE = (EDIT *)FindWndID(ID_TYPD_EDIT);
	if( pE != NULL ) {
		pE->Text[0] = 0;
	}

	SendMsg((WND *)ActiveDlg, DLG_DRAW, 0, 0, 0);
}

