
#ifndef TAPE_H
#define TAPE_H

#include "config.h"
#include "mach85.hh"

#include <memory>


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


class HPMachine;



class Tape
{
 public:
  void New();

  bool Load(std::string filename);
  void Save(std::string newFilename = "");

  uint16_t read(int head) { return TAPBUF[head][TAPPOS]; }
  void     write(int head, uint16_t data) { TAPBUF[head][TAPPOS] = data; }

  void advance(int dir=1) { TAPPOS += dir; }

  int32_t getTAPPOS() const { return TAPPOS; }
  uint8_t getWriteEnableFlag() const { return TAPBUF[0][1]; }

 private:
  std::string mFilename;

  // 2 tracks, 128Kbytes / track (including GAPs, SYNCs, HEADERs, DATA, etc)
  uint16_t TAPBUF[2][TAPEK*1024];

  // current position of tap read/write head on the tape (ie, in TAPBUF)
  int32_t  TAPPOS;


  void AddTapeHole(long p);
};


class TapeDrive : public Peripheral
{
 public:
  TapeDrive() { InitTape(); }

  // Installs the tape drive into the machine (i.e. routes the IO addresses to this peripheral)
  void install(HPMachine&);

  void powerOn() { InitTape(); }

  void InitTape();
  bool LoadTape(); // TODO: remove me
  void InsertTape(std::shared_ptr<Tape> tape);
  void EjectTape();

  std::shared_ptr<Tape> getTape() { return mTape; }

  void setTapeStatusChangedCallback(std::function<void(TapeDrive&)> f) { mOnTapeStatusChanged=f; }

  bool isPowerOn() const { return IO_TAPCTL & 0004; }
  bool isCartridgeInserted() const { return (IO_TAPSTS | IO_TAPCART) & 1; }
  int  getActiveHead() const { return IO_TAPCTL & 1; }
  bool isMotorOn() const { return IO_TAPCTL & 0002; }
  bool isForward() const { return IO_TAPCTL & 0010; }
  bool isHighSpeed() const { return IO_TAPCTL & 0020; }
  bool isWriting() const { return IO_TAPCTL & 0340; }

 private:
  HPMachine* mMachine = nullptr; // the machine in which we are installed

  std::shared_ptr<Tape> mTape;

  /* .. ... ...
     || ||| ||+- 001 active head
     || ||| |+-- 002 is motor on
     || ||| +--- 004 is power on
     || ||+----- 010 forward direction
     || |+------ 020 high speed
     || +------- 040 \
     |+--------- 100  >- current write mode
     +---------- 200 /

     write mode:
     0300 -> writing gap
     0100 -> writing sync
     0040 -> writing data
  */
  uint8_t IO_TAPCTL;
  
  /* .. ... ...
     || ||| ||+- 001       cartridge inserted
     || ||| |+-- 002 HOLE  detected hole
     || ||| +--- 004
     || ||+----- 010       write enable
     || |+------ 020
     || +------- 040 GAP   on gap (only if power and motor on in TAPCTL) 
     |+--------- 100 TACH  position is even number
     +---------- 200 READY

     ILIM and STALL  :  004, 020  but I don't know which is which...

     Writing to TAPSTS actually writes to TAPCTL, or it writes a sync or gap.
  */
  uint8_t IO_TAPSTS;
  uint8_t IO_TAPCART;

  /*
    When writing to TAPDAT and we are not currently writing a GAP or are on a HOLE, write the value to tape.
    Reading from TAPDAT skips gaps and syncs on the tape and returns the next data (even when on a hole).
    TODO: not sure about the hole thing. Since we never write to a hole, the data at a hole is most likely always 0.
  */
  uint8_t IO_TAPDAT;	// byte written to TAPDAT, should be able to be read back from TAPDAT

  bool	TAP_ADVANCE;

  std::function<void(TapeDrive&)> mOnTapeStatusChanged;

  void SetTapeStatus();

  uint8_t readTAPDAT();
  void    writeTAPDAT(uint8_t val);
  uint8_t readTAPSTS();
  void    writeTAPSTS(uint8_t val);
};


#endif
