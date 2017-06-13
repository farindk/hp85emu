/*******************************************************
  BEEP.C - sound support for HP-85 emulator
  Copyright 2006 Everett Kaser
  All rights reserved
 See HP85EM.TXT for legal usage.

WARNING!!! This program is designed and compiled as a 32-bit program!!!
Pointers to objects are converted to DWORDs and vice-versa.  If you
port this to a 64-bit or greater architecture, changes will have to be
made!!!

 ********************************************************/
#include	"main.h"
#include	"support.h"
#include	"mach85.h"

#include	<mmsystem.h>
//#include <malloc.h>

#define	WAVEBUFLEN	22050*5		// enough room for 5 seconds of sound (for manually toggled speaker sounds)

//*******************************************************************
HWAVEOUT	hWave=NULL;
DWORD		wavePlaying=0;

HGLOBAL		hMem=NULL;
BYTE		*pMem=NULL;

HGLOBAL		hHeader=NULL;
LPWAVEHDR	pHeader=NULL;

MMTIME		wavePos;

DWORD		waveOffTime;

DWORD		LastOnCycles, LastOffCycles, DurOffCycles;
BOOL		SpeakerOff=TRUE;
BYTE		*pBuild=NULL;

//*******************************************************************
void CALLBACK waveOutCallBack(HWAVEOUT hW, UINT msg, DWORD instance, DWORD param1, DWORD param2) {

	switch( msg ) {
	 case WOM_CLOSE:
		break;
	 case WOM_DONE:
		wavePlaying = 0;
		break;
	 case WOM_OPEN:
		break;
	 default:
		break;
	}
}

//*******************************************************************
BOOL WaveOpen() {

	WAVEFORMATEX	waveformat;

	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	waveformat.nChannels = 1;
	waveformat.nSamplesPerSec = 22050;
	waveformat.nAvgBytesPerSec= 22050;
	waveformat.nBlockAlign = 1;
	waveformat.wBitsPerSample = 8;
	waveformat.cbSize = 0;

	if( MMSYSERR_NOERROR != waveOutOpen(&hWave, WAVE_MAPPER, &waveformat, (DWORD)&waveOutCallBack, 0, CALLBACK_FUNCTION) ) {
		hWave = NULL;
		return FALSE;
	}

	if( NULL==(hMem=GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, WAVEBUFLEN)) ) {
closewave:
		waveOutClose(hWave);
		hWave = NULL;
		return FALSE;
	}
	if( NULL==(pMem=GlobalLock(hMem)) ) {
freemem:
		GlobalFree(hMem);
		goto closewave;
	}
	memset(pMem, 128, WAVEBUFLEN);	// init wave buffer to silence
	if( NULL==(hHeader=GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(WAVEHDR))) ) {
unlockmem:
		GlobalUnlock(hMem);
		pMem = NULL;
		goto freemem;
	}
	if( NULL==(pHeader=(LPWAVEHDR)GlobalLock(hHeader)) ) {
		GlobalFree(hHeader);
		hHeader = NULL;
		goto unlockmem;
	}
/*
typedef struct {
    LPSTR  lpData;
    DWORD  dwBufferLength;
    DWORD  dwBytesRecorded;
    DWORD  dwUser;
    DWORD  dwFlags;
    DWORD  dwLoops;
    struct wavehdr_tag * lpNext;
    DWORD  reserved;
} WAVEHDR;
*/
	pHeader->lpData = (LPSTR)pMem;
	pHeader->dwBufferLength = WAVEBUFLEN;
	pHeader->dwFlags = 0;
	pHeader->dwLoops = 0;
	pHeader->lpNext = NULL;

	waveOutPrepareHeader(hWave, pHeader, sizeof(WAVEHDR));

	return TRUE;
}

//*******************************************************************
void WaveClose() {

	if( hWave != NULL ) {
		waveOutBreakLoop(hWave);
		waveOutReset(hWave);
		waveOutUnprepareHeader(hWave, pHeader, sizeof(WAVEHDR));
		waveOutClose(hWave);
		hWave = NULL;
	}

	if( hHeader != NULL ) {
		GlobalUnlock(hHeader);
		GlobalFree(hHeader);
		hHeader = NULL;
		pHeader = NULL;
	}
	if( hMem != NULL ) {
		GlobalUnlock(hMem);
		GlobalFree(hMem);
		hMem = NULL;
		pMem = NULL;
	}
}

//********************************************************************
void WaveStopBeep() {

	if( hWave && hMem && pMem && wavePlaying ) {
		waveOutBreakLoop(hWave);
		waveOutPause(hWave);
		waveOutReset(hWave);

		memset(pMem, 128, 10);
		pHeader->dwFlags &= ~(WHDR_BEGINLOOP | WHDR_ENDLOOP);
		pHeader->dwLoops = 0;
		pHeader->dwBufferLength = 10;
		wavePlaying = 2;
		waveOutWrite(hWave, pHeader, sizeof(WAVEHDR));

		waveOutPause(hWave);
		waveOutReset(hWave);

		wavePlaying = 0;
	}
}

//********************************************************************
void WaveStdBeep() {

	long	bph, c, ic, ib;
	BYTE	*pB;

	if( hWave && hMem && pMem ) {
		if( wavePlaying ) WaveStopBeep();

		bph = (22050/1200)/2;				// number of bytes in one 1/2-cycle
		c = 2205/(2*bph);					// number of integral full-cycles in 1/10th of 1 second buffer
		pHeader->dwBufferLength = c*bph*2;	// set length of buffer to integral # of cylces worth of bytes

		pB = pMem;
		for(ic=0; ic<c; ic++) {	// do a cycle
			for(ib=0; ib<bph; ib++) *pB++ = 128;// low for 1/2 the cycle
			for(ib=0; ib<bph; ib++) *pB++ = 255;// hi  for 1/2 the cycle
		}
		pHeader->dwFlags |= WHDR_BEGINLOOP | WHDR_ENDLOOP;
		pHeader->dwLoops = 0xFFFFFFFF;
		waveOutWrite(hWave, pHeader, sizeof(WAVEHDR));
		wavePlaying = 1;
	}
}

//********************************************************************
void WaveBeepOn() {

	DWORD	s, c;

	if( hWave && hMem && pMem ) {
		c = SecondsCycles*613062+Cycles;
		LastOnCycles = c;		// mark how many cycles into HP-85 CPU execution we are
		SpeakerOff = FALSE;
		if( pBuild==NULL ) {
			DurOffCycles = 10;	// default OFF duration in case only a single ON cycle in the beep
			pBuild = pMem;
		} else {	// been off, just came back on, write out "off cycle" bytes
			DurOffCycles = c-LastOffCycles;
			s = (22*DurOffCycles)/613;	// number of samples to write for OFF period
			if( (DWORD)(WAVEBUFLEN - (pBuild-pMem)) >= s ) {
				memset(pBuild, 128, s);
				pBuild += s;
			}
		}
	}
}

//********************************************************************
void WaveBeepOff() {

	DWORD	s, c;

	SpeakerOff = TRUE;
	if( hWave && hMem && pMem ) {
		if( pBuild!=NULL ) {
			c = SecondsCycles*613062+Cycles;
			LastOffCycles = c;		// mark how many cycles into HP-85 CPU execution we are
			s = (22*(c-LastOnCycles))/613;	// number of samples to write for ON period
			if( (DWORD)(WAVEBUFLEN - (pBuild-pMem)) >= s ) {
				memset(pBuild, 255, s);
				pBuild += s;
			}
		}
	}
}

//********************************************************************
// if they're doing a manual beep (toggling the speaker on/off bit in the keyboard controller)
// and the speaker is in the OFF position and it hasn't been toggled for 4*DurOffCycles, play the wave output
// and
void WaveOnTimer() {

	DWORD	s, c;

	c = SecondsCycles*613062+Cycles;
	if( hWave && hMem && pMem && pBuild!=NULL && SpeakerOff && (c-LastOffCycles)>2*DurOffCycles && wavePlaying==0 ) {
		WaveStopBeep();

		s = (22*DurOffCycles)/613;	// number of samples to write for OFF period
		if( (DWORD)(WAVEBUFLEN - (pBuild-pMem)) >= s ) {
			memset(pBuild, 128, s);
			pBuild += s;
		}

		pHeader->dwFlags &= ~(WHDR_BEGINLOOP | WHDR_ENDLOOP);
		pHeader->dwLoops = 0;
		pHeader->dwBufferLength = pBuild-pMem;
		waveOutWrite(hWave, pHeader, sizeof(WAVEHDR));
		wavePlaying = 2;
		pBuild = NULL;
	}
}

