/*******************************************
 SUPPORT.C - Graphics, etc, routines
	Copyright 2003-17 Everett Kaser
	All rights reserved.
 See HP85EM.TXT for legal usage.
 *******************************************/

//*********************************
// INCLUDES

#include "main.h"
#include "support.h"
#include "mach85.h"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <math.h>

//***********************************************************************
#if DEBUG

BYTE	DBuf[256];
BOOL	DBTRUE=FALSE;
int		deb_startlog=0;

//***********************************************************************
void WriteDebug(BYTE *p) {

	int		hFile;
	char	tbuf[128];

	if( -1==(hFile = Kopen("debug.log", O_WRBIN)) ) hFile = Kopen("debug.log", O_WRBINNEW);
	if( hFile != -1 ) {
		Kseek(hFile, 0, SEEK_END);
		Kwrite(hFile, p, strlen((char*)p));
		if( ROM!=NULL ) sprintf(tbuf, "\t\tROM:PC=%3.3o:%o IOBITS=%2.2X\r\n", ROM[060000], 256*Reg[5]+Reg[4], ROM[0100667]);
		else sprintf(tbuf, "\r\n");
		Kwrite(hFile, (BYTE*)tbuf, strlen(tbuf));
		Kclose(hFile);
	}
}

//***********************************
void DebLog(BYTE *p) {

	if( deb_startlog ) WriteDebug(p);
}

#endif // DEBUG

//********************************************************************************
// Global graphics variables, common to all .KE programs running at any given time
PBMB		pbFirst=NULL;

BYTE		TmpFname[512];
BYTE		iniName[]="Series80.ini";

int			TdBMW, TdBMH, TsBMW, TsBMH;
int			TdclipX1, TdclipX2, TdclipY1, TdclipY2;
int			TsclipX1, TsclipX2, TsclipY1, TsclipY2;
int			TdstepX, TdstepY, TsstepX, TsstepY, TastepX, TastepY;
BYTE		*Tdpbits, *Tspbits, *Tapbits;

#ifdef NOT_MS_WINDOWS
	typedef struct tagBITMAPFILEHEADER { // bmfh
			WORD    bfType;
			DWORD   bfSize;
			WORD    bfReserved1;
			WORD    bfReserved2;
			DWORD   bfOffBits;
	} BITMAPFILEHEADER;

	typedef struct tagBITMAPINFOHEADER{ // bmih
	   DWORD  biSize;
	   long   biWidth;
	   long   biHeight;
	   WORD   biPlanes;
	   WORD   biBitCount;
	   DWORD  biCompression;
	   DWORD  biSizeImage;
	   long   biXPelsPerMeter;
	   long   biYPelsPerMeter;
	   DWORD  biClrUsed;
	   DWORD  biClrImportant;
	} BITMAPINFOHEADER;

	typedef struct tagRGBQUAD { // rgbq
		BYTE    rgbBlue;
		BYTE    rgbGreen;
		BYTE    rgbRed;
		BYTE    rgbReserved;
	} RGBQUAD;

	typedef struct tagBITMAPINFO { // bmi
	   BITMAPINFOHEADER bmiHeader;
	   RGBQUAD          bmiColors[1];
	} BITMAPINFO;
#endif

//*************************************************************
//*************************************************************
// HARDWARE-INDEPENDENT GRAPHICS FUNCTIONS
//*************************************************************
//*************************************************************

//*************************************************************
// Load a MS Windows/OS2 .BMP format file into a device-independent
// memory array.  BMP file can be 8-bit (256 color / palettized)
// or 24-bit (RGB pixel values), but can NOT be compressed, and is
// assumed to be in the "bottom-up" format (bmiHeader.biHeight > 0).
// LoadBMB supports TWO transparency colors.  If first pixel
// coordinates (transX1, transY1) are (-1,-1) then the bitmap has
// no transparency color.  If it's a valid pixel coordinate, then
// transX2 and transY2 can either be (-1,-1) for only ONE transparency
// color OR the coordinates of a second color pixel.
PBMB LoadBMB(char *fname, int transX1, int transY1, int transX2, int transY2) {

// NOTE: If porting to non-MS Windows platform, use
//	typedefs for BITMAPFILEHEADER and BITMAPINFO at top of this file
//  (define NOT_MS_WINDOWS).
	BITMAPFILEHEADER	bmfh;
	BITMAPINFO			*pbmi;
	DWORD				dwInfoSize, dwBitsSize, BytesRead;
	PBMB				pX;
	BYTE				*pB, *pL, *pO, *pA;
	int					hFile;
	int					x, y, w, h, bitcnt, r, g, b;
	BYTE				ci, tR1, tG1, tB1, tR2, tG2, tB2, tran;
	DWORD				linlen;

	// Open the file: read access, prohibit write access
	hFile = Kopen(fname, O_RDBIN);
	if( -1 == hFile ) return NULL;

	// Read in the BITMAPFILEHEADER
	BytesRead = Kread(hFile, (BYTE *)&bmfh, sizeof(BITMAPFILEHEADER));
	if( BytesRead!=sizeof(BITMAPFILEHEADER) || (bmfh.bfType != *(WORD *)"BM") ) {
		Kclose(hFile);
		return NULL;
	}
	// Allocate memory for the information structure & read it in
	dwInfoSize = bmfh.bfOffBits - sizeof (BITMAPFILEHEADER);

	if( NULL==(pbmi=(struct tagBITMAPINFO *)malloc(dwInfoSize)) ) {
		Kclose(hFile);
		return NULL;
	}
	BytesRead = Kread(hFile, (BYTE *)pbmi, dwInfoSize);
	if( BytesRead != dwInfoSize )	{
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}
	if( pbmi->bmiHeader.biBitCount<8 || pbmi->bmiHeader.biCompression!=0 ) {	// don't handle < 256 color bitmaps OR compressed bitmaps
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}

	// Create the DIB
	if( NULL==(pX=CreateBMB((WORD)(pbmi->bmiHeader.biWidth), (WORD)(pbmi->bmiHeader.biHeight), NULL)) ) {
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}
	if( transX1>=0 && transX1<(int)pX->physW && transY1>=0 && transY1<(int)pX->physH ) {
		if( NULL==(pX->pAND=(BYTE *)malloc(pX->physW*pX->physH)) ) {
			DeleteBMB(pX);
			Kclose(hFile);
			free(pbmi);
			return NULL;
		}
		tran = 1;	// we're doing a transparent (masked) bitmap
	} else tran = 0;

	// allocate temp storage for .BMP file RGB/palette-index bytes
	dwBitsSize = bmfh.bfSize - bmfh.bfOffBits;
	if( NULL==(pB=(BYTE *)malloc(dwBitsSize)) ) {
		DeleteBMB(pX);
		Kclose(hFile);
		free(pbmi);
		return NULL;
	}

	BytesRead = Kread(hFile, pB, dwBitsSize);
	Kclose(hFile);
	if( BytesRead != dwBitsSize ) {
		free(pB);
		DeleteBMB(pX);
		free(pbmi);
		return NULL;
	}

	w = pbmi->bmiHeader.biWidth;
	h = pbmi->bmiHeader.biHeight;
	bitcnt = pbmi->bmiHeader.biBitCount - 8;	// it's either 8 or 24, so 0 or non-0
	if( pbmi->bmiHeader.biSizeImage==0 ) {
		pbmi->bmiHeader.biSizeImage = 4*((w*(bitcnt?3:1)+3)/4)*h;
	}
	linlen = pbmi->bmiHeader.biSizeImage / h;
	if( tran ) {	// if transparent (masked) bitmap, get transparency color(s)
		if( bitcnt ) {	// 24-bit bitmap
			pL = pB + linlen*(h-transY1-1) + 3*transX1;
			tB1 = *pL++;
			tG1 = *pL++;
			tR1 = *pL++;
			if( transX2>=0 && transX2<(int)pX->physW && transY2>=0 && transY2<(int)pX->physH ) {
				pL = pB + linlen*(h-transY2-1) + 3*transX2;
				tB2 = *pL++;
				tG2 = *pL++;
				tR2 = *pL++;
			} else {
				tB2 = tB1;
				tG2 = tG1;
				tR2 = tR1;
			}
		} else {		// 8-bit bitmap
			pL = pB + linlen*(h-transY1-1) + transX1;
			ci = *pL;
			tR1 = pbmi->bmiColors[ci].rgbRed;
			tG1 = pbmi->bmiColors[ci].rgbGreen;
			tB1 = pbmi->bmiColors[ci].rgbBlue;
			if( transX2>=0 && transX2<(int)pX->physW && transY2>=0 && transY2<(int)pX->physH ) {
				pL = pB + linlen*(h-transY2-1) + transX2;
				ci = *pL;
				tR2 = pbmi->bmiColors[ci].rgbRed;
				tG2 = pbmi->bmiColors[ci].rgbGreen;
				tB2 = pbmi->bmiColors[ci].rgbBlue;
			} else {
				tB2 = tB1;
				tG2 = tG1;
				tR2 = tR1;
			}
		}
	}
	for(y=0; y<h; y++) {
		pL = pB + linlen*(h-y-1);	// common .BMPs are inverted vertically
		pO = pX->pBits + pX->linelen * y;
		if( tran ) pA = pX->pAND + pX->physW * y;
		for(x=0; x<w; x++) {
			if( bitcnt ) {	// 24-bit
				b = *pL++;
				g = *pL++;
				r = *pL++;
			} else {		// 8-bit
				ci = *pL++;
				b = pbmi->bmiColors[ci].rgbBlue;
				g = pbmi->bmiColors[ci].rgbGreen;
				r = pbmi->bmiColors[ci].rgbRed;
			}
			*pO++ = b;
			*pO++ = g;
			*pO++ = r;
			if( tran ) *pA++ = ((r==tR1 && g==tG1 && b==tB1) || (r==tR2 && g==tG2 && b==tB2)) ? 255 : 0;
		}
	}
	free(pB);
	free(pbmi);

	return pX;
}

//***************************************************************
void SetupBMD(PBMB *bmD) {

	PBMB	bD;

	bD = *bmD;
	if( bD==0 ) bD = KWnds[0].pBM;
	TdBMW = bD->logW;
	TdBMH = bD->logH;
	TdclipX1 = bD->clipX1;
	TdclipY1 = bD->clipY1;
	TdclipX2 = bD->clipX2;
	TdclipY2 = bD->clipY2;

	TdstepX	= 3;
	TdstepY = bD->linelen;
	Tdpbits = bD->pBits;
	*bmD = bD;
}


void SetupBMS(PBMB *bmS) {

	PBMB	bS;

	bS = *bmS;
	if( bS==0 ) bS = KWnds[0].pBM;
	TsBMW = bS->logW;
	TsBMH = bS->logH;
	TsclipX1 = bS->clipX1;
	TsclipY1 = bS->clipY1;
	TsclipX2 = bS->clipX2;
	TsclipY2 = bS->clipY2;
	TsstepX	= 3;
	TsstepY = bS->linelen;
	Tspbits = bS->pBits;
	TastepX	= 1;
	TastepY = bS->physW;
	Tapbits = bS->pAND;
	*bmS = bS;
}

//**************************************************************************
// Block-transfer a region from one bitmap to another.
// If either bitmap handle is NULL, redirect it to the screen bitmap
// In the source (masked) bitmap, the pBits memory block has 3 bytes
// per pixel containing the RGB information to be forced into the
// destination.  If 'masked' is TRUE, then the pAND memory block has
// 1 byte per pixel that is 255 to leave the destination untouched at
// that pixel, 0 to force the RGB pBits pixel color.  (If 'masked' is
// FALSE, then the pAND is ignored and all source pixel values are forced
// into the destination.
void BltBMB(PBMB bmD, int xd, int yd, PBMB bmS, int xs, int ys, int ws, int hs, int masked) {

	BYTE	*pDST, *pSRC, *pAND, *pD, *pS, *pA;
	int		x, y, ix;

	SetupBMD(&bmD);
	SetupBMS(&bmS);
	if( bmS->pAND==NULL ) masked = 0;	// if no mask, don't try to use it

	if( xs < TsclipX1 ) {
		if( (ws -= TsclipX1-xs)<=0 ) return;	// if src is clear off the left of source bitmap
		xd += TsclipX1-xs;
		xs = TsclipX1;
	}
	ix = xs+ws-TsclipX2;
	if( ix>0 ) {
		if( (ws -= ix)<=0 ) return;	// if src is clear off right of src bitmap
	}
	if( xd < TdclipX1 ) {
		if( (ws -= TdclipX1-xd)<=0 ) return;	// if dest is CLEAR off the left of dest bitmap
		xs += TdclipX1-xd;
		xd = TdclipX1;
	}
	ix = xd+ws-TdclipX2;
	if( ix>0 ) {
		if( (ws -= ix)<=0 ) return;	// if dest is clear off right of dst bitmap
	}
	if( ys < TsclipY1 ) {
		if( (hs -= TsclipY1-ys)<=0 ) return;	// if src is clear off top of source bitmap
		yd += TsclipY1-ys;
		ys = TsclipY1;
	}
	ix = ys+hs-TsclipY2;
	if( ix>0 ) {
		if( (hs -= ix)<=0 ) return;	// if src is clear off bottom of src bitmap
	}
	if( yd < TdclipY1 ) {
		if( (hs -= TdclipY1-yd)<=0 ) return;	// if dest is CLEAR off top of dst bitmap
		ys += TdclipY1-yd;
		yd = TdclipY1;
	}
	ix = yd+hs-TdclipY2;
	if( ix>0 ) {
		if( (hs -= ix)<=0 ) return;	// if dst is clear off bottom of dst bitmap
	}
	pS = Tspbits + TsstepY*ys + TsstepX*xs;
	pD = Tdpbits + TdstepY*yd + TdstepX*xd;
	if( masked ) {
		pA = Tapbits + TastepY*ys + TastepX*xs;
		for(y=0; y<hs; y++) {
			pAND = pA;
			pSRC = pS;
			pDST = pD;
			for(x=0; x<ws; x++) {
				if( *pAND == 0 ) {	// if AND mask has a hole, paint the SRC into the DST
					*pDST = *pSRC;
					*(pDST+1) = *(pSRC+1);
					*(pDST+2) = *(pSRC+2);
				}
				pSRC += TsstepX;
				pAND += TastepX;
				pDST += TdstepX;		// move dest ptr to next pixel
			}
			pA += TastepY;
			pS += TsstepY;
			pD += TdstepY;
		}
	} else {
		for(y=0; y<hs; y++) {
			pSRC = pS;
			pDST = pD;
			for(x=0; x<ws; x++) {
				*pDST = *pSRC;
				*(pDST+1) = *(pSRC+1);
				*(pDST+2) = *(pSRC+2);
				pSRC += TsstepX;
				pDST += TdstepX;
			}
			pS += TsstepY;
			pD += TdstepY;
		}
	}
	KInvalidate(bmD, xd, yd, xd+ws, yd+hs);
}

//*************************************************************
void ClipBMB(PBMB hBM, int x, int y, int w, int h) {

	if( hBM==0 ) hBM = KWnds[0].pBM;
	if( -1==x ) {
		hBM->clipX1 = hBM->clipY1 = 0;
		hBM->clipX2 = hBM->logW;
		hBM->clipY2 = hBM->logH;
	} else {
		hBM->clipX1 = Kmax(x, 0);
		hBM->clipY1 = Kmax(y, 0);
		hBM->clipX2 = Kmin(x + w, hBM->logW);
		hBM->clipY2 = Kmin(y + h, hBM->logH);
	}
}

//************************************************************
// Similar to Label85() in MACH85.C, but avoids the repetitive
// overhead of SetupBMD() and clipping, etc, for each point.
//
void Label85BMB(PBMB hBM, int x, int y, int rowH, BYTE *str, int n, DWORD clr) {

	int		cx, cy;
	BYTE	c, *pBits, *pDR, *pDC, r, g, b, bits;
	DWORD	fclr;

	SetupBMD(&hBM);
	if( x<TdclipX1 || y<TdclipY1 || y+8>TdclipY2 ) return;
	if( x +8*n-1 > TdclipX2 ) n = (TdclipX2-TdclipX1)/8;

	while( n--  &&  x+8 <= TdclipX2 ) {
		fclr = clr;
		c = *str++;
		if( c & 128 ) {
			if( Model<2 ) {
				LineBMB(hBM, x, y+8, x+6, y+8, clr);
				LineBMB(hBM, x, y+9, x+6, y+9, clr);
			} else {
				fclr = clr^0x00FFFFFF;
				RectBMB(hBM, x, y-2, x+8, y-2+rowH, clr, CLR_NADA);
			}
			c &= 0x7F;
		}
		pDC = Tdpbits + TdstepY*y + TdstepX*(x+1);
		r = (BYTE)(fclr & 0x0FF);
		g = (BYTE)((fclr & 0x0FF00)>>8);
		b = (BYTE)((fclr & 0x0FF0000)>>16);
		if( Model<2 ) {
			pBits = HP85font[c];
			for(cx=0; cx<7; cx++) {
				c = *pBits++;
				pDR = pDC;
				for(cy=0; cy<8; cy++) {
					if( c & 0x80 ) {
						*pDR++ = b;
						*pDR++ = g;
						*pDR++ = r;
						pDR += TdstepY-3;
					} else pDR += TdstepY;
					c <<= 1;
				}
				pDC += TdstepX;
			}
		} else {
			pBits = HP85font[HP87font[c]];
			for(cx=0; cx<7; cx++) {
				bits = *pBits++;
				pDR = pDC;
				for(cy=0; cy<8; cy++) {
					if( bits & 0x80 ) {
						*pDR++ = b;
						*pDR++ = g;
						*pDR++ = r;
						if( cx==0 && (c==29 || c==31) ) {
							pDR -= 6;
							*pDR++ = b;
							*pDR++ = g;
							*pDR++ = r;
							pDR += 3;
						}
						if( cy==0 && (c==28 || c==31) ) {
							pDR -= TdstepY+3;
							*pDR = *(pDR-TdstepY) = b;
							++pDR;
							*pDR = *(pDR-TdstepY) = g;
							++pDR;
							*pDR = *(pDR-TdstepY) = r;
							pDR += TdstepY+1;
							if( !IsPageSize24() ) { // if PAGESIZE 16
								int		ex;
								pDR += 8*TdstepY-3;	// move to bottom of char cell
								for(ex=0; ex<5; ex++) {
									*pDR = b;
									++pDR;
									*pDR = g;
									++pDR;
									*pDR = r;
									pDR += TdstepY-2;
								}
								pDR -= 13*TdstepY-3;
							}
						}
						pDR += TdstepY-3;
					} else pDR += TdstepY;
					bits <<= 1;
				}
				pDC += TdstepX;
			}
		}
		x += 8;
	}
}

//**************************************************************************
// Smooth resize the source bitmap into the destination bitmap
// averaging source pixels to generate destination pixels.

// NOTE!!!! Does NOT do masked bitmaps!!!

void StretchBMB(PBMB bmD, int xd, int yd, int wd, int hd, PBMB bmS, int xs, int ys, int ws, int hs) {

	BYTE	*pDST, *pSRC, *pD, *pS, *pSS;
	int		Ycnt, Xcnt, Ydest, Xdest, srcXcnt, srcYcnt, srcXcnt2, srcYcnt2;
	PBMB	hTmpBM;
	double	wPPP, hPPP, wP, hP;	// pixels-per-pixels, ratio of ws/wd and hs/hd
	double	fYfirst, fXfirst, fysoff, fxsoff, r, g, b, d, m;

	if( wd<=0 || hd<=0 || ws<=0 || hs<=0 ) return;
	if( wd<ws && hd>hs ) {
		hTmpBM = CreateBMB((WORD)wd, (WORD)hs, NULL);
		StretchBMB(hTmpBM, 0, 0, wd, hs, bmS, xs, ys, ws, hs);
		StretchBMB(bmD, xd, yd, wd, hd, hTmpBM, 0, 0, wd, hs);
		DeleteBMB(hTmpBM);
		return;
	} else if( wd>ws && hd<hs ) {
		hTmpBM = CreateBMB((WORD)ws, (WORD)hd, NULL);
		StretchBMB(hTmpBM, 0, 0, ws, hd, bmS, xs, ys, ws, hs);
		StretchBMB(bmD, xd, yd, wd, hd, hTmpBM, 0, 0, ws, hd);
		DeleteBMB(hTmpBM);
		return;
	}
	if( wd==ws && hd==hs ) {
		BltBMB(bmD, xd, yd, bmS, xs, ys, ws, hs, FALSE);
		return;
	}
	SetupBMD(&bmD);
	if( bmS==0 ) bmS = KWnds[0].pBM;
	SetupBMS(&bmS);

	if( wd>=ws && hd>=hs ) {	// stretch X and stretch Y
		wPPP = (double)wd/(double)ws;
		hPPP = (double)hd/(double)hs;

		pD = Tdpbits + TdstepY*yd + TdstepX*xd;
		pS = Tspbits + TsstepY*ys + TsstepX*xs;

		pSRC = pS;

		hP = hPPP;
		for(Ycnt=srcYcnt=0, Ydest=yd; Ycnt<hd; Ycnt++, Ydest++) {
			if( hP==0 ) {
				hP = hPPP-(1.0-fYfirst);
				pS += TsstepY;
				++srcYcnt;
			}
			if( hP<1.0 ) fYfirst = hP;
			else fYfirst = 1.0;

			pDST = pD + TdstepY*Ycnt;
			pSRC = pS;

			wP = wPPP;

			if( Ydest>=TdclipY1 && Ydest<TdclipY2 ) {
				for(Xcnt=srcXcnt=0, Xdest=xd; Xcnt<wd; Xcnt++, Xdest++) {
					if( wP==0 ) {
						pSRC += TsstepX;
						wP = wPPP-(1.0-fXfirst);
						++srcXcnt;
					}
					if( wP<1.0 ) fXfirst = wP;
					else fXfirst = 1.0;

					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						if( fYfirst==1.0 || (srcYcnt+1)>=hs ) {
							if( fXfirst==1.0 || (srcXcnt+1)>=ws ) {
								*pDST = *pSRC;
								*(pDST+1) = *(pSRC+1);
								*(pDST+2) = *(pSRC+2);
							} else {
								*pDST = (BYTE)((double)(*pSRC)*fXfirst + (double)(*(pSRC+TsstepX))*(1.0-fXfirst));
								*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fXfirst + (double)(*(pSRC+TsstepX+1))*(1.0-fXfirst));
								*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fXfirst + (double)(*(pSRC+TsstepX+2))*(1.0-fXfirst));
							}
						} else {
							if( fXfirst==1.0 ) {
								*pDST = (BYTE)((double)(*pSRC)*fYfirst + (double)(*(pSRC+TsstepY))*(1.0-fYfirst));
								*(pDST+1) = (BYTE)((double)(*(pSRC+1))*fYfirst + (double)(*(pSRC+TsstepY+1))*(1.0-fYfirst));
								*(pDST+2) = (BYTE)((double)(*(pSRC+2))*fYfirst + (double)(*(pSRC+TsstepY+2))*(1.0-fYfirst));
							} else {
								r = fXfirst*fYfirst;// top-left piece
								g = (1.0-fXfirst)*fYfirst;// top-right piece
								b = fXfirst*(1.0-fYfirst);// bottom-left piece
								m = (1.0-fXfirst)*(1.0-fYfirst);// bottom-right piece
								*pDST = (BYTE)((double)(*pSRC)*r + (double)(*(pSRC+TsstepY))*b + (double)(*(pSRC+TsstepX))*g + (double)(*(pSRC+TsstepY+TsstepX))*m);
								*(pDST+1) = (BYTE)((double)(*(pSRC+1))*r + (double)(*(pSRC+TsstepY+1))*b + (double)(*(pSRC+TsstepX+1))*g + (double)(*(pSRC+TsstepY+TsstepX+1))*m);
								*(pDST+2) = (BYTE)((double)(*(pSRC+2))*r + (double)(*(pSRC+TsstepY+2))*b + (double)(*(pSRC+TsstepX+2))*g + (double)(*(pSRC+TsstepY+TsstepX+2))*m);
							}
						}
					}
					wP -= fXfirst;
					pDST += TdstepX;
				}
			}
			hP -= fYfirst;
		}
	} else if( ws>=wd && hs>=hd ) {	// compress X and compress Y
		wPPP = (double)ws/(double)wd;
		hPPP = (double)hs/(double)hd;

		pD = Tdpbits + TdstepY*yd + TdstepX*xd;
		pS = Tspbits + TsstepY*ys + TsstepX*xs;
		for(Ycnt=0, Ydest=yd; Ycnt<hd; Ycnt++, Ydest++) {
			pDST = pD + TdstepY*Ycnt;
			if( Ydest>=TdclipY1 && Ydest<TdclipY2 ) {
				srcYcnt = (int)floor((double)(hPPP*(double)Ycnt));
				for(Xcnt=0, Xdest=xd; Xcnt<wd; Xcnt++, Xdest++) {
					fYfirst = 1.0 - modf((hPPP*(double)Ycnt), &fysoff);	// how much of the first Y pixel line to use for this dest pixel
					srcXcnt = (int)floor((double)(wPPP*(double)Xcnt));
					pSRC = pS + TsstepY*srcYcnt + TsstepX*srcXcnt;
					r = g = b = d = 0.0;
					srcYcnt2 = srcYcnt;
					for(hP=hPPP; hP>0 && srcYcnt2<hs; ) {
						fXfirst = 1.0 - modf((wPPP*(double)Xcnt), &fxsoff);
						pSS = pSRC;
						srcXcnt2 = srcXcnt;
						for(wP=wPPP; wP>0 && srcXcnt2<ws; ) {
							m = fYfirst * fXfirst;
							b += (double)(*pSS) * m;
							g += (double)(*(pSS+1)) * m;
							r += (double)(*(pSS+2)) * m;
							d += m;
							pSS += TsstepX;
							++srcXcnt2;
							wP -= fXfirst;
							if( wP>=1.0 ) fXfirst = 1.0;
							else fXfirst = wP;
						}
						pSRC += TsstepY;
						++srcYcnt2;
						hP -= fYfirst;
						if( hP>=1.0 ) fYfirst = 1.0;
						else fYfirst = hP;
					}
					if( Xdest>=TdclipX1 && Xdest<TdclipX2 ) {
						*pDST = (BYTE)(double)(b/d);
						*(pDST+1) = (BYTE)(double)(g/d);
						*(pDST+2) = (BYTE)(double)(r/d);
					}
					pDST += TdstepX;
				}
			}
		}
	}
	KInvalidate(bmD, xd, yd, xd+wd, yd+hd);
}

//**************************************************************************
// Plot a point of a specific color in the destination bitmap
void PointBMB(PBMB bmD, int x, int y, DWORD clr) {

	BYTE	*pDST;

	SetupBMD(&bmD);
	if( x<TdclipX1 || x>=TdclipX2 || y<TdclipY1 || y>=TdclipY2 ) return;
	pDST = Tdpbits + TdstepY*y + TdstepX*x;
	*pDST++ = (BYTE)((clr & 0x0FF0000)>>16);
	*pDST++ = (BYTE)((clr & 0x0FF00)>>8);
	*pDST++ = (BYTE)(clr & 0x0FF);
	KInvalidate(bmD, x, y, x+1, y+1);
}

//**************************************************************************
// Draw a line between two points on the destination bitmap
void LineBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD clr) {

	BYTE	*pDST, r, g, b;
	int		x, y, d, w, s;
	PBMB	bmDO;

	bmDO = bmD;
	SetupBMD(&bmD);
	r = (BYTE)(clr & 0x000000FF);
	g = (BYTE)((clr & 0x0000FF00) >> 8);
	b = (BYTE)((clr & 0x00FF0000) >> 16);
	if( x1==x2 ) {	// VERTICAL line
		if( x1<TdclipX1 || x1>=TdclipX2 ) return;
	// if end-points are same, just plot single point
		if( y1<y2 ) {
			d = 1;
			if( y1>=TdclipY2 || y2<TdclipY1 ) return;
			if( y1<TdclipY1 ) y1 = TdclipY1;
			if( y2>=TdclipY2 ) y2 = TdclipY2-1;
			w = y2-y1+1;
			s = TdstepY;
		} else if( y1>y2 ) {
			d = -1;
			if( y2>=TdclipY2 || y1<TdclipY1 ) return;
			if( y2<TdclipY1 ) y2 = TdclipY1;
			if( y1>=TdclipY2 ) y1 = TdclipY2-1;
			w = y1-y2+1;
			s = -TdstepY;
		} else w = 0;
		if( w==0 ) {
			PointBMB(bmDO, x1, y1, clr);
			return;
		}
		pDST = Tdpbits + TdstepY*y1 + TdstepX*x1;
		for(y=y1; w--; y += d) {
			*pDST = b;
			*(pDST+1) = g;
			*(pDST+2) = r;
			pDST += s;
		}
		++x2;	// for invalidate
	} else if( y1==y2 ) {	// HORIZONTAL line
		if( y1<TdclipY1 || y1>=TdclipY2 ) return;
	// if end-points are same, just plot single point
		if( x1<x2 ) {
			d = 1;
			if( x1>=TdclipX2 || x2<TdclipX1 ) return;
			if( x1<TdclipX1 ) x1 = TdclipX1;
			if( x2>=TdclipX2 ) x2 = TdclipX2-1;
			w = x2-x1+1;
			s = TdstepX;
		} else if( x1>x2 ) {
			d = -1;
			if( x2>=TdclipX2 || x1<TdclipX1 ) return;
			if( x2<TdclipX1 ) x2 = TdclipX1;
			if( x1>=TdclipX2 ) x1 = TdclipX2-1;
			w = x1-x2+1;
			s = -TdstepX;
		} else w = 0;
		if( w==0 ) {
			PointBMB(bmDO, x1, y1, clr);
			return;
		}
		pDST = Tdpbits + TdstepY*y1 + TdstepX*x1;
		for(x=x1; w--; x += d) {
			*pDST = b;
			*(pDST+1) = g;
			*(pDST+2) = r;
			pDST += s;
		}
		++y2;	// for invalidate
	} else {	// DIAGONAL line
		int dX, dY, Xincr, Yincr, dPr, dPru, P, Ax, Ay;

		pDST = Tdpbits + TdstepY*y1 + TdstepX*x1;
		if( x1 > x2 ) {
			Xincr = -1;
			TdstepX = -TdstepX;
		} else Xincr = 1;

		if( y1 > y2 ) {
			Yincr = -1;
			TdstepY = -TdstepY;
		} else Yincr = 1;
		Ax = x1;
		Ay = y1;
		dX = abs(x2-x1);
		dY = abs(y2-y1);
		if( dX >= dY ) {
			dPr = dY<<1;
			dPru = dPr - (dX<<1);
			P = dPr - dX;
			for(; dX >= 0; dX--) {
				if( Ax>=TdclipX1 && Ax<TdclipX2 && Ay>=TdclipY1 && Ay<TdclipY2 ) {
					*pDST = b;
					*(pDST+1) = g;
					*(pDST+2) = r;
				}
				if( P > 0 ) {
					Ay += Yincr;
					pDST += TdstepY;
					P += dPru;
				} else P += dPr;
				Ax += Xincr;
				pDST += TdstepX;
			}
		} else {
			dPr = dX<<1;
			dPru = dPr - (dY<<1);
			P = dPr - dY;
			for(; dY >= 0; dY--) {
				if( Ax>=TdclipX1 && Ax<TdclipX2 && Ay>=TdclipY1 && Ay<TdclipY2 ) {
					*pDST = b;
					*(pDST+1) = g;
					*(pDST+2) = r;
				}
				if( P > 0 ) {
					Ax += Xincr;
					pDST += TdstepX;
					P += dPru;
				} else P += dPr;
				Ay += Yincr;
				pDST += TdstepY;
			}
		}
	}
	KInvalidate(bmD, x1, y1, x2, y2);
}

//**************************************************************************
// Draw a rectangle in the destination bitmap.  The rectangle may be
// SOLID, OUTLINED, or BOTH, depending upon the values of 'fill' and 'edge'.
// if 'fill' is not -1, fill the rectangle with that color
// if 'edge' is not -1, outline the rectangle with that color
void RectBMB(PBMB bmD, int x1, int y1, int x2, int y2, DWORD fill, DWORD edge) {

	BYTE	*pDST, *pD, r, g, b;
	int		x, y, w, h, ww, edged;
	PBMB	bmDO;

	if( x1>x2 ) {
		x = x1;
		x1 = x2;
		x2 = x;
	}
	if( y1>y2 ) {
		y = y1;
		y1 = y2;
		y2 = y;
	}
	bmDO = bmD;
	SetupBMD(&bmD);
	edged = (edge==-1)?0:1;
	if( fill != -1 ) {
		x = Kmax(TdclipX1, x1+edged);
		y = Kmax(TdclipY1, y1+edged);
		w = Kmin(TdclipX2, x2-edged)-x;
		h = Kmin(TdclipY2, y2-edged)-y;
		if( w>0 && h>0 ) {
			r = (BYTE)(fill & 0x000000FF);
			g = (BYTE)((fill & 0x0000FF00) >> 8);
			b = (BYTE)((fill & 0x00FF0000) >> 16);
			pD = Tdpbits + TdstepY*y + TdstepX*x;
			while( h-- ) {
				pDST = pD;
				for(ww=w; ww--; ) {
					*pDST = b;
					*(pDST+1) = g;
					*(pDST+2) = r;
					pDST += TdstepX;
				}
				pD += TdstepY;
			}
		}
	}
	if( edged ) {
		--x2;
		--y2;
		LineBMB(bmDO, x1, y1, x2, y1, edge);
		LineBMB(bmDO, x2, y1, x2, y2, edge);
		LineBMB(bmDO, x2, y2, x1, y2, edge);
		LineBMB(bmDO, x1, y2, x1, y1, edge);
		++x2;
		++y2;
	}
	KInvalidate(bmD, x1, y1, x2, y2);
}

//********************************************************
void DrawHiLoBox(PBMB hBM, long x, long y, long w, long h, DWORD clrhi, DWORD clrlo) {

	LineBMB(hBM, x, y+h-1, x, y, clrhi);
	LineBMB(hBM, x, y, x+w-1, y, clrhi);
	LineBMB(hBM, x+w-1, y+1, x+w-1, y+h-1, clrlo);
	LineBMB(hBM, x+w-1, y+h-1, x+1, y+h-1, clrlo);

	++x;
	++y;
	w -= 2;
	h -= 2;
	LineBMB(hBM, x, y+h-1, x, y, clrhi);
	LineBMB(hBM, x, y, x+w-1, y, clrhi);
	LineBMB(hBM, x+w-1, y+1, x+w-1, y+h-1, clrlo);
	LineBMB(hBM, x+w-1, y+h-1, x+1, y+h-1, clrlo);
}

//***************************************************************
//* SupportStartup() is called from MAIN during startup, and is *
//* a chance to do any one-time initialization stuff.			*
//***************************************************************
void SupportStartup() {

	iniRead(iniName);
}

//***********************************************************
//* SupportCleanup() is called when HP-85 is about to exit.	*
//* It is responsible for deleting/freeing all resources	*
//* which were allocated and haven't already been freed.	*
//***********************************************************
void SupportShutdown() {

	PBMB	pBM, pBMnext;

	iniWrite(iniName);
	iniClose();

	for(pBM=pbFirst; pBM!=NULL; ) {
		pBMnext = (PBMB)pBM->pNext;
		DeleteBMB(pBM);
		pBM = pBMnext;
	}
}

//*****************************************************************
//* ssWBegin() is called from HP85OnShutdown() to open/create the EM85.CFG file for writing.
//* ssRBegin() is called from HP85OnStartup() to open the Series80.ini file for reading.
//* ssEnd() is called from HP85OnShutdown() and HP85OnStartup() to close the Series80.ini after it's been read or written.
//*****************************************************************
#if 0
long	hssFile=-1;
char	ssBuf[260], *pssIN=NULL, *pssINP;
//* ssFirstVal controls whether (FALSE) a ' ' or (TRUE) a '{' is written out IN FRONT OF each VALUE.
BOOL	ssFirstVal=TRUE;
//* ssEndBlockEndBlock determines whether (FALSE) just a '}' is written out (end of line) or (TRUE, ssWBlockEnd() called twice back-to-back) CR/LF is written followed by TABS followed by '}' followed by CR/LF
long	ssEndBlockCnt, ssBeginBlockCnt;
long	ssBlockLevel, ssValLevel;
char	ssCFGname[]="Series80.ini";

//*****************************************************************
void ssEnd() {

	if( hssFile!=-1 ) {
		Kclose(hssFile);
		hssFile = -1;
	}
	if( pssIN!=NULL ) {
		free(pssIN);
		pssIN = NULL;
	}
}

//****************************************************************
//****************************************************************
//****************************************************************
//*** WRITING
//****************************************************************
//****************************************************************
//****************************************************************
BOOL ssWBegin() {

	ssBlockLevel = ssValLevel = ssEndBlockCnt = ssBeginBlockCnt = 0;
	if( hssFile!=-1 ) Kclose(hssFile);
	if( -1 == (hssFile=Kopen(ssCFGname, O_WRBINNEW)) ) return FALSE;
	return TRUE;
}

//*****************************************************************
void ssWBlockBegin(char *str) {

	int		i;

	if( ssEndBlockCnt || ssBeginBlockCnt ) Kwrite(hssFile, (BYTE*)"\r\n", 2);
	for(i=0, ssBuf[0]=0; i<ssBlockLevel; i++) strcat(ssBuf, "\t");
	sprintf(ssBuf+strlen(ssBuf), "{%s ", str);
	Kwrite(hssFile, (BYTE*)ssBuf, strlen(ssBuf));
	++ssBlockLevel;
	ssEndBlockCnt = 0;
	++ssBeginBlockCnt;
	ssFirstVal = FALSE;
}

void ssWBlockEnd() {

	int		i;

	if( ssBlockLevel ) {
		--ssBlockLevel;
		if( ssEndBlockCnt ) {
			Kwrite(hssFile, (BYTE*)"\r\n", 2);
			for(i=0; i<ssBlockLevel; i++) Kwrite(hssFile, (BYTE*)"\t", 1);
			Kwrite(hssFile, (BYTE*)"}", 1);
		} else Kwrite(hssFile, (BYTE*)"}", 1);
		++ssEndBlockCnt;
		ssBeginBlockCnt = 0;
	} else SystemMessage("ssWBlockEnd() before ssWBlockBegin()");
}

//*****************************************************************
void ssWValuesBegin() {

	if( ssFirstVal ) {	// already have a pending '{', because we're going to have another pending '{'
		Kwrite(hssFile, (BYTE*)"{", 1);

	}
	ssFirstVal = TRUE;
	++ssValLevel;
}

void ssWValuesEnd() {

	if( ssValLevel ) {
		if( ssFirstVal ) Kwrite(hssFile, (BYTE*)"{", 1);	// no data in the value block
		Kwrite(hssFile, (BYTE*)"}", 1);
		--ssValLevel;
	} else SystemMessage("ssWValuesEnd() before ssWValuesBegin()");
}

//*****************************************************************
void ssWValueDec(long val) {

	if( ssValLevel ) {
		sprintf(ssBuf, "%c%ld", ssFirstVal?'{':' ', val);
		Kwrite(hssFile, (BYTE*)ssBuf, strlen(ssBuf));
		ssFirstVal = FALSE;
	} else SystemMessage("ssWValueDec() before ssWValuesBegin()");
}

//*****************************************************************
void ssWValueOct(long val) {

	if( ssValLevel ) {
		sprintf(ssBuf, "%c0%lo", ssFirstVal?'{':' ', val);
		Kwrite(hssFile, (BYTE*)ssBuf, strlen(ssBuf));
		ssFirstVal = FALSE;
	} else SystemMessage("ssWValueOct() before ssWValuesBegin()");
}

//*****************************************************************
void ssWValueHex(DWORD val) {

	if( ssValLevel ) {
		sprintf(ssBuf, "%c0x%8.8lX", ssFirstVal?'{':' ', val);
		Kwrite(hssFile, (BYTE*)ssBuf, strlen(ssBuf));
		ssFirstVal = FALSE;
	} else SystemMessage("ssWValueDec() before ssWValuesBegin()");
}

void ssWValueStr(char *str) {

	long	s, d;

	if( ssValLevel ) {
		strcpy(ssBuf, ssFirstVal?"{\"":" \"");
		Kwrite(hssFile, (BYTE*)ssBuf, 2);
		if( strlen(str)>128 ) SystemMessage("ssWValueStrUser() string longer than 128 characters");
		else {
			for(s=d=0; str[s]; ) {
				if( str[s]=='"' || str[s]=='\\' ) ssBuf[d++]='\\';
				ssBuf[d++] = str[s++];
			}
			ssBuf[d++] = '"';
			ssBuf[d] = 0;
			Kwrite(hssFile, (BYTE*)ssBuf, d);
		}
		ssFirstVal = FALSE;
	} else SystemMessage("ssWValueStr() before ssWValuesBegin()");
}

//****************************************************************
//****************************************************************
//****************************************************************
//*** READING
//****************************************************************
//****************************************************************
//****************************************************************
BOOL ssRBegin() {

	long	s;

	ssBlockLevel = ssValLevel = 0;
	if( hssFile!=-1 ) Kclose(hssFile);
	if( -1 == (hssFile=Kopen(ssCFGname, O_RDBIN)) ) return FALSE;
	s = Kseek(hssFile, 0, SEEK_END);
	// one extra for NUL on last line in case no CR/LF on last line
	if( NULL == (pssIN = (char *)malloc(s+1)) ) {
		Kclose(hssFile);
		return FALSE;
	}
	Kseek(hssFile, 0, SEEK_SET);
	Kread(hssFile, (BYTE*)pssIN, s);
	Kclose(hssFile);
	pssINP = pssIN;

	return TRUE;
}

void ssRSkipWhite() {

	while( *pssINP && (*pssINP==' ' || *pssINP=='\t' || *pssINP=='\r' || *pssINP=='\n') ) ++pssINP;
}

// chews white space, gets non-{ non-} non-white-space word into ssBuf, or ssBuf[0]==0.
void ssRGetWord() {

	char	*pB;

	pB = ssBuf;
	ssRSkipWhite();
	while( *pssINP && *pssINP!=' ' && *pssINP!='\t' && *pssINP!='\r' && *pssINP!='\n' && *pssINP!='{' && *pssINP!='}' ) *pB++ = *pssINP++;
	*pB = 0;
}

//*****************************************************************
// Chews up an expected { and gets the expected following keyword/variable into ssBuf.
// Either returns a pointer to the keyword (in ssBuf) or ssBuf[0]==0.
char *ssRBlockBegin() {

	ssBuf[0] = 0;
	ssRSkipWhite();
	if( *pssINP=='{' ) {
		++pssINP;
		ssRGetWord();
		++ssBlockLevel;
	}
	return ssBuf;
}

// Same as ssRBlockBegin() excepts returns index into IniStr of matching string (or 0).
long ssRBlockBeginIni() {

	long	i;

	ssRBlockBegin();
	for(i=1; i<IniStrCnt; i++) if( !stricmp(IniStr[i], ssBuf) ) return i;
	return 0;
}

long ssRValueStrIni() {

	long	i;
	char	*pB;

	ssBuf[0] = 0;
	ssRSkipWhite();
	if( *pssINP=='"' ) {
		++pssINP;
		pB = ssBuf;
		while( (pB-ssBuf)<256 && *pssINP && *pssINP!='"' && *pssINP>=' ' && (*pssINP!='\\' || *(pssINP+1)) ) {
			if( *pssINP=='\\' ) ++pssINP;
			*pB++ = *pssINP++;
		}
		*pB = 0;
		if( *pssINP=='"' ) {
			++pssINP;
			for(i=1; i<IniStrCnt; i++) if( !stricmp(IniStr[i], ssBuf) ) return i;
		}
	}
	return 0;
}

char *ssRValueStr() {

	char	*pB;

	ssBuf[0] = 0;
	ssRSkipWhite();
	if( *pssINP=='"' ) {
		++pssINP;
		pB = ssBuf;
		while( (pB-ssBuf)<256 && *pssINP && *pssINP!='"' && *pssINP>=' ' && (*pssINP!='\\' || *(pssINP+1)) ) {
			if( *pssINP=='\\' ) ++pssINP;
			*pB++ = *pssINP++;
		}
		*pB = 0;
		if( *pssINP=='"' ) ++pssINP;
	}
	return ssBuf;
}

BOOL ssRIsEnd() {

	ssRSkipWhite();
	return !(*pssINP);
}

BOOL ssRIsEndBlock() {

	ssRSkipWhite();
	return *pssINP=='}';
}

BOOL ssRBlockEnd() {

	ssRSkipWhite();
	if( *pssINP!='}' ) return FALSE;
	++pssINP;
	--ssBlockLevel;
	return TRUE;
}

BOOL ssRValuesBegin() {

	ssRSkipWhite();
	if( *pssINP!='{' ) return FALSE;
	++pssINP;
	++ssValLevel;
	return TRUE;
}

BOOL ssRIsValue() {

	char	c;

	ssRSkipWhite();
	c = *pssINP;
	if( (c>='0' && c<='9') || c=='"' ) return TRUE;
	return FALSE;
}

BOOL ssRValuesEnd() {

	ssRSkipWhite();
	if( *pssINP!='}' ) return FALSE;
	++pssINP;
	--ssValLevel;
	return TRUE;
}

BOOL ssRValueNum(long *pNum) {

	long	val;
	char	c;

	ssRSkipWhite();
	c = *pssINP;
	if( c<'0' || c>'9' ) return FALSE;
	val = 0;
	if( c=='0' ) {	// OCT or HEX
		++pssINP;
		if( *pssINP=='x' ) {	// HEX
			++pssINP;
			c = toupper(*pssINP);
			if( (c>='0' && c<='9') || (c>='A' && c<='F') ) {
				do {
					c = toupper(*pssINP++) - '0';
					if( c>9 ) c -= 7;
					val = 16*val + (long)c;
					c = toupper(*pssINP);
				} while( (c>='0' && c<='9') || (c>='A' && c<='F') );
			} else return FALSE;
		} else {	// OCT
			--pssINP;
			do {
				c = *pssINP++;
				val = 8*val + (c-'0');
			} while( *pssINP>='0' && *pssINP<='7' );
			if( *pssINP=='8' || *pssINP=='9' ) return FALSE;
		}
	} else {	// DEC
		do {
			c = *pssINP++;
			val = 10*val + (c-'0');
		} while( *pssINP>='0' && *pssINP<='9' );
	}
	*pNum = val;
	return TRUE;
}

void ReadIniError() {

	long	i;

	for(i=0; pssINP[i] && i<50; i++) ssBuf[i] = pssINP[i];
	ssBuf[i] = 0;
	sprintf((char*)ScratBuf, "Series80.ini error at: %s", ssBuf);
	SystemMessage((char*)ScratBuf);
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

char	*INIname="emu.ini";
char	*INIbuf=NULL;
char	*INIp;
char	INItmp[260];

long	INIfile=-1;

long	INIlevel;
long	INIrw=0;	// -1 if we're reading, +1 if we're writing, 0 if neither
BOOL	INIneedEOL;	// TRUE if we've written part of a variable and need a CR/LF before starting another variable or section

//*****************************************************************
// start WRITING
BOOL INIwrite() {

	INIlevel = 0;
	INIneedEOL = FALSE;
	if( INIfile!=-1 ) Kclose(INIfile);
	if( -1 == (INIfile=Kopen(INIname, O_WRBINNEW)) ) {
		INIrw = 0;
		return FALSE;
	}
	INIrw = 1;
	return TRUE;
}

//*****************************************************************
// start READING
BOOL INIread() {

	long	s;

	INIlevel = 0;
	INIneedEOL = FALSE;
	if( INIfile!=-1 ) Kclose(INIfile);
	INIrw = 0;
	if( -1 == (INIfile=Kopen(INIname, O_RDBIN)) ) return FALSE;
	s = Kseek(INIfile, 0, SEEK_END);
	// one extra for NUL on last line in case no CR/LF on last line
	if( NULL == (INIbuf = (char *)malloc(s+1)) ) {
		Kclose(INIfile);
		return FALSE;
	}
	Kseek(INIfile, 0, SEEK_SET);
	Kread(INIfile, (BYTE*)INIbuf, s);
	INIbuf[s] = 0;
	Kclose(INIfile);
	INIp = INIbuf;
	INIrw = -1;

	return TRUE;
}

//*****************************************************************
// WRITING: if INIneedEOL is true, write out an EOL sequence
void INIendline() {

	if( INIneedEOL ) {
		Kwrite(INIfile, (BYTE*)"\r\n", 2);
		INIneedEOL = FALSE;
	}
}

//*****************************************************************
// WRITING or READING
void INIclose() {

	INIendline();	// in case we're writing
	if( INIfile!=-1 ) {
		Kclose(INIfile);
		INIfile = -1;
	}
	if( INIbuf!=NULL ) {	// in case we're reading
		free(INIbuf);
		INIbuf = NULL;
	}
	INIrw = 0;
}

//*****************************************************************
// READING: skip white space, including CR and LF.
void INIskipwhiteALL() {

	if( INIrw<0 ) while( *INIp && (*INIp==' ' || *INIp=='\t' || *INIp=='\r' || *INIp=='\n') ) ++INIp;
}

//*****************************************************************
// READING: skip white space, but stop if CR or LF is encounter.
void INIskipwhiteLINE() {

	if( INIrw<0 ) while( *INIp && (*INIp==' ' || *INIp=='\t') ) ++INIp;
}

//*****************************************************************
// WRITING: Start a new section
void INIsection(long level, char *name) {

	long	i;

	INIendline();
	if( level + strlen(name) + 15 < sizeof(INItmp) ) {
		for(i=0; i<level; i++) INItmp[i] = '\t';
		INItmp[i] = 0;
		sprintf(INItmp+strlen(INItmp), "[%ld %s]\r\n", level, name);
		Kwrite(INIfile, (BYTE*)INItmp, strlen(INItmp));
		INIlevel = level;
		INIneedEOL = FALSE;
	}
}

//*****************************************************************
// WRITING: Start a new variable
void INIvar(char *name) {

	long	i;

	INIendline();
	if( INIlevel + 1 + strlen(name) < sizeof(INItmp) ) {
		for(i=0; i<=INIlevel; i++) INItmp[i] = '\t';
		INItmp[i] = 0;
		sprintf(INItmp+strlen(INItmp), "%s=", name);
		Kwrite(INIfile, (BYTE*)INItmp, strlen(INItmp));

		INIneedEOL = TRUE;
	} else SystemMessage("INIvar() name too long");
}

//*****************************************************************
// WRITING: output an octal number
void INIputOCT(long val) {

	if( INIneedEOL ) {
		sprintf(INItmp, "0%lo ", val);
		Kwrite(INIfile, (BYTE*)INItmp, strlen(INItmp));
	} else SystemMessage("INIputOCT() output before var");
}

//*****************************************************************
// WRITING: output a decimal number
void INIputDEC(long val) {

	if( INIneedEOL ) {
		sprintf(INItmp, "%ld ", val);
		Kwrite(INIfile, (BYTE*)INItmp, strlen(INItmp));
	} else SystemMessage("INIputDEC() output before var");
}

//*****************************************************************
// WRITING: output a hex number
void INIputHEX(DWORD val) {

	if( INIneedEOL ) {
		sprintf(INItmp, "0x%8.8lX ", val);
		Kwrite(INIfile, (BYTE*)INItmp, strlen(INItmp));
	} else SystemMessage("INIputHEX() output before var");
}

//*****************************************************************
// WRITING: output a string
void INIputSTR(char *str) {

	char	*d;

	if( INIneedEOL ) {
		if( strlen(str)<128 ) {
			d = INItmp;
			*d++ = '"';
			for(; *str; ) {
				if( *str=='"' || *str=='\\' ) *d++='\\';
				*d++ = *str++;
			}
			*d++ = '"';
			*d = 0;
			Kwrite(INIfile, (BYTE*)INItmp, strlen(INItmp));
		} else SystemMessage("INIputSTR() string too long");
		ssFirstVal = FALSE;
	} else SystemMessage("INIputSTR() output before var");

	Kwrite(INIfile, (BYTE*)str, strlen(str));
}
#endif // 0

//************************************************************
//************************************************************
//************************************************************
long	INIsize, INIused;
char	*INIbuf=NULL;
char	*pINIsec, *pINIvar;

#define	INIwhite(s)			(*(s)==' ' || *(s)=='\t')
#define	INIeol(s)			(*(s)=='\r' || *(s)=='\n')
#define	INIwhiteALL(s)		(INIwhite(s) || INIeol(s))
#define	INIskipwhite(s) 	{while( INIwhite(s) ) ++s;}
#define	INIskipline(s)  	{while( *s && !INIeol(s) ) ++s; while( *s && INIeol(s) ) ++s; }
#define	INIskipwhiteALL(s)	{\
								do {\
									INIskipwhite(s);\
									if( INIeol(s) ) INIskipline(s);\
								} while( INIwhiteALL(s) );\
							}

#define	INIchunkslop		8192

/*************************************************
 If SECTION is found:
	pINIsec = ptr to '[' at beginning of section
	pINIvar = NULL
	returns TRUE
 Else
	pINIsec = ptr to NUL at EOF
	pINIvar = NULL
	returns FALSE
 *************************************************/
BOOL FindSec(char *srchname) {

	size_t	len;
	char	*pSecName=NULL;

//char tttbuf[128];
	if( INIbuf != NULL ) {
		len = strlen(srchname);
		pINIvar = NULL;	// need a iniFindVariable before iniGetNum or iniGetStr
		for(pINIsec=INIbuf; *pINIsec; ) {
//sprintf(tttbuf,"@sec(len=%d) %16.16s", *((long*)(pINIsec+1)), pINIsec+12);
//SystemMessage(tttbuf);
			if( *pINIsec=='[' ) {
//SystemMessage("FS got [");
				pSecName = pINIsec + 12;	// skip [0xCHECKSUM=
//sprintf(tttbuf,"FS pSecName=%16.16s", pSecName);
//SystemMessage(tttbuf);
				if( !strnicmp(pSecName, srchname, len) ) {	// if found a section with the same STARTING characters
//SystemMessage("FS name FIRST match");
					if( *(pSecName+len)==']' ) return TRUE;	// if found matching name doesn't have any additional trailing characters, SUCCESS!
//SystemMessage("FS name DID NOT match");
				}
			}
			pINIsec += *((long*)(pINIsec+1));
		}
	}
	// leave pINIsec pointing at End Of File
	return FALSE;
}

/********************************************************
 Finds a variable in the current SECTION, leaving pINIvar
 EITHER pointing to start of variable name AND returning length of old variable line (including end-of-line sequence),
 OR pointing to the '[' that starts the next section OR pointing to the EOF NUL char AND returning 0.
  ********************************************************/
long FindVar(char *srchvar) {

	long	oldlen, namlen;
	char	*pINItmp;

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		namlen = strlen(srchvar);
		pINIvar = pINIsec + 13;
		INIskipline(pINIvar);	// skip the section line
		while( *pINIvar ) {
			INIskipwhiteALL(pINIvar);
			if( *pINIvar=='[' ) return 0;	// all new variable, didn't exist before
			if( !strnicmp(pINIvar, (char*)srchvar, namlen) ) {	// found beginning of old variable (at least)
				pINItmp = pINIvar + namlen;
				if( *pINItmp=='=' ) {	// found old variable
					INIskipline(pINItmp);
					oldlen = pINItmp - pINIvar;
					// pINIvar now points to the beginning of the VAR line and oldlen is its length
					return oldlen;
				}
				// start of found variable matched, but is longer than what we're looking for
			}
			INIskipline(pINIvar);
		}
	}
	return 0;
}

/************************************/
BOOL INIopenHole(char *s, long len) {

	char	*m;

	if( INIused+(len) > INIsize ) {	// need to increase size of buffer
		if( NULL==(m=(char*)malloc(INIsize+len+INIchunkslop)) ) return FALSE;
		memcpy(m, INIbuf, s-INIbuf);	// copy stuff above the hole
		memcpy(m+(s-INIbuf)+len, s, INIbuf+INIused-s);	// copy stuff below the hole
		if( pINIsec!=NULL ) pINIsec = m + (pINIsec-INIbuf) + ((pINIsec>s) ? len : 0);
		if( pINIvar!=NULL ) pINIvar = m + (pINIvar-INIbuf) + ((pINIvar>s) ? len : 0);
		free(INIbuf);
		INIbuf = m;
		INIsize += len+INIchunkslop;
		INIused += len;
	} else {
		memmove(s+len, s, INIused - (s-INIbuf));
		if( pINIsec!=NULL && pINIsec>s ) pINIsec += len;
		if( pINIvar!=NULL && pINIvar>s ) pINIvar += len;
		INIused += len;
	}
	return TRUE;
}

/************************************/
void INIcloseHole(char *s, long len) {

	if( INIused-(s-INIbuf) < len ) len = INIused-(s-INIbuf);
	memcpy(s, s+len, INIused-(s-INIbuf+len));
	if( pINIsec!=NULL && pINIsec>=s+len ) pINIsec -= len;
	if( pINIvar!=NULL && pINIvar>=s+len ) pINIvar -= len;
	INIused -= len;
}

/***************************************/
BOOL GetOctDecHex(char **pp, DWORD *pv) {

	char	*p, c;
	BOOL	retval;
	DWORD	val;

	p = *pp;
	retval = FALSE;
	val = 0;

	c = *p;
	if( c>='0' && c<='9' ) {
		if( c=='0' ) {	// OCT or HEX
			++p;
			c = *p;
			if( toupper(c)=='X' ) {	// HEX
				++p;
				c = toupper(*p);
				while( (c>='0' && c<='9') || (c>='A' && c<='F') ) {
					++p;
					c -= '0';
					if( c>9 ) c -= 7;
					val = 16*val + (DWORD)c;
					c = toupper(*p);
				}
			} else {	// OCT
				--p;
				c = '0';
				do {
					++p;
					val = 8*val + (c-'0');
					c = *p;
				} while( c>='0' && c<='7' );
			}
		} else {	// DEC
			do {
				++p;
				val = 10*val + (c-'0');
				c = *p;
			} while( c>='0' && c<='9' );
		}
		retval = TRUE;
	}
	*pv = val;
	*pp = p;
	return retval;
}

/*****************************************************************
 Calculate a checksum for buffer p (length 'len') and return it. */
DWORD GetChksum(char *p, DWORD len) {

	DWORD	dw, sum;

	dw = len>>2;	// number of whole DWORDS
	sum = 0;
	while( dw-- ) {
		sum += *((DWORD*)p);
		p += 4;
	}
	len &= 3;
	dw = 0;
	while( len-- ) dw = (dw<<8) | *p++;
	sum += dw;

	return sum;
}

/***
	.INI FILE HANDLING

	NOTE: Do NOT use '[', ']', or '=' in NAMES of sections or variables.
	There is no checking for that situation, and if you do, hell will rain down upon your head with a liquified garlic and harbenero sautee...
	When formatting the string value of a variable for the iniSetVariable call, you MUST surround all string-values with double-quotes (ASCII 34)
	and any 'internal' double-quotes within a string value must be preceded by a backslash (ASCII 92), ie, "abc\"def".  A backslash character may be included
	by preceding it with another backslash (ie, \\).  Numbers may be formatted as octal (leading ZERO 0 character), decimal (NO leading 0 character),
	or hex (leading 0x or 0X, ie, 0xFFFF).  Numbers are NOT quoted.

	When in the disk file, the iniSec header looks like     [0xCHECKSUM=HeaderName]
	When in memory, in INIbuf, the iniSec header looks like [LLLLfxxxxx=HeaderName]
		where:
			LLLL is a DWORD value that is the LENGTH of this section, from the '[' at the start of this section to the '[' at the start of the next section OR the EOF.
			f is a byte of FLAGS, currently which is all 0 bits except the LSbit which MAY be 1 if the checksum didn't match for this section when it was read in.
			xxxxx are unused bytes, currently undefined.
		This transformation takes place when the file is read into INIbuf from disk,
		and the LLLL length is maintained as the buffer is manipulated.
		ALL of these values get overwritten by the checksum during iniWrite, although they are saved and restored
			so that iniWrite doesn't destroy the information in INIbuf (for continued use).
	The application has NO idea that this is going on behind the scenes, as its only interface with the INIbuf
	is via the functions below, which do NOT give direct access to the buffer.
 ***/

//==============================================================
// returns:
//	-1 = error opening file, buffer created
//	 0 = file opened and read ok, chksum matched
//	 1 = file opened but read error
//	 2 = file opened and read, but chksum error
long iniRead(BYTE *fname) {

	DWORD	hFile, chksum, seclen, filsum;
	BYTE	*pWsec, *pSum, *pNext;
	BOOL	gotchksum;
	long	s, r, retval, i;

	retval = 1;	// preset to FAIL

	if( INIbuf!=NULL ) {
		free(INIbuf);
		INIbuf = NULL;
	}
	pINIvar = pINIsec = NULL;
	if( -1!=(hFile=Kopen((char*)fname, O_RDBIN)) ) {
		s = Kseek(hFile, 0, SEEK_END);
		// one extra for NUL on last line in case no CR/LF on last line
		INIsize = s+1;
		if( s>=10 ) {
			if( NULL != (INIbuf = (char *)malloc(INIsize)) ) {
				Kseek(hFile, 0, SEEK_SET);
				r = Kread(hFile, (BYTE*)INIbuf, s);

				INIbuf[r] = 0;	// place NUL at end of buffer
				INIused = r+1;
				retval = 0;	// success

				pINIsec =INIbuf;
				INIskipwhiteALL(pINIsec);
				pWsec = (BYTE*)pINIsec;
				while( *pWsec && *pWsec=='[' ) {
					// find len of each section, checksum it, and compare it to the stored checksum.
					// if checksums don't match, set flag bit0.
					pNext = pWsec+1;	// skip the '[', point at the file's checksum for this section
					gotchksum = GetOctDecHex((char**)&pNext, &filsum);	// get the checksum from the file
					pNext = pWsec+1;				// point at the start of the checksum area
					for(i=0; i<10; i++) *pNext++ = 0;	// zero-out the checksum area
					for(pSum=pWsec; pSum<pNext || (*pSum && *pSum!='['); pSum++) ;	// find the end of this SECTION
					seclen = pSum - pWsec;			// get the length of this SECTION
					chksum = GetChksum((char*)pWsec, seclen);	// get the actual checksum of this SECTION
					*((DWORD*)(pWsec+1)) = seclen;	// set the length of this SECTION
					if( !gotchksum || chksum != filsum ) {
						pWsec[5] = 1;	// if the checksums don't match, set the BAD CHECKSUM flag
						retval = 2;
					}
					pWsec += seclen;
				}
				if( r!=s && retval==0 ) retval = 1;
			}
			Kclose(hFile);
			return retval;
		}
	}
	INIbuf = (char*)malloc(INIchunkslop);
	*INIbuf = 0;
	INIsize = INIchunkslop;
	INIused = 1;
	return -1;	// set to didn't exist, buffer created
}

//==============================================================
// returns:
//	1 = success
//	0 = error
long iniWrite(BYTE *fname) {

	DWORD	hFile, chksum, seclen;
	char	chkbuf[16], savbuf[10];
	BYTE	*pWsec, *pSum;
	long	retval, i;

	retval = 1;	// preset to SUCCESS
	if( INIbuf != NULL ) {
		if( -1!=(hFile=Kopen((char*)fname, O_WRBINNEW)) ) {
			pWsec = (BYTE*)INIbuf;
			// for each section
			while( *pWsec=='[' ) {
				// save the checksum "internal variables" area and zero it out
				pSum = pWsec+1;
				for(i=0; i<10; i++) {
					savbuf[i] = pSum[i];
					pSum[i] = 0;
				}
				// now checksum the section
				seclen = *((DWORD*)savbuf);	// get the section LENGTH
				chksum = GetChksum((char*)pWsec, seclen);
				sprintf(chkbuf, "0x%8.8lX", chksum);
				// and write its ASCII value into the checksum area for this section
				for(i=0; i<10; i++) pSum[i] = chkbuf[i];
				// and write this section to the file
				if( seclen!=Kwrite(hFile, (BYTE*)pWsec, seclen) ) retval = 0;	// failed
				// now restore "internal variables" area
				for(i=0; i<10; i++) pSum[i] = savbuf[i];
				// now move to the next section
				pWsec += seclen;
			}
			Kclose(hFile);
		} else retval = 0;	// failed
	} else retval = 0;	// failed
	return retval;
}

//==============================================================
void iniClose() {

	if( INIbuf!=NULL ) {
		free(INIbuf);
		INIbuf = NULL;
	}
}

//==============================================================
// returns:
//	1 = var found
//	0 = not found
long iniFindSec(BYTE *str) {

	size_t	len;
	char	*pSec;
	long	retval;

	len = strlen((char*)str);
	retval = 0;	// preset to FAIL
	pINIvar = NULL;	// need a iniFindVar before iniGetNum or iniGetStr
	if( INIbuf!=NULL ) {
		for(pINIsec=INIbuf; *pINIsec; ) {
			if( *pINIsec=='[' ) {
				pSec = pINIsec + 12;	// skip [0xCHECKSUM=
				if( !strnicmp(pSec, (char*)str, len) && *(pSec+len)==']' ) {
					retval = 1;	// signal success
					break;
				}
			}
			pINIsec += *((long*)(pINIsec+1));
		}
	}
	return retval;
}

//==============================================================
// returns:
//	1 = var found
//	0 = not found
long iniFindVar(BYTE *str) {

	size_t	len;
	long	retval=0;

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		len = FindVar((char*)str);
		if( len ) {
			while( *pINIvar && *pINIvar++ != '=' ) ;	// skip to variable values, now ready for iniGetNum and/or iniGetStr
			retval = 1;	// signal success
		}
	}
	return retval;
}

//==============================================================
// returns:
//	1 = number found
//	0 = not found
long iniGetNum(DWORD *var) {


	long	sign, retval=0;
	DWORD	val;

	sign = 0;
	if( INIbuf!=NULL && pINIvar!=NULL ) {	// must have a buffer, must have done an iniFindVariable
		INIskipwhite(pINIvar);
		if( *pINIvar=='-' ) {
			sign = 1;
			++pINIvar;
		}
		if( GetOctDecHex(&pINIvar, &val) ) retval = 1;
		if( sign ) val = (DWORD)(-(long)val);
		*var = val;
	}
	return retval;
}

//==============================================================
// returns:
// -1 = str found, longer than maxlen
//	0 = str not found
// >0 = len of found string
long iniGetStr(BYTE *var, WORD maxlen) {

	long	i, retval=0;
	char	c;

	i = 0;
	if( INIbuf!=NULL && pINIvar!=NULL ) {	// must have a buffer, must have done an iniFindVariable
		INIskipwhite(pINIvar);
		c = *pINIvar;
		if( c=='"' ) {
			++pINIvar;
			while( i+1<maxlen && (c = *pINIvar) && c!='"' && c>=' ' && (c!='\\' || *(pINIvar+1)) ) {
				if( c=='\\' ) c = *(++pINIvar);
				var[i++] = c;
				++pINIvar;
			}
			if( *pINIvar=='"' ) {
				++pINIvar;
				retval = i;	// set to length of found string
			} else retval = -1;	// set to string longer than buffer
		}
	}
	var[i] = 0;
	return retval;
}

//==============================================================
// If section already exists, same as iniFindSec().
// Else ADDS a new section named [str] and makes is active section.
// returns:
//	1 = success
//	0 = failure
long iniSetSec(BYTE *str) {

	long	len, eollen, seclen, retval=0, i;
	char	eolbuf[8], *pSec;

	len = strlen((char*)str);
	if( INIbuf!=NULL ) {
		if( !FindSec((char*)str) ) {	// if didn't find section, add it (pINIsec is pointing to the end of file
			eollen = GetSystemEOL((BYTE*)eolbuf);
			seclen = len+eollen+13;	// +2 for []'s, +1 for '=', +10 for 0xCHECKSUM
			if( INIopenHole(pINIsec, seclen) ) {
				pSec = pINIsec;
				*pSec++ = '[';
				for(i=0; i<10; i++) *pSec++ = 0;	// clear checksum area for LENGTH and FLAGS
				*pSec++ = '=';
				strcpy(pSec, (char*)str);
				pSec += len;
				*pSec++ = ']';
				for(i=0; i<eollen; i++) *pSec++ = eolbuf[i];
				*((DWORD*)(pINIsec+1)) = seclen;
				pINIvar = NULL;
				retval = 1;	// signal success
			}
		} else retval = 1;	// signal success
	}
	return retval;
}

//==============================================================
// If variable already exists, it's value is replaced by 'str'.
// Else ADDS a new var named 'var' and sets its value to 'str'.
// returns:
//	1 = success
//	0 = failure
long iniSetVar(BYTE *var, BYTE *str) {

	long	oldlen, namlen, newlen, varlen, eollen, retval=0;;
	BYTE	eolbuf[8];
	int		i;

	eollen = GetSystemEOL(eolbuf);
	namlen = strlen((char*)var);
	varlen = strlen((char*)str);
	newlen = namlen+1+varlen+ eollen;// +1 for '='

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		oldlen = FindVar((char*)var);
		if( oldlen>newlen ) INIcloseHole(pINIvar, oldlen-newlen);
		else if( newlen!=oldlen ) INIopenHole(pINIvar, newlen-oldlen);
		*((long*)(pINIsec+1)) += (newlen-oldlen);	// adjust SECTION LENGTH

		strcpy(pINIvar, (char*)var);	// copy name
		pINIvar += namlen;
		*pINIvar++ = '=';
		strcpy(pINIvar, (char*)str);
		pINIvar += varlen;
		for(i=0; i<eollen; i++) *pINIvar++ = eolbuf[i];
		retval = 1;	// signal success
	}
	return retval;
}

//==============================================================
// Deletes the CURRENT section and ALL its variables.
void iniDelSec() {

	long	len;

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		len = *((long*)(pINIsec+1));
		INIcloseHole(pINIsec, len);
		pINIsec = INIbuf;
		pINIvar = NULL;
	}
}

//==============================================================
// Deletes a variable from the CURRENT section.
// returns:
//	1 = success
//	0 = failure
long iniDelVar(BYTE *varname) {

	long	oldlen, retval=0;

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		oldlen = FindVar((char*)varname);
		if( oldlen ) {
			INIcloseHole(pINIvar, oldlen);
			*((long*)(pINIsec+1)) -= oldlen;
			retval = 1;	// signal success
		}
	}
	return retval;
}

//==============================================================
// Advances to the next section, stores its name in 'secnam'.
// returns:
// -1 = section found, name longer than maxlen
//	0 = section not found (End Of File)
// >0 = len of new (found) section name
long iniNextSec(BYTE *var, WORD maxlen) {

	long	i, retval=0;;
	char	*pSec;

	i = 0;
	if( INIbuf!=NULL && pINIsec!=NULL ) {
		pINIsec += *((long*)(pINIsec+1));
		if( *pINIsec ) {
			pSec = pINIsec+12;	// skip [0xCHECKSUM=
			while( i+1<maxlen && *pSec!=']' ) var[i++] = *pSec++;
			retval = (i+1<maxlen || *pSec==']')?i:-1;
		} else pINIsec = NULL;
		pINIvar = NULL;
	}
	var[i] = 0;
	return retval;
}

//==============================================================
// Deletes entire contents of ini buffer, leaves buffer allocated
void iniDelAll() {

	if( INIbuf!=NULL ) {
		INIused = 1;
		*INIbuf = 0;
		pINIsec = pINIvar = NULL;
	}
}

//==============================================================
// returns:
//	TRUE = var data at EOL (no more data)
//	FALSE = var still has more data before EOL
BOOL iniEndVar() {

	if( INIbuf==NULL || pINIvar==NULL ) return TRUE;
	INIskipwhite(pINIvar);
	return (*pINIvar==0 || INIeol(pINIvar));
}

//==============================================================
// returns:
//	1 = current section had a bad checksum when read from file
//	0 = current section checksum was good
BOOL iniBadSum() {

	if( INIbuf!=NULL && pINIsec!=NULL ) return (pINIsec[5] & 1);
	return FALSE;
}

//==============================================================
// If variable already exists, 'num' is formatted as text and appended to its value.
// Else ADDS a new var named 'var' and sets its value to 'num'.
// NOTE: 'num' will be formatted as:
//	UNSIGNED HEX (0xHHHHHHHH) if 'form'==2,
//	UNSIGNED OCTAL (0ooooo) if 'form'==1,
//	SIGNED DECIMAL if 'form'==anything else
// returns:
//	1 = success
//	0 = failure
long iniAddVarNum(BYTE *var, DWORD val, DWORD form) {

	long	oldlen, namlen, newlen, varlen, eollen, retval=0;
	BYTE	eolbuf[8], numbuff[16];
	int		i;

	if( form==2 ) sprintf((char*)numbuff, " 0x%8.8lX", (DWORD)val);
	else if( form==1 ) sprintf((char*)numbuff, " 0%lo", (DWORD)val);
	else sprintf((char*)numbuff, " %ld", val);

	eollen = GetSystemEOL(eolbuf);
	namlen = strlen((char*)var);
	varlen = strlen((char*)numbuff);
	newlen = namlen+1+varlen+ eollen;// +1 for '='

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		oldlen = FindVar((char*)var);
		if( oldlen ) {	// var existed before
			INIopenHole(pINIvar+oldlen, varlen);
			*((long*)(pINIsec+1)) += varlen;	// adjust SECTION LENGTH
			pINIvar += oldlen-eollen;	// setup to overwrite previous EOL and new hole with formatted string
		} else {	// else variable didn't exist before, create it
			INIopenHole(pINIvar, namlen);
			*((long*)(pINIsec+1)) += newlen;	// adjust SECTION LENGTH
			strcpy(pINIvar, (char*)var);	// copy name
			pINIvar += namlen;
			*pINIvar++ = '=';
		}
		for(i=0; i<varlen; i++) {	// copy out the formatted number string
			*pINIvar++ = numbuff[i];
		}

		for(i=0; i<eollen; i++) *pINIvar++ = eolbuf[i];	// output eol sequence
		pINIvar = NULL;	// NULL pointer since it's not in a good place to do further work
		retval = 1;	// signal success
	}
	return retval;
}

//==============================================================
// If variable already exists, 'str' is appended to its value.
// Else ADDS a new var named 'var' and sets its value to 'str'.
// NOTE: 'str' is an unquoted string.  It will get quotes added around
//	it, and any '"' or '\' chars within the string will get a '\' char
//	add in front of them.
// returns:
//	1 = success
//	0 = failure
long iniAddVarStr(BYTE *var, BYTE *str) {

	long	oldlen, namlen, newlen, varlen, eollen, formvlen, retval=0;
	BYTE	eolbuf[8];
	int		i;

	eollen = GetSystemEOL(eolbuf);
	namlen = strlen((char*)var);
	varlen = strlen((char*)str);
	newlen = namlen+1+varlen+ eollen;// +1 for '='

	if( INIbuf!=NULL && pINIsec!=NULL ) {
		// add 3 to target length, 1 for leading space, 2 for surrounding quotes.
		// then add one more for every '\' or '"' in the string, which will need to be escaped by a '\'
		for(i=0, formvlen=varlen+3; i<varlen; i++) if( str[i]=='\\' || str[i]=='"' ) ++formvlen;
		oldlen = FindVar((char*)var);
		if( oldlen ) {	// var existed before
			INIopenHole(pINIvar+oldlen, formvlen);
			*((long*)(pINIsec+1)) += formvlen;	// adjust SECTION LENGTH
			pINIvar += oldlen-eollen;	// setup to overwrite previous EOL and new hole with formatted string
		} else {	// else variable didn't exist before, create it
			INIopenHole(pINIvar, namlen + (formvlen-varlen));
			*((long*)(pINIsec+1)) += (newlen+(formvlen-varlen));	// adjust SECTION LENGTH
			strcpy(pINIvar, (char*)var);	// copy name
			pINIvar += namlen;
			*pINIvar++ = '=';
		}
		*pINIvar++ = ' ';	// output leading space
		*pINIvar++ = '"';	// output leading quote
		for(i=0; i<varlen; i++) {	// copy out the unquoted string, escaping any '\' or '"' chars with a '\' char.
			if( str[i]=='\\' || str[i]=='"' ) *pINIvar++ = '\\';
			*pINIvar++ = str[i];
		}
		*pINIvar++ = '"';

		for(i=0; i<eollen; i++) *pINIvar++ = eolbuf[i];	// output eol sequence
		pINIvar = NULL;	// NULL pointer since it's not in a good place to do further work
		retval = 1;	// signal success
	}
	return retval;
}


