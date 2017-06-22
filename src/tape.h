
#ifndef TAPE_H
#define TAPE_H

#include "config.h"
#include "mach85.hh"

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

BOOL LoadTape();

extern char		CurTape[64];



class HPMachine;



class Tape
{
 public:
  void Load(std::string filename);
  void Save(std::string newFilename = "");

  uint16_t operator[](int head);

 private:
  std::string mFilename;
};


class TapeDrive : public Peripheral
{
 public:
  TapeDrive() { }

  // Installs the tape drive into the machine (i.e. routes the IO addresses to this peripheral)
  void install(HPMachine&);

  void powerOn() { InitTape(); }

  void InitTape();
  bool LoadTape(); // TODO: remove me
  // TODO void InsertTape();
  void EjectTape();

  void setTapeStatusChangedCallback(std::function<void(TapeDrive&)> f) { mOnTapeStatusChanged=f; }

 private:
  HPMachine* mMachine = nullptr; // the machine in which we are installed

  uint8_t IO_TAPCTL;
  uint8_t IO_TAPSTS;
  uint8_t IO_TAPCART;
  uint8_t IO_TAPDAT;	// byte written to TAPDAT, should be able to be read back from TAPDAT

  std::function<void(TapeDrive&)> mOnTapeStatusChanged;

  void SetTapeStatus();

  uint8_t readTAPDAT();
  void    writeTAPDAT(uint8_t val);
  uint8_t readTAPSTS();
  void    writeTAPSTS(uint8_t val);
};


#endif
