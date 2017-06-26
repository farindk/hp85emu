
#include "tape.h"
#include "mach85.hh"
#include "main.hh"

#include <assert.h>
#include <algorithm>
#include <stdlib.h>
#include <string.h>


char	CurTape[64];

#define	TAP_GAP		0x8000
#define	TAP_SYNC	0x4000
#define	TAP_DATA	0x2000
#define	TAP_HOLE	0x1000




void TapeDrive::InitTape()
{
  IO_TAPSTS = 0240;	// note: OCTAL value
  IO_TAPCART = 0;
  IO_TAPCTL = 0;

  if (mTape) {
    InsertTape(mTape);
  }

  // LoadTape();
}


//*********************************************************************
void Tape::Save(std::string newFilename)
{
  // if no new filename was supplied, use old filename

  if (newFilename.empty()) {
    newFilename = mFilename;
  }
  else {
    mFilename = newFilename;
  }


  FILE* fh = fopen(mFilename.c_str(), "rb");
  if (fh) {
    TAPBUF[0][0] = TAPREV;	// update tape design revision#
    *((uint32_t*)&TAPBUF[0][2]) = TAPPOS;	// save tape "position"
    fwrite(TAPBUF, 1, sizeof(TAPBUF), fh); // write to disk any changes to the tape
    fclose(fh);
  }
}


//*********************************************************************
void TapeDrive::EjectTape()
{
  if (mTape) {
    mTape->Save();
    mTape = nullptr;
  }

  IO_TAPSTS &= ~1;
  IO_TAPCART = 0;

  if (mOnTapeStatusChanged) {
    mOnTapeStatusChanged(*this);
  }
}


#define	HOLE_LEN	2
//**************************************************************
void Tape::AddTapeHole(long p)
{
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
void Tape::New()
{
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

#if TODO
  IO_TAPSTS = 0250;  // tape is write enabled
  IO_TAPCART = 1;			// signify cartridge inserted, not yet CAT'd
  TAP_ADVANCE = FALSE;
#endif
}


//**************************************************************
bool Tape::Load(std::string filename)
{
  if( filename.empty() ) return FALSE;

  // read TAPE file
  std::string FilePath = "tapes/" + filename;
  //FILE* fh = getEnvironment()->openEmulatorFile(FilePath);
  FILE* fh = fopen(filename.c_str(), "rb");

  printf("loading %s\n",FilePath.c_str());

  if (fh==NULL) return FALSE;

  printf("loading...\n");

  fread((BYTE *)TAPBUF, 1, sizeof(TAPBUF), fh);
  fclose(fh);

  TAPPOS = std::max(528+2048, (int)*((DWORD *)&TAPBUF[0][2]));	// position tape to previous position (or right of single-hole in case of bug)
  // IO_TAPSTS BIT0==0 and IO_TAPCART BIT0==1 tells system that a cartridge is in, but hasn't yet been seen by the HP-85 tape code

  return TRUE;
}

#if TODO
// NOTE: TapBuf[0][1] is actually write ENABLE, not write PROTECT.
// So it's set to 0010 when ENABLED, 0000 when PROTECTED.
// Ditto for taphead[1] in Get/Set TapeWriteProtect() below

//*****************************************************************
// Returns TRUE if 'tapename' is WRITE-PROTECTED
BOOL GetTapeWriteProtect(char *tapename)
{
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
#endif

#if TODO
//*****************************************************************
// Changes the state of the WRITE-PROTECT tab on a tape cartridge
void SetTapeWriteProtect(char *tapename, BOOL wp)
{
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
#endif

#if TODO
#define	DEBUG_TAPSTS	0
//*****************************************************************
void DrawTapeStatus(PBMB hBM, long tx, long ty)
{
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
void DrawTape(PBMB hBM, long tx, long ty)
{
  int		y, h;

  if( Model>= 2 ) return;
  h = tapeBM->physH/4;
  y = (((IO_TAPSTS | IO_TAPCART) & 1)*2*h) + h*(IO_TAPCTL & 2)/2;
  BltBMB(hBM, tx, ty, tapeBM, 0, y, tapeBM->physW, h, TRUE);

  if( (IO_TAPSTS | IO_TAPCART) & 1 ) Label85BMB(hBM, tx, ty-10, 12, (BYTE*)CurTape, strlen(CurTape), MainWndTextClr);
  DrawTapeStatus(hBM, tx, ty);
}
#endif



void TapeDrive::InsertTape(std::shared_ptr<Tape> tape)
{
  if (mTape) {
    mTape->Save();
  }

  mTape = tape;

  IO_TAPSTS  = 0240 | mTape->getWriteEnableFlag();	// OR-in write-enable flag
  IO_TAPCART = 1;
  TAP_ADVANCE = FALSE;

  if (mOnTapeStatusChanged) {
    mOnTapeStatusChanged(*this);
  }
}


//**************************************************************
// get current byte of data from tape.
// if HOLE, set HOLE flag in TAPSTS
// if EVEN address, set TACH bit (tach every other location)
// if PWR-UP and MOTOR ON:
//		if not GAP, clear GAP flag, else set GAP flag
//		if tape data == 0, uninitialized area of tape, clear READY, GAP, HOLE, ILIM, and STALL

void TapeDrive::SetTapeStatus()
{
  WORD	td;
  td = mTape->read(IO_TAPCTL & 1);
  if( td & TAP_HOLE ) IO_TAPSTS |= 0020;
  if( !(mTape->getTAPPOS() & 1) ) IO_TAPSTS |= 0100;
  else IO_TAPSTS &= ~0100;
  if( (IO_TAPCTL & 0006)==0006 ) {
    if( !(td & TAP_GAP) ) IO_TAPSTS &= ~0040;
    else IO_TAPSTS |= 0040;
    if( td==0 ) IO_TAPSTS = (IO_TAPSTS & 0111);
  }
}

//*****************************************************************
// 177410 TAPSTS
void TapeDrive::writeTAPSTS(uint8_t val)
{
  int Model = mMachine->getModel();

  BYTE	i;

  if( Model<2 ) {
    if( (IO_TAPCTL & 0340)==0 ) {	// not writing previously
      if( (val & 0340)==0040 ) {	// starting a DATA write (no sync or gap)
					// if JUST starting write of DATA, make sure we're pointing at the
					// next block of data, and not a GAP.
        do {
          i = mTape->read(IO_TAPCTL & 1);
          if( i & (TAP_DATA | TAP_HOLE) ) break;
          mTape->advance();    // skip ahead to first DATA or HOLE byte
        } while( TRUE );
      }
    }
    if( (val & 0340)==0100 ) {	// writing a SYNC
      if( !(mTape->read(IO_TAPCTL & 1) & TAP_HOLE) ) {
        mTape->write(IO_TAPCTL & 1, IO_TAPDAT | TAP_SYNC);	// write the SYNC word and advance tape
        mTape->advance();
      }
      val &= ~0100;	// clear SYNC command
      val |= 0040;	// set writing DATA command (always after SYNC)
    }
    if( (IO_TAPCTL ^ val) & 036 ) {	// if changes in tape pwo/motor/speed
      IO_TAPCTL = val;
#if TODO
      DrawTape(KWnds[0].pBM, TAPEx, TAPEy);
      FlushScreen();
#endif

      if (mOnTapeStatusChanged) {
        mOnTapeStatusChanged(*this);
      }

      IO_TAPSTS |= 0200;	// set READY, always READY after power-up/down
    } else IO_TAPCTL = val;
  }
}


uint8_t TapeDrive::readTAPSTS()
{
  uint8_t retval=0;
  long	dir;
  uint8_t w, i;

  int Model = mMachine->getModel();

  if( Model<2 ) {
    IO_TAPSTS = (IO_TAPSTS & 0251) | 0040;	// clear tach,hole,ilim,stall, set gap, leave alone write-enabled and tape-in-drive
    if( (IO_TAPSTS | IO_TAPCART) & 1 ) {	// if tape in drive
      dir = (IO_TAPCTL & 0010) ? 1 : -1;	// tape direction
      if( TAP_ADVANCE && (IO_TAPCTL & 0006)==0006 ) {
        mTape->advance(dir);	// motor's running, they're reading TAPSTS, but not TAPDAT, so keep tape advancing
        TAP_ADVANCE = FALSE;
      }
      SetTapeStatus();
      if( (IO_TAPCTL & 0006)==0006 ) {		// if MOTOR-ON and PWR-UP
        if( (IO_TAPCTL & 0340)==0000 ) {	// if READING
          if( (mTape->read(IO_TAPCTL & 1) & TAP_SYNC)  ) {	// if SYNC byte, skip it
            mTape->advance(dir);
            SetTapeStatus();
          }
          w = mTape->read(IO_TAPCTL & 1);
          if( (IO_TAPCTL & 0026)==0026 && (w & TAP_DATA) ) {	// if searching over DATA
            for(i=0; i<8; i++) {
              w = mTape->read(IO_TAPCTL & 1);
              if( !(w & TAP_DATA) ) break;
              mTape->advance(dir);	// move faster when searching over DATA
            }
            SetTapeStatus();
            IO_TAPSTS |= 0100;	// definitely should have seen tach in there
          } else
            if( mTape->read(IO_TAPCTL & 1) & TAP_DATA ) {	// if on DATA
              IO_TAPSTS |= 0200;	// set READY
              TAP_ADVANCE = TRUE;	// flag we're on data/gap.  If they don't read TAPDAT, advance on the next read of TAPSTS.
            } else {
              mTape->advance(dir);	// if not DATA, advance ptr
              SetTapeStatus();
            }
        } else {	// else we're writing
          IO_TAPSTS |= 0200;	// always ready when writing
          w = IO_TAPCTL & 0340;
          if( w==0300 ) {			// writing GAP
            // if not on hole (NEVER overwrite hole), write GAP word
            if( !(mTape->read(IO_TAPCTL & 1) & TAP_HOLE) ) {
              mTape->write(IO_TAPCTL & 1, TAP_GAP);
            }
            mTape->advance(dir);
            SetTapeStatus();
          } else if( w==0100 ) {	// writing SYNC
            // should never happen if we understand and have structured things right...
#if TODO
            SystemMessage("Writing SYNC during read of TAPSTS");
#endif
          } else if( w==0040 ) {	// writing DATA
            // do nothing, let write to TAPDAT do the write and advance tape
          }
        }
      } else IO_TAPSTS |= 0200;	// always READY if motor off or power down
    } else if( !IO_TAPCART ) IO_TAPSTS |= 0200;	// always READY if no tape in drive

    retval = IO_TAPSTS;
    IO_TAPSTS &= ~0200;	// clear ready
    IO_TAPSTS |= IO_TAPCART;	// set cartridge in if was first check

#if TODO
    DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
#endif

    if (mOnTapeStatusChanged) {
      mOnTapeStatusChanged(*this);
    }
  }

  return retval;	// READ
}

//*****************************************************************
// 177411 TAPDAT
void TapeDrive::writeTAPDAT(uint8_t val)
{
  BYTE	i, w;

  if( mMachine->getModel()<2 ) {
    i = IO_TAPDAT = val;

    w = IO_TAPCTL & 0340;
    if( w==0300 ) {				// writing GAP
      // probably just setting up for SYNC write, do nothing
    } else if( w==0100 ) {		// writing SYNC
      // should never happen if we understand and have structured things right...
#if TODO
      SystemMessage("Writing to TAPDAT during writing of SYNC");
#endif
    } else if( w==0040 ) {		// writing data
      i |= TAP_DATA;
    }
    // if motor on and writing and not on a hole (never overwrite THOSE!), write the byte to tape
    if( (IO_TAPCTL & 0006)==0006 ) {	// if the motor is on
      if( w && !(mTape->read(IO_TAPCTL & 1) & TAP_HOLE) ) {
 	mTape->write(IO_TAPCTL & 1, i);	// write new value
      }
      mTape->advance();	// advance tape
    }

#if TODO
    DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
#endif

    if (mOnTapeStatusChanged) {
      mOnTapeStatusChanged(*this);
    }
  }
}



uint8_t TapeDrive::readTAPDAT()
{
  uint8_t retval=0;

  if( mMachine->getModel()<2 ) {
    // skipping GAPS/SYNCS, get next data byte (since we're supposed to be ready)
    if( (IO_TAPCTL & 0006)==0006 ) {	// PWR-UP, MOTOR ON
      while( !(mTape->read(IO_TAPCTL & 1) & (TAP_DATA | TAP_HOLE)) ) {
        mTape->advance( (IO_TAPCTL & 0010) ? 1 : -1 );
      }
      retval = IO_TAPDAT = (BYTE)(mTape->read(IO_TAPCTL & 1) & 0x00FF);
      mTape->advance();
      TAP_ADVANCE = FALSE;
    } else {	// MOTOR OFF, just echo what was written
      retval = IO_TAPDAT;
    }

#if TODO
    DrawTapeStatus(KWnds[0].pBM, TAPEx, TAPEy);
#endif

    if (mOnTapeStatusChanged) {
      mOnTapeStatusChanged(*this);
    }
  }

  return retval;	// READ
}


void TapeDrive::install(HPMachine& machine)
{
  mMachine = &machine;

  machine.assignIO(0177410,
                   std::bind(&TapeDrive::readTAPSTS,this),
                   std::bind(&TapeDrive::writeTAPSTS,this,std::placeholders::_1));
  machine.assignIO(0177411,
                   std::bind(&TapeDrive::readTAPDAT,this),
                   std::bind(&TapeDrive::writeTAPDAT,this,std::placeholders::_1));
}
