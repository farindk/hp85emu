==========================================================================
This HELP is from the file "docs\readme.txt".  NO keys are active in the
Help window, it must be navigated using the mouse.  All keys are sent to
the main emulation window.
==========================================================================
Contents:
  QUICK REFERENCE
  CONFIGURING THE EMULATOR
  LEGAL STUFF
  INTRODUCTION
  HOW TO USE THE HP-85 EMULATOR
  MISCELLANEOUS USEFUL INFORMATION
  WHAT WORKS
  WHAT DOESN'T WORK YET
  KNOWN BUGS
  DESIGN PROBLEMS  
  RESOURCES
  HISTORY
  REVISIONS
  CONTACT INFORMATION

==========================================================================
QUICK REFERENCE
==========================================================================
While the Series 80 emulation is RUNNING, you can:

  CTL-F8 breaks into debug mode at any time
  You can left-click and SHIFT-left-click on the keys on the screen,
  Left-clicking on the tape will load and manage tape cartridges.
  Left-clicking on the disk drives will load and manage diskettes.
  
  PC KEY      SERIES 80 KEY
  ---------   -------------
  ESC         KEY LABEL        
  F1-F7       K1-K14           
  F8          RUN / CONT       
  F9          LIST / PLIST     
  F10         GRAPH or A/G     
  F11         -unassigned-     
  Break       PAUSE / STEP               
  Insert      INS-RPL                    
  Delete      -CHAR / -LINE              
  Home        home-arrow                 
  End         -unassigned-               
  Page Up     ROLL ^                     
  Page Down   ROLL v                     
  CTL-Up      views older internal print-out
  CTL-Down    views more recent internal print-out 
  Mouse-wheel scrolls (up/down) the internal print-out and/or CRT
  CTL-LEFT-click on disk drive auto-types and executes the
              "MASS STORAGE IS..." command for that drive.
  double-clicking on a PROG or BPGM file in an auto-CAT listing
              auto-types and executes the appropriate LOAD or
              LOADBIN command to load that file.

While in DEBUG mode you can:

  CTL-F2  - set or clear a MARK on the current disassembly line
  SHIFT-CTL-F2  - clear all MARKs in the disassembly
  F2      - move the current disassembly line to the next MARKed line
  F8      - RUN the Series 80 emulation
  F9      - single-step a single CPU instruction (may just be an ARP/DRP)
  F10     - toggle a breakpoint on the current cursor line
  CTL-F10 - clears *ALL* breakpoints
  F11     - single-step *OVER* the current line (including ARP/DRP/JSBs)
  CTL-F12 - resets the 'HP-85' to POWER-ON condition (restart)
  RIGHT-click on a line to edit it (or press 'E' to edit current line)
  J       - JUMPs disassembly to target address on the current code line
  BACKSPACE - JUMPs disassembly back to the previous JUMPed-from address
  . (period)- dumps the currently disassembled 8K of ROM to .LST commands

==========================================================================
CONFIGURING THE EMULATOR
==========================================================================
There are two entries in the menu:

1) SERIES 80 OPTIONS - this contains everything regarding deciding what
    your Series 80 emulated system contains, including the Model (85A,
    85B, or 87), which option ROMs are 'installed', how much RAM is
    installed, and which devices (via the DEVICES pushbutton and sub-
    dialog) are attached.  Changes in the DEVICES sub-dialog are not
    actually saved/actuated until the DEVICES dialog is closed AND the
    SERIES 80 OPTIONS dialog is OK'd.  So, if you make changes in the
    DEVICES dialog, click CLOSE, and then click CANCEL in the SERIES 80
    OPTIONS dialog, the changes you made in the DEVICES dialog will be
    discarded.
    
2) EMULATOR OPTIONS - this contains everything that is not Series 80
    related, but rather options that control the LOOK of the emulator and
    how the emulator runs.

-------
DEVICES
-------
In the DEVICES dialog, accessed via the SERIES 80 OPTIONS dialog, you can
add or remove devices.  All devices are shared in common by the different
Series 80 models (they're not model-specific, they're system-wide).  All
of the devices work on all of the models EXCEPT the 5MB hard disk, which
does NOT work on the HP-85A model.

The devices dialog allows you to specify the Select Code (3-10) and
Controller Address (0-30) for each device.  You can install up to 32
devices (you can have as many 'instances' of any given device type as
you want, as long as they're at unique SC/CA combinations). The default
HPIB "talk/listen address" for the Series 80 computers is 21, so that
address should not be used as the Controller Address for any devices.
Use the '+' buttons at the bottom of the dialog to ADD new devices.
Use the DELETE buttons by each "Current device" to delete (remove) a
device from the system.

All of the disk drives create their 'disks' in the DISKS0-DISKS9
folders.  The 5.25" and 3.5" drives use the same size diskettes, so
those 'diskettes' are interchangeable between those two types of
drives (functionally, there's no difference between the two types
of drives, in the emulator, they just have different graphics for
the drive in the drive window).  The 5.25"/3.5" diskette files are
270,336 bytes long.  The 8" drive uses a file 1,152,000 bytes long,
and the 5MB hard disk uses a file 4,856,832 bytes long (so no, it
wasn't really a 5MB hard disk).

However, one potential 'gotcha' to be aware of: the HP-85B ROM
code thought the 9135A (5MB) hard disk had 152 cylinders of a 124 sectors
per cylinder, whereas the HP-86/87 ROM thought it had 153 cylinders.
The emulator creates a file large enough for the slightly larger size.
So if you were to COMPLETELY fill one of these 'disks' while in HP-87
mode, and then switched to HP-85B mode, the files at the very end of
the disk would probably not be accessible.  YOU'VE BEEN WARNED. :-)
Unlike most real hard disks, the 'disk' in the 5MB drives in the
emulator can be selected just like the diskettes in the disk drives,
so you can have multiple alternate hard disk sets of files.

NOTE: the disk-selection dialog will only show you disks of the proper
size for the drive you've 'opened,' so you can't load a 5MB 'disk'
into a 5.25" drive, nor vice-versa.

TECHNICAL NOTE (as if this whole file isn't...): Some LIF utilities
generate identical files to what this emulator uses, but they don't
ignore the "4 spare tracks" that most all HP computers spared on
disks, so their lengths are longer. As a result, the emulator also
recognizes 286,720 byte files as being 5.25" and 3.5" diskettes,
and recognizes 1,182,720 byte files as being 8" diskettes.  The
emulator (or really the Series 80 computer) won't USE the extra
space in those files, but it will recognize the file as being a
file of the appropriate size and load it. HOWEVER (there always is
one...) the emulator will NOT write out the original size of the
disk, but rather only the shorter size, trimming the "spared tracks"
off the end of the file.  So make SURE you have backups of any
such files that you try to import from elsewhere, just in case
something goes wrong ("it could happen...").

Each emulated disk drive gets it's own window.  Those windows can be
moved around and resized to fit your whimsy.  Each drive unit has an
"auto-CAT" feature that will show the current contents of that disk
drive unit.  The diskette drives will (by default) have two of these
"auto-CAT" displays, while the 5MB hard disk window will only show
one (since there's only one drive, natch).  With the diskette windows,
you may be odd, misshapen and misunderstood, and you may only want
an auto-CAT of ONE of the drive units.  There is a checkbox by the
disk title above the auto-CAT boxes that controls whether the auto-CAT
feature is on or off for that drive unit.  Also, the disk title above
each auto-CAT will be red if the disk is write-protected, green if not.
At the bottom of each auto-CAT list is a final line that contains
the number of sectors USED on the disk and the total number of sectors
on the disk, along with the percentage-used (for the math-challenged
out there).  The volume sectors and directory sectors are included
in the count of sectors used, but "purged files" are NOT included
in the used count, since they're still available for use.

The "print to file" device will print to a file called "PRTxxx.TXT"
where 'xxx' is the HPIB address of the print-to-file device.  So,
if you put a print-to-file device at select code 527, for example,
and execute a PRINTER IS 527 command, then all PRINT statements
will be written to the PRT527.TXT ASCII file.

The devices in the DEVICES dialog are kept sorted by SC and then CA.  You
might want to have them be sorted by device type.  You might not want to
want that, because you'd be disappointed (at least, for now).  It can be
a little weird, because the devices can jump around when you edit the
select code or controller address and tab away from the edit field, but
the cursor moves appropriately, so hopefully you won't get lost.

==========================================================================
LEGAL STUFF
==========================================================================
1) The ROM files and application pacs are the exact bytes from the
   actual Series 80 ROMs, disks and tapes.  These, naturally, are
   the intellectual property of Hewlett-Packard Company.

2) All of the other files (the "emulator files") are of my creation, 
   and they are:
     Copyright 2006-17 Everett Kaser.
     All rights reserved.

   However...
   
   You MAY use and redistribute the emulator files however you like,
   under these conditions:
    a) You don't sell them for any amount without first getting my
       written permission.
    b) You don't distribute modified versions of the package or any of
       its files, unless you also get an acknowledged copy of your
       modified version to me AND release your changes under these
       same stipulations (ie, not for sale, copyright retained, etc).
       I intend to remain the "keeper of the sources" for this emulator,
       and really don't want to see multiple "flavors" of it floating 
       about without good reason.  But I'm a reasonable fellow... :-)
       Let me know if there's something you'd like to do.  See 
       "CONTACT INFORMATION" at the end of this document.
   Other than that, knock yourself out and have some fun!

3) On January 1, 2030, I release all of my copyrights on this package 
   to the public domain.  On that date, the HP-85 will be 50 years old,
   and I'll be lucky to be still alive, so who cares?

==========================================================================
INTRODUCTION
==========================================================================
The reason for this emulator is that I'm exceedingly fond of the old
HP-85/87 machines, the actual machines are rapidly disappearing from the
face of the Earth, and I'm hoping that this emulator will help preserve
its memory a while longer.  (Plus, I enjoy these kinds of things. :-)

The priorities, when designing and writing the HP-85 emulator, were:
1) To perfectly emulate as much Series 80 hardware as possible.
2) To mainain the original "look and feel" as much as possible.
3) To make the emulator source code as portable as possible.

Regarding 2), the Series 80 systems were developed in assembly language,
and the assembler used OCTAL for its number representation, not HEX.
I remain faithful to this in most aspects of this emulator, although the
emulator code does support displaying of numbers also in decimal and/or
hexidecimal.  However, it's not been tested and debugged using decimal 
and hex displays very much, so there may be issues using those bases.

Regarding 3), the emulator is written in C, and uses as few OS routines
as possible. This meant no OS-provided menus, dialogs, etc.  The SUPPORT.C
file contains all of the graphics utility routines that draw into memory
bitmap buffers (3 RGB bytes per pixel).  The only drawing to the actual
screen hardware (via the OS) is done via a single call in MAIN.C
(in the WM_PAINT message) to copy the memory bitmap to the screen memory.
Most of these routines were cribbed from my KINT interpreter code, and as
such, certainly have some things that are not particularly pertinent to
the Series 80 emulator.  But, they work, they were already debugged, so...
NOTE: As of version 4.0, the above has been stretched to add the ability
to create additional windows (KCreateWnd), each with their own PROC in
the MAIN.C file.  This allows things like the disk drive being in its
own window with automatic CAT (directory listing), as well as the HELP
file being opened all the time (in case you want to have the key-map
reference showing or something else), and opens the possibility of
some day adding other 'peripherals' such as a plotter, etc.

(More information regarding my KINT interpreter, including source files,
is available at: http://www.kaser.com/kint.html.)

DLG.C contains dialog and dialog control code.  Again, none of this code
uses ANY OS calls, but rather draws the dialogs and controls itself, using
the SUPPORT.C bitmap routines and the Series 80 font.
   
Regarding fonts, yes, they're pretty ugly.  But again, to remain faithful
to the Series 80 appearance (fonts) AND to remain as portable as possible,
I chose to use the Series 80 font EVERYWHERE.  You could, of course,
implement your own set of dialogs to replace the code in DLG.C, and make
things look much better (although much less portable and transparent).
Again, NOTE the above NOTE regarding the 4.0 addition of OS-dependent
windows and window procedures... opening Pandora's Box on portability.

==========================================================================
HOW TO USE THE Series 80 EMULATOR
==========================================================================
The program needs these files in order to function:
  HP85.EXE       the emulator program
  ROMS85\ROMSYS1 HP-85A system ROM bytes from 0-8K
  ROMS85\ROMSYS2 HP-85A system ROM bytes from 8K-16K
  ROMS85\ROMSYS3 HP-85A system ROM bytes from 16K-24K
  ROMS85\ROM000  HP-85A system ROM bytes from 24K-32K (option ROM 0)
  ROMS85\romsys1B HP-85B system ROM bytes from 0-8K
  ROMS85\ROMSYS2B HP-85B system ROM bytes from 8K-16K
  ROMS85\ROMSYS3B HP-85B system ROM bytes from 16K-24K
  ROMS85\ROM000B HP-85B system ROM bytes from 24K-32K (option ROM 0)
  ROMS85\ROM010  HP-85 Program Development ROM bytes (option ROM 10)
  ROMS85\ROM050  HP-85 Assembler ROM bytes (option ROM 50)
  ROMS85\ROM250  HP-85 Forth ROM (unofficial)
  ROMS85\ROM260  HP-85 Matrix ROM bytes (option ROM 260)
  ROMS85\ROM300  HP-85A I/O ROM bytes (option ROM 300)
  ROMS85\ROM300B HP-85B I/O ROM bytes (option ROM 300)
  ROMS85\ROM317  HP-85 Extended Mass Storage bytes (option ROM 317)
  ROMS85\ROM320  HP-85A Mass Storage ROM bytes (option ROM 320)
  ROMS85\ROM320B HP-85B Mass Storage ROM bytes (option ROM 320)
  ROMS85\ROM321  HP-85B Electronic Disk ROM bytes (option ROM 321)
  ROMS85\ROM340  HP-85 Service ROM bytes (option ROM 340)
  ROMS85\ROM350  HP-85 Advanced Programming ROM bytes (option ROM 350)
  ROMS85\ROM360  HP-85 Plotter/Printer ROM bytes (option ROM 360)
  ROMS85\RAM_IO.DIS disassembly records for RAM and I/O addresses
  ROMS87\ROMSYS1 HP-87 system ROM bytes from 0-8K
  ROMS87\ROMSYS2 HP-87 system ROM bytes from 8K-16K
  ROMS87\ROMSYS3 HP-87 system ROM bytes from 16K-24K
  ROMS87\ROM000  HP-87 system ROM bytes from 24K-32K (option ROM 0)
  ROMS87\ROM001  HP-87 Graphics ROM bytes (option ROM 1, built-in)
  ROMS87\ROM016  HP-87 MIKSAM ROM bytes (option ROM 016)
  ROMS87\ROM030  HP-87 Language ROM bytes (option ROM 30)
  ROMS87\ROM047  HP-87 Everett Kaser's Extension ROM (unofficial)
  ROMS87\ROM050  HP-87 Assembler ROM bytes (option ROM 50)
  ROMS87\ROM070  HP-87 Andre Koppel's System Extension ROM (unofficial)
  ROMS87\ROM250  HP-87 Forth ROM (unofficial)
  ROMS87\ROM260  HP-87 Matrix ROM part 1
  ROMS87\ROM261  HP-87 Matrix ROM part 2
  ROMS87\ROM300  HP-87 I/O ROM bytes (option ROM 300)
  ROMS87\ROM317  HP-87 Extended Mass Storage bytes (option ROM 317)
  ROMS87\ROM320  HP-87 Mass Storage ROM bytes (option ROM 320)
  ROMS87\ROM321  HP-87 Electronic Disk ROM bytes (option ROM 321)
  ROMS87\ROM347  HP-87 Advanced Programming ROM part 2
  ROMS87\ROM350  HP-87 Advanced Programming ROM part 1
  ROMS87\ROM360  HP-87 Plotter ROM bytes (option ROM 360)
  ROMS87\RAM_IO.DIS disassembly records for RAM and I/O addresses
  IMAGES\85KEYS*.BMP  small and big images of the HP-85 keys
  IMAGES\87KEYS*.BMP  small and big images of the HP-87 keys
  IMAGES\TAPE*.BMP    small and big images of the HP-85 tape drive
  IMAGES\DISKS*.BMP   small and big images of the disk drives
  IMAGES\RUNLITE*.BMP small and big images of HP logo and power/run light
  TAPES\        sub-directory.  This is where "tapes" are stored.
  DISKS1\       contains some HP-85 software disks
  DISKS2\       contains some HP-85/87 miscellaneous disks
  DISKS3\ - DISKS9\ other folders in which to store your disks
  DOCS\README.TXT     this file

Files that get created by the HP-85 emulator:
  SERIES80.CFG  This was the 2.0 and 3.0 emulator's configuration file,
                holding configuration info.  It is no longer used with
                version 4.0, being replace by SERIES80.INI.
  SERIES80.INI  An ASCII configuration file containing the settings for
                the emulator.  It IS manually editable, although not
                really designed or intended to be manually edited.
  ROM85.SRC     created when you do an EXPORT of the ROM disassembly,
                this is an ASCII file containing the disassembly of the
                entire 64Kbyte address space WITH WHICHEVER OPTION ROM
                IS CURRENTLY MAPPED IN!
  *.LST         While broken into debugger, the '.' key will export
                whichever 8K block of ROM is currently disassembled
                (it uses the top-most line to determine the address)
                to a .LST file with that ROM's name.  The .LST file
                will be a list of DOT-COMMANDS suitable for use with
                the HP80DIS program for further disassembling the ROM.
                See the HP80DIS program documentation for further info.
  *.DIS         Stored in the ROMS85 and ROMS87 folders, these files
                contain all the information you enter via the debug
                disassembly window.  Similar to what the HP80DIS
                program does with the .LST files, these .DIS files
                are binary files (as opposed to the .LST files which
                are ASCII text), and are not quite as capable as the
                various DOT-COMMANDS available to the HP80DIS program.
                The two SHOULD be brought more into congruence so that
                they're equally cabable and the information is more
                easily exchanged, but that's beyond the scope of the
                time I have to invest right now.

The program works much better with these additional files:
  ROMSYS00.DIS  disassembly comments for system ROM bytes from 0-8K
  ROMSYS08.DIS  disassembly comments for system ROM bytes from 8K-16K
  ROMSYS16.DIS  disassembly comments for system ROM bytes from 16K-24K
  ROM0.DIS      disassembly comments for system ROM bytes from 24K-32K
  ROM10.DIS     disassembly comments for Program Development ROM
  ROM50.DIS     disassembly comments for Assembler ROM
  ROM260.DIS    disassembly comments for Matrix ROM
  ROM300.DIS    disassembly comments for I/O ROM bytes
  ROM317.DIS    disassembly comments for Extended Mass Storage ROM
  ROM320.DIS    disassembly comments for Mass Storage ROM
  ROM340.DIS    disassembly comments for Service ROM
  ROM350.DIS    disassembly comments for Advanced Programming ROM
  ROM360.DIS    disassembly comments for Plotter/Printer ROM
                
Source files that are included with the distribution that allow you to
modify and build your own version of the HP-85 emulator are:
  SRC\MAIN.C        Operating System (OS) dependent code
  SRC\MAIN.H
  SRC\SUPPORT.C     Mostly display graphics library
  SRC\SUPPORT.H
  SRC\MACH85.C      The actual HP-85 emulation code
  SRC\MACH85.H
  SRC\ioINTERNAL.C  IO routines for "internal devices" (CRT, KYBD, etc)
  SRC\cardHPIB.C    IO routines for HPIB card and (tightly tied to the dev*.c files)
  SRC\ioDEV.h		IO routines header file
  SRC\devDISK.c		IO routines for all the disk drives
  SRC\devFilePRT.c	IO routines for the print-to-file device
  SRC\devHardPRT.c	IO routines for the print-to-printer device
  SRC\DLG.C         The dialogs
  SRC\DLG.H
  SRC\BEEP.C        The Series 80 'beeper' via digital output
  SRC\BEEP.H
  SRC\RESOURCE.H	Windows ID_ definitions
  SRC\HP85.RC		Windows ICON definition
  SRC\ICON.ICO      MS-Windows HP-85 icon file
  DOCS\REF.TXT      miscellaneous technical references about Series 80

You should maintain the structure of sub-folders that's in the ZIP file,
as that's where the HP85.EXE program expects to find them.

A debugger is integrated with the emulator, which allows you to
disassemble the ROM code, single-step through it, set breakpoints, etc.
If the Emulator Option "Start in debug mode" is checked, then when the
program is first started, it will automatically come up in the debugger
with the HP-85 halted.  This is the same as the moment the power switch
is first turned on for a real HP-85.  If no Series80.CFG or Series80.ini
file exists, the program will be configured with its default conditions.
Any changes you make to the default settings will be saved in Series80.ini
when you terminate the program and reloaded the next time it's run.

Once you've unpacked and installed the files (which you've probably
already done, since you're reading this...), run HP85.EXE.  At that point
you can configure your Series 80 using the menu (each menu item has a
shortcut key listed in front of it which may be used in place of clicking
on the menu).  You can select which ROMs you want loaded and whether the
machine has 16K or 32K bytes of RAM, and whether the display is single- or
double-sized (you will probably want to stick with single-size unless you
have a display that has 1280 or more pixels in the horizontal direction,
and with the HP-87 emulation you'll need a REALLY, REALLY hi-res monitor
to be able to use the double-size feature).

The disassembled ROM code has been PARTIALLY commented and structured.
You can further comment and "structure" the code by adding labels and
comments, and by specifying the "type" and "size" for each line.  Edit
the data for any line of the disassembly by RIGHT-clicking on the line,
or by pressing the 'E' key to edit the data for the CURRENT line.
The "type" tells the disassembler how to treat each "line" of code:
  COD  a regular CPU instruction, possibly preceded by an ARP and/or DRP.
  BYT  a data byte
  ASC  an ASCII string, set the SIZE field to the length of the string.
  ASP  an ASCII string whose last character has the MS (Parity) bit set.
  BSZ  a block of bytes set to 0, set SIZE to the number of bytes.
  DEF  a two-byte address on a line by itself.
  VAL  specify an 8-byte BCD floating point number (not implemented).
  RAM  for locations defined in RAM.  Set SIZE to # of bytes at location.
  I/O  for I/O locations from 177400 to 177777.
  DRP/ARP for stand-alone DRP or ARP instruction with no following COD.
For each line you edit in this way, a record is added to the ROMx.DIS file
for that 8 Kbyte ROM.  You can delete a record for a line by editing the
line, then clicking on the "Delete" button.  There is a checkbox at the
bottom of the edit dialog which will cause the emulator to automatically
advance to the next line and restart the dialog after you click OK.

The "Address" field in the dialog is what determines which line of the
disassembly the "edit" is attached to.  You can click on ANY line, edit
the address to a different value, and the values you enter into the dialog
will be attached to that "different" address, not the one you clicked on.

When lines of code have unknown multi-byte data registers and immediate
arguments (such as LDM R#,=123,456,...), the disassembler can't determine
how many bytes to use following the opcode as immediate arguments.  You
can set an override by setting the SIZE field for that line to the number
of bytes of data that follow the opcode.

==========================================================================
MISCELLANEOUS USEFUL INFORMATION
==========================================================================
This documentation file having grown and evolved, rather than being
planned and designed, I couldn't find a good place to put this information,
so here's where it goes...

For IMPORTING LIF files: You can import individual LIF files that were
saved using LIFUTIL with its DFS method (so that a "logical volume
label and director sector" were prepended) using the IMPORT LIF FILE...
menu entry.  If you have a folder of LIF files that were saved using
LIFUTIL with the DFS method, you can import ALL of them in one swell
foop also by holding down SHIFT while clicking on the IMPORT LIF FILES
TO :D700 menu.  It will ask for the folder containing the files, and then
bring them all into the current :D700 disk.  The 'gotcha' is that the
filenames may not be the right filenames, and since it's a 'batch'
approach, you don't get the chance to change the names one-by-one like
when you import the files one-by-one.

==========================================================================
WHAT WORKS
==========================================================================
1) The CPU.
2) The CPU RAM (16 or 32 Kbytes) and EXTENDED RAM (up to 1024 Kbytes).
3) The system ROMs.
4) The plug-in option ROMs.
5) The keyboard.
6) The CRT alpha and graphics.
7) Printer output (to a CRT emulated thermal paper strip).
8) The tape cartridges.
9) The TIMERs (CLKSTS/CLKDAT) in the keyboard controller.
10) The BEEPER (with "glitches" at times)
11) The speed of real Series 80 hardware is (optionally) maintained.
12) The Service ROM's "short" tests.
13) Printer output to a file (default: PRINTER IS 710)
14) Printer output to a REAL printer (default: PRINTER IS 720)
15) 5.25", 3.5", 8" diskette drives and a 5MB hard disk.
16) LIF file import to and export from the :D700 diskette.

You can type in most BASIC programs and they'll actually run.
However... if you use any functionality that tries to access
hardware that's not yet implemented, things come to a screeching
halt, resulting in a hang or a break into the emulator's debug mode.

==========================================================================
WHAT DOESN'T WORK YET
==========================================================================
1) I/O cards (except for HP-IB for printers and disk drives)
2) A memory dump feature for the debugger
3) The Service ROMs "long" tests

==========================================================================
KNOWN BUGS
==========================================================================
1) The Program Development ROM, when loaded, can cause erratic behavior.
   The Program Development ROM was actually a ROM written for the rack-
   mounted HP-85 (the HP 9915, an HP-85 in an instrument box), which
   was created by a Colorado HP division, and I don't believe it was 
   ever intended for use in a "normal" HP-85.  A lot of it is now
   working, but there may still be some outstanding issues.
2) EXTENDED MASS STORAGE and I/O ROMs are mostly not useful, since 
   they access unsupported hardware.  However, SOME features of those
   ROMs will work, as long as they only access implemented hardware.
3) The Service ROM passes the initial "power-up" quick-tests, but if
   you press keys to run the extended test, it still fails many of
   the sub-systems (CRT, Printer, Tape, Timers, Keyboard, etc). I've
   not yet looked into why, but many of these tests are VERY dependent
   upon the original hardware and timing issues which are almost
   impossible to duplicate perfectly in software.
4) If you try to access a tape that hasn't been ERASETAPE'd yet,
   it searches the entire tape, then rewinds in FRONT of the end-of-
   tape holes, leaving it stuck there.  This probably has to do with
   the handling of "gap detection" on un-initilized tape media.
5) Alpha-keys emulation doesn't currently allow for the FLIP statement
   to work.
6) All of the dialogs with text-edit controls need to have text-limits
   (length limits) set on those controls.
7) Toggling the "Run at natural (slow) speed" setting in the
   Emulator Options dialog can cause the emulator to appear to hang.
   If you want to change this setting, I recommend changing it, then
   immediately exiting the emulator, then restart it.
8) While the emulator works fine when "Run at natural (slow) speed"
   is OFF, for most things, there WILL be some problems encountered.
   For example, most BEEP statements will not be audible, because
   "time is sped up" for the Series 80 machine, so the BEEP gets
   turned off almost as soon as it's started.  There are probably
   other situations where running fast will cause problems.  If you
   encounter something funny, try turning ON the "Run at natural
   (slow) speed" option in the EMULATOR OPTIONS dialog.
9) Similar to 5) above, the keys emulation is a little flakey, as
   it depends upon the host/native computer's key repeat feature
   to do repeating keys.  So in some situations (such as in the
   RACE game included in the ACTION GAMES pack), key repeat
   emulation won't be as fast or accurate as needed.

==========================================================================
DESIGN PROBLEMS
==========================================================================
1) Since the option ROMs are mapped into the same address space,
   the way the disassembler works and the way the ROM mapping code
   works, if you look at "system ROM code" (from 0-24K) while an
   option ROM (other than ROM 0) is mapped in, that option ROM's
   DISASMREC structures will also be the ones that are mapped in
   for the 24K-32K (060000-077777) address space, and so IT'S labels
   are the ones that will be found instead of ROM 0's.  Complex to
   say in one sentence. :-)  Anyway, you will find that "labels are
   missing" or "wrong labels are displayed" when disassembling code
   in the 0-24K ROM space while an option ROM other than ROM 0 is
   mapped in.  This can most easily be fixed by checking the SOURCE
   address being disassembled, and if in the 0-057777 range, always
   use the ROM0 DISASMREC structures for referred to addresses.
2) The current emulator only supports one possible label for any
   given address.  The HP-85 software designers were in the habit
   of "reusing" the same RAM location and giving it different label
   names in different places.  This can result in some strange label
   name usage in some places.  It would be better to be able to assign
   multiple labels to the same address, with one being the "primary"
   label, and let the DISASMREC structures override which target label
   is to be used on any given line of code.
3) Currently, breakpoints are only set at execution addresses, and
   the addresses that are in option ROM space (060000-077777) will
   hit no matter which ROM is mapped in.  The breakpoint should include
   a ROM# option for that address range, and it would also be REALLY
   nice to have breakpoints that could be set on read and/or write of
   a data address or range of addresses.
4) The printer output bitmap (prtBM) is 2048 pixels tall.  When output
   reaches the bottom of the bitmap, the whole bitmap is scrolled upward
   via a BltBMB() call, a blank "line" added to the bottom via RectBMB(),
   and then output continues.  This means that initial output to the
   printer goes very rapidly, but once you hit the bottom there is a
   significant decrease in speed.  Instead of scrolling the bitmap, it
   would be better to keep a couple of pointers and "wrap" the output
   (and display) vertically around the bitmap, so there was never any
   need to scroll, just erase a small band in the "gap" between beginning
   and end, and adjust a few pointers.
5) When doing a PgUp keystroke in the debugger, the ScrollUp() function 
   can sometimes fail to move the code at all, so you have to use up
   cursor to get "over the hump".  This is most noticed in the RAM area,
   where large blocks of RAM are allocated on a single line.
6) Need to add a memory dump (similar to the CPU registers, but for
   viewing user-chosen RAM/ROM addresses) option.
7) Although I tried to write most of the emulator so that it wouldn't
   be sensitive to "big-endian" vrs "little-endian" memory addressing
   on different computer hardware, it's very likely that there is still
   some "endian" sensitivities, so if you try to port the emulator to a
   "big-endian" architecture, keep an eye out for those!
8) The UI for LIF file import/export is a terrible HACK, and needs to be
   rewritten completely.
9) The Tape and Disc loading dialogs are also complete hacks, and really
   need to use List Boxes, not Radio Buttons.  But that's a LOT of
   code for just those uses.  Maybe someday.

==========================================================================
RESOURCES
==========================================================================
The home page for this emulator is at:
  http://www.kaser.com/hp85.html
  
At the time of this writing, these other web sites exist that
contain a great deal of information and files regarding the 
HP Series 80 computers, including ROM images, software, and manuals
for both the HP-85 and the HP-86/87:
  http://www.series80.org/index.html
  http://www.hpmuseum.org/
  http://www.vintagecomputers.freeserve.co.uk/index.htm
  http://www2.akso.de/files/series_80/

A completely different emulator for the Series 80 was written by
Olivier De Smet.  His emulator can be downloaded from:
  http://olivier.2.smet.googlepages.com/hpseries80

I have no connection with any of these additional websites, so 
WYSIWYG ("what you see is what you get").

==========================================================================
HISTORY
==========================================================================
My connection with the HP-85 doesn't go back to its conception, which
I believe happened at the old APD (Advanced Products Division) of HP
in the San Francisco Bay area in the mid-1970's before I even worked
for HP, and before that division moved to Corvallis in the summer of 1976.
But the HP-85, in many ways, shaped the direction of much of my life.

I joined HP in Corvallis, Oregon on Oct 25, 1976, about three months
after the plant opened there, as a "Production I" worker, preforming 
ICs for hand-loading onto calculator PC boards.  My salary was $580 
per month (that was BEFORE taxes!) I'd attended college at OSU in
Corvallis for 5 years, starting in Electrical Engineering, but
switching to Art (Custom Designed Jewelry and Metalsmithing) after
a year and a half, and that was the B.S. degree that I got in 1976.  I
decided I didn't want to be a jeweler, and needed work, put in an
application at the newly opened Hewlett-Packard plant in Corvallis.  I
was to find out later how very lucky I was: they opened job applications
for ONE week, and took in over 6000 applications.  Out of those, they
hired 300 people, of which I was one.

At the time, the HP-97 (Topcat) and the HP-67 were the top-end calculators
being manufactured, along with the 20-series (Woodstock).  Over the next
two years, I worked my way through manufacturing into the PC (Printed
Circuit) board Test and Repair area, and then was promoted to a
"Technician I" position, one of only two technicians in the entire 
calculator production area.  In 1979, the R&D lab was working on two major
new products: the HP-41 and the HP-85.  It was decided that I would work 
on one of those, and the other tech (Jan) would work on the other.  Jan 
was the more experienced tech, so he was assigned the HP-85 and I was 
assigned the HP-41.  We were to work with the Production Engineers, who
were working with the R&D engineers, to transition the products from the
Lab to Production.  That process started more than a year in advance (as
production lines don't just spring into being overnight).  But while I 
was working on the HP-41, I was very interested in the HP-85, too.

When I was still in college, I saw an article in Scientific American
about John Conway's "Game of Life" (cellular automata).  This was back
before cellular automata and chaos theory and many other things were
very widely known (or even existed), and I was FASCINATED by it.  I
can remember sitting in the jewelry lab at OSU with graph paper, working
out generation after generation of the Game of Life by hand! :-)  In
the late 1970's (1978?) (just about the time I was joining HP), Popular
Electronics magazine published a two-part article on building a COMPUTER,
the COSMAC ELF.  It was all on one small board, used an RCA 1802 CPU, had
8 toggle switches and a push button for entering programs and data (into
256 BYTES of memory), and two hex-digit displays and a single LED for
output.  Remembering some of my early electronics training from OSU, I set
out to build one, and one of the first programs I wrote for it was a small
Game of Life (there was also a project for hooking up a small CRT monitor,
which used the entire 256 bytes of memory (including the program space)
as the data for the CRT output.  Eventually I designed and built a 16K
byte memory board to plug into the thing.

So, when the HP-85 came along, I was VERY interested in learning to 
program one so that I could use IT to crank out screen after screen of
the Game of Life.  I talked Jan into introducing me to the lab software
engineers, and fortunately they were extremely nice guys (Nelson Mills,
Kent Henscheid, Homer Russell, and George Fichter).  Instead of "Go away,
kid, don't bother me!" they took time from their certainly busy schedule
to explain things to me about the workings of the HP-85.  They did all of
their work on terminals hooked up to HP 1000 systems, writing and 
assembling source code, etc.  There was one wire-wrapped breadboard for
the four software engineers to test their code on.  It stood 6 feet tall
and about 2 feet square, and each of the "controllers" (keyboard, crt,
tape, printer, IO) were on separate huge wire-wrapped boards, built up
from discrete TTL logic ICs.  The first actual prototypes of the HP-85
with real ICs weren't available until sometime during 1979, and even
then the system ROM code was still tested and debugged mostly on the
breadboard.  For the prototypes, the system ROM code was burned into
EPROMs which were then plugged into custom designed PC boards that
plugged into the backplane I/O slots.  But the software designers needed
the breadboard so that they could rapidly make changes to the code, set
breakpoints, single-step, etc.

Someone had written a BASIC program "text editor", and one of the Lab
engineers had written a BASIC language "assembler".  It read the source
file from tape one line at a time, and assembled it into a string
variable (using each character of the string to hold one byte of the
assembled program.  He then wrote a binary program (on the HP 1000 system)
that could be used to write that string variable to tape as a binary
program type file.  It was very crude, and VERY slow, but it worked.  I
procured a copy of it from him, and proceeded to learn to write assembly
language programs for the HP-85 (by that time there were a few prototypes
in the production area).  I'd work on the code at home, then go into HP
early in the morning and type in the programs and assemble and debug
them.  I remember asking Nelson, "Now, tell me again, what exactly is
'parsing'???"  I was ignorant, and they were patient and helpful!  One
time, I kept getting an error on this one line of this simple little
program, and I couldn't figure out WHY it was happening.  I went over and
asked Nelson to look at it, and he stared at it and scratched his head.
Neither one of us could figure out what was going on.  Finally, the light
dawned (for him).  I'd typed an uppercase letter-O intead of the number 0
on that line.  Sigh.

(This is a long story, isn't it??? :-)

Now, one of the Applications Engineers, Bill Kemper, used to work at the
HP division in Loveland, Colorado, which is where the HP 98xx computers
were made before they moved to the new Ft. Collins division.  The HP-85
was somewhat of a "rival" product to the desktop computers that the 
Ft. Collins division were making.  They weren't too happy about us
creating a lower-cost machine that they saw as cutting into their market,
so there was a bit of rivalry (some might even say animosity) between the
two divisions.  Bill was working on a Waveform Analysis pack for the
HP-85, and his Fast Fourier Transform took 80 seconds to process 256
"points" of data, significantly longer than Ft. Collins' higher powered
(and higher priced) computers.  Having worked with some of the folks
there, Bill wanted to "beat them".  The lab guys were too busy with their
jobs to help, so he asked me if I thought I could write an assembly
language program to increase the speed of his FFT subroutine.  At first I
balked.  I'd barely been made aware of FFTs when I was still studying
Electrical Engineering at OSU, and THAT had been about 8 years earlier.
But then, the more he talked about it, I realized that what he was asking
for was merely converting the BASIC subroutine to assembly language, not
redesigning it or anything.  So, I really didn't need to KNOW anything
about FFTs, I just needed to know how to convert from BASIC to assembly,
and use the system ROM routines to fetch and store variables and do the
same math on them that was being done in the BASIC subroutine.  So, I
agreed to try.  (I was still working in production as a Tech at this time,
remember, so all of this was "on the sly", mostly done at home in evenings
and very early in the mornings at HP.)

Several weeks later, I finally succeeded.  The time to do the transform
was cut to right around 20 seconds, which was fast enough to beat the
Ft. Collins machine.  But in the process, I'd written about 750 lines of
assembly language code to do what his BASIC subroutine had been doing,
and the BASIC assembler that I was using took 30 MINUTES to assemble
those 750 lines.  Each time.  Every time I had to make a change, 30 more
minutes.  Make a stupid typing error: 30 more minutes.  So, by the time
I was done, I knew what my next project was going to be: an assembly
language assembler.  I was planning to do it as a binary program (what
else would I do?), but when I mentioned my plans to Nelson and Kent (the
two lab engineers that I bothered the most :-), they suggested I write it
as ROM code instead.  That would never have occured to me, but then I
realized that there were EPROM boards that plug-in ROMs could be run
from, so I said, "Why not?"  This was probably getting close to the Fall
of 1979.

Writing it as a ROM, though, meant that I had to write code at night,
then go in early in the morning and type the code into the R&D lab's
HP 1000 system, where it would be assembled and I could load it into the
breadboard to test and debug.  Every night and morning, another cycle
of writing, assembling, and debugging.  Within a couple of months, I had
a rudimentary assembler working out of ROM, sufficiently so that I could
burn it into EPROMs and use it in real prototypes to write and assemble
binary programs (but not the ROM itself, as there was no way to move the
files back to the system for burning the EPROMs).  And it was FAST! :-)

Then, introduction approached, and all of the marketing and lab management
and upper-level engineers went out on "New Product Tour", showing the new
product to the sales force and retail stores, different groups covering
different cities, all around the US and Europe.  When they came back after
the tour and started comparing notes, they realized that almost everywhere
they went, people asked, "What languages can it be programmed in?  Just
BASIC?  No assembly language?  You really need assembly language!"  They
realized that they really needed an assembler product for the HP-85.  It
was then that the lab engineers cleared their throats and said, "Ummm, we
KNOW someone that already has one working..."  The next thing I know, I'm
called into the R&D lab to give demos to just about every marketing and
R&D manager except for the division manager.  Within days, the R&D 
managers are out talking to the production managers about "loaning me" to
them to turn my assembler ROM into a "real" product.

SIDE STORY: introduction of the HP-85, in December of 1979, was a
potential disaster.  Corvallis Division had never built anything like that
(everything was "pocket sized" before), and there were a host of
production problems resulting in a LOT of problems in the shipped items.
Upper management at Corvallis fortunately made the right call, and teams
were created from just about every able body (and mind) that knew ANYTHING
about the HP-85, and they were sent out to every retail outlet that was
carrying the HP-85 in the U.S. and Europe, to take apart and examine EVERY
HP-85 that had been shipped.  Anything that was fixable in the field was
fixed, if not, the unit was shipped back to Corvallis for repair (and
there were a LOT of problems found).  What could have been a disaster
turned into a real PR coup with the retail folks.  "Wow, these HP folks
really stand behind their products!  They're sending engineers right to
my store to check and repair my stock!!!"  It turned out very well, and
production quickly got their act together, and that's how I made my first
HP business trip, to Dallas, Texas across to the Florida panhandle,
and back to New Orleans.  Now, BACK TO THE STORY...

By this time, the HP-41 had already been introduced, and in my production
tech job, I'd moved on to the 5 1/4" disk drives for the HP-85. Those
were pretty well ready for production ramp, so my time was apparently
fairly flexible, and off to the R&D lab I went.  They hired an outside
contract writer to write the manual for the Assembler ROM while I
continued to work on the code and work with the manual writer to provide
him with all the technical information and explanations he needed in
order to write the manual.  By mid-year, the HP-85 Assembler ROM
product was done.  I remember standing in the lab, with Nelson, Kent,
and Bill (the Applications Engineer, which was under Marketing), and
Bill was telling Nelson and Kent they should hire me, and they
were saying they didn't have any openings, that Bill should hire me.
I just stood there grinning, not thinking they were really serious.
But they were.  Within a few weeks, the Applications Engineering group
had opened a job requisition.  It's kind of funny, I don't know WHERE
they thought they were going to find someone to fill it.  The req
specified that the applicant had to be familiar with HP-85 computer,
with HP-85 assembly language, knowledgeable with the system BASIC
operation from an internal, low-level view-point, have experience with
the HP-85 Assember ROM, etc, etc.  That's how you write a job req when
there's ONE particular person you want. :-)  By August 1980, I was an
official Hewlett-Packard Software Applications Engineer.  They told me
later how they had taken the req to the Personnel Department (what's 
now called "Human Resources"), along with my application, saying they
wanted to hire me for the position.  Personel scanned through the req
and my application, and said, "What? Are you guys crazy?  This guy has
an ART degree!  Are you SURE you want to hire this guy?"  And it was
ONLY because I HAD a 4-year B.S. degree that they could hire me as a
software engineer, because HP's personnel guidelines required all
engineers to have a 4-year degree.  They just didn't specify in WHAT
the degree had to BE.

SIDE STORY: Someone in Ft. Collins Colorado had some metal "Capricorn"
belt-buckles made for the HP-85 sales force (I think).  Bill Kemper
received some of these, and he gave me one of them as thanks.  I still
have the buckle, which has zodiacal capricorn goat on the front, and
on the back says, "(c) CAPRICORN HP-85 A/S * DESIGNED AND SCULPTURED
BY: HAL PLATT * HEWLETT-PACKARD CO. FT. COLLINS CO. * NOGARIS ART
FOUNDRY".  I wore that belt buckle for 15-20 years, until I got tired
of wearing a belt...

Anyway, I worked in Applications Engineering for a year or two, until it
was disbanded and absorbed into the R&D lab (another interesting side-
story, but maybe for another time...)  I worked in the R&D lab until
late 1983, at which time I moved back into Marketing into the support
group (helping to solve problems in the field, supporting Field
Engineers and Sales Reps, etc), where I worked until 1986.  Of course,
I also wrote the HP-87 Assembler ROM (and helped to put together all the
information that the writer needed for the manual), and also finished up
the HP-87 Advanced Programming ROM.  I also wrote a bunch of the games in
the HP-85 Games Pak II, and all of the games in the HP-86/87 "Action 
Games" package, including the early "3-D maze" game Mouser, which could
be played solo, or between two players on two HP-86/87 machines connected
by HP-IB, Serial, Modem, or HP-IL.

After the HP-86/87 was completed, a group started work on "the next
generation" Series 80 machine, Aquarius.  They started designing a
new 16-bit CPU (similar in design to the HP-85/87 CPU, but "bigger and
better"), and an entirely new computer.  Unfortunately (but inevitably),
by that time the writing was on the wall, and that project was cancelled
before it got very far.  Around 1984, I spent $4200 of my own money to
buy an IBM PC for home.  It had a CGA 4-color display, two floppy drives
(no hard disk).  What a machine. :-)  At the time, it felt like taking
a step backwards.  You had to wait for the thing to boot its operating
system off the diskette, then you had to load the program you wanted to
run.  The fan was NOISY (contrasted with the silent HP-85/87).  But I
knew, by that point, where the world was headed.  Within a couple of
years I became proficient at programming IBM-compatible PCs and MS-DOS,
and released my first freeware game, Snarf, in Jan 1988, and then my
first shareware game, Solitile, in mid-1989.

Eventually, I spent time on the HP 110 (The Portable) and The Portable
Plus, one of the earliest "laptop" computers (the Grid machine had come
out just before the 110 was introduced).  Later I worked on a plug-in
Rocky Mountain BASIC computer board for IBM PC compatibles, and then
joined the team working on the HP 95LX (which was already underway),
writing the selftest code for it, then worked on the 100LX, and the
200LX.

But that's another story.  :-)  After working on HP's laptop computers
in the mid-1990's, I left HP in early 1997, and have been writing puzzle
and logic games ever since.

==========================================================================
REVISIONS
==========================================================================
I originally started an HP-85 emulator in December 2000.  I managed to
get the CPU, keyboard, RAM, and ROM mostly working (with a few problems),
but ran out of time and had to get back to "real" work.  Also, that was
based on some REALLY screwy code, and was compiled as a 16-bit Windows
application.  I never got back to it until early February 2006, at which
point I started over from scratch (in terms of the "framing code", but I
kept most of the HP-85 specific emulation code), and this is the result.
It's about time to get back, once again, to "real" work, but hopefully
it won't be another 5 years before I return.

I owe a great deal of thanks to Kent Kenscheid for his help in figuring
out how the HP-85 Tape Controller worked.  Without his help, the emulation
of the Tape Controller would have taken MUCH MUCH longer.  Thanks, Kent!

Feb 18, 2015 - Well, 9 years later... sigh.  But better late than never,
I suppose.  Finally, HP-85B and HP-87 emulation added.

HP-85 EMULATOR
   version 1.1 - Mar 24, 2006
   version 2.0 - Jun  1, 2006
   version 3.0 - Feb 18, 2015
     - Added emulation of HP-85B & HP-87, along with associated ROM images
     - Added color options to Emulator Options dialog
     - Fixed trash bug when IMPORTing LIF file to an EMPTY disk
     - Lengthened code labels from 6 to 8 (HP-87 Assembler ROM allowed 8)
     - Fixed: multi-byte length for BYT statement was ignored, 1 per line
     - Changed DISKS folder (limited to 30 disks in one folder) to support
       for 10 different folders (DISKS0 through DISKS9).
     - Changed disk dialog so that the NEW button gets disabled when the
       current DISKSx folder is full of disks.
     - SHIFT-click on 'IMPORT LIF FILE TO ":D700"' menu will import an
       entire FOLDER of LIFUTIL LIF files onto the current :D700 disk
       (assuming there's room). That's how I imported all of the diskettes
       distributed in DISKS1 (folder 1 in the 'disks' dialog that pops up
       when you click on a drive).  Note that the files in the diskettes
       in DISKS1 are a mish-mash of HP-85 and HP-87 files, and SOME were
       created before the final release of ROMs, so addresses in binary
       programs and source files MAY not be correct for the released ROMs.
       User beware.
   version 3.1 - Mar  4, 2015
    - FIXED: ROLL-DOWN key on HP-85 emulation was typing ELSE.
    - FIXED: EXT ROM had a bug in it's LOC opcode when at the same addr.
    - FIXED: The TAPE version of the 85 Standard Pak was empty
    - ADDED: "disks2\85 ASM ROM SRC (87 80)" which is HP-87 Assembler
       source files for the HP-85 Assembler ROM, disassembled and
       commented using the HP80DIS utility.
    - ADDED: "TYPE ASCII FILE AS KEYS" to menu.
    - FIXED: Program's ICON wasn't being used.
   version 3.2 - Jun 2016 (Fixed JSB X instruction trashing R2-3)
   version 3.3 - Jul 14, 2016 (Fixed SB and CM instructions, which
       weren't always setting the OVF flag correctly (which was
       preventing the FORTH ROMs and BPGMs from working), and added
       -LINE line-clearing keystrokes during "TYPE ASCII FILE..."
       execution to avoid problems when input line length was
       exactly the width of the screen.  Also, fixed keyboard
       emulation which was causing PROGRAM DEVELOPMENT ROM problems.
   version 3.4 - Aug 4, 2016 - Importing LIF files wasn't calculating
       the remaining space properly (volume and directory sectors
       were subtracted twice) so files didn't always fit on disk.
       Also added a pile of software disks to DISKS3.
   version 3.5 - Aug 28, 2016 - Enhanced "Import LIF File": if you
       have an HPWUTIL-created directory containing an HPWLIF.DIR
       file and a set of associated LIF files, you can 'import'
       the HPWLIF.DIR file and the emulator will REALLY import
       all of the individual LIF files in that folder that are
       listed IN the HPWLIF.DIR file.  You must still supply a
       'target' filename in the dialog, but it can be anything as
       it is ignored and the filenames in the HPWLIF.DIR file are
       used instead.
   version 3.5.1 - Nov 9, 2016 - Changed opcode 336 (NOP1) so that
       if it's executed, the emulator breaks into DEBUG mode.
   version 3.5.2 - Nov 11, 2016 - With EMS ROM loaded in HP-87
       mode, an "Unknown HP-IB command" error occurred.  This was
       caused by not implementing all of the HP-IB "Listen"
       commands.  The others that hadn't been used have been turned
       into no-ops.  Also, put in patch to SLOW DOWN typing when
       Program Development ROM is loaded AND two of the same char
       are being typed in a row, as the PD ROM was deleting the
       second one.
   version 3.5.3 - Dec 16, 2016
       1) 87 emulation didn't allow READ of CRTSAD & CRTRAM
       2) ADM R4,=offset wasn't implemented right, jump to ozone.
       3) With I/O ROM installed, PRINTER IS 710 got CR/LF at
          end of PLIST lines, but NOT at end of PRINT"" lines.
   version 3.5.5 - April 1, 2017 - Doing "STATUS 7, 9; A" would
       hang (CMD bit was being set in CCR *after* the CMD was
       already written to the OB, rather than writing to the OB
       DURING the CMD bit being high and then dropping CMD).
   version 4.0.0 - April 26, 2017 - MAJOR rewrite/reorganization of
       the code to add sub-windows for disk drives, help, etc,
       and to make the IO more 'modularized' for easier enhancement
       and addition of new peripherals.
       - Export of LIF file was always writing WS_FILE for the LIF
         file name in the exported file header.  It now writes the
         correct LIF file name.
       - Enable MOUSE WHEEL to scroll the internal printer up/down (if
         the mouse is ON the internal 'paper') and 'roll' the CRT up/down
         (if the mouse is ON the CRT area).
       - Moved the 82901 disk drive to its own window and added
         automatic CAT-listing of the current disks.
       - Moved the HELP to its own window, allowing it to stay open
         while using the emulated Series 80 (such as displaying the
         key mapping or whatever).  If there's things you have trouble
         remembering, you COULD edit the DOCS\README.TXT file to add
         them at the start of the file, then they'd easily be displayed.
   version 4.0.2 - May 6, 2017
       - ADDED checkboxes in DISK window to enable/disable auto-CAT for
         each of the drives (individually).
       - ADDED ability to move the HPIB card to any Select Code and/or
         add multiple HPIB cards at different Select Codes, each with
         its own 82901, file print, and hard print.  The lowest addressed
         disk drive on the highest Select Code is the DEFAULT mass
         storage device (that's Series 80, not the emulator).
       - ADDED CTL-LEFT-click on disk drive or tape cartridge causes
         MASS STORAGE IS ":Dxxx" (or :T) to be typed on Series 80
         along with a CR to enter the command, making it VERY easy to
         switch default disk drives. Obviously, this would interrupt a
         running program, so it's only intended for use when a program
         is not actively running.
       - ADDED double-click on PROG or BPGM in an auto-CAT listing causes
         LOAD "filename  :Dxxx" or LOADBIN "filename  :Dxxx" (as approp-
         riate) to be typed on Series 80 along with a CR to enter the
         command, making it VERY easy to load a BASIC or binary program
         from a disk, regardless of the current MSI.
       - FIXED: Some LIF disk images with files from non-Series 80 systems
         had files with "0 bytes per record" in the directory entry, and
         that caused the emulator to crash.  No more.
       - ADDED: Some more LIF file types for other systems.
       - CHANGED: text shown in auto-CAT for some LIF file types.
  version 4.0.3 - May 7, 2017
       - FIXED: Default disc drive wasn't always found and set at power-on.
  version 4.0.4 - May 10, 2017
       - FIXED: Without a Series80.ini file, would crash at startup
       - CHANGED: Finished separating all of the IO devices (code-wise)
          from the HPIB card emulation, to make things simpler, easier
          to modify and expand.
       - CHANGED: Print to File and print to 'real' printer are now
          movable (only by editing the .ini file manually, for now) to
          ANY HPIB device address from 1 to 30, except 0, where the disk
          drive is locked, for now, or 21, which is the Series 80's address.
       - ADDED: Purely cosmetic, but if you prefer the looks of a 9121
          3.5" drive to an 82901 5.25" drive, you can go into the
          EMULATOR OPTIONS dialog and check "Show 3.5" diskette drive".
  version 4.0.5 - May 11, 2017
       - FIXED: WRITING TO DISK was BROKEN.  Oops.
       - FIXED: INITIALIZE of a disk was BROKEN.  Double-Oops.
  version 4.1.0 - May 19, 2017
       - CHANGED: Major changes to .INI file settings for IO cards/devices.
       - REMOVED: OPTION to select 3.5" diskette graphics (now handled by
          type of device ADDED in DEVICES dialog).
       - CHANGED: Disk module routines to 'generalize' them so they work
          with all kinds of disks.
       - ADDED: DEVICES dialog to the SERIES 80 OPTIONS dialog.
       - ADDED: 8" diskette (9895A) support.
       - ADDED: 5MB hard disk (9135A) support.
       - ADDED: A section "CONFIGURING THE EMULATOR" to the help (this
         file) with a significant DEVICES sub-section.
       - ADDED: A line at the bottom of each auto-CAT that indicates the
          number of sectors used out of the total number of sectors on
          the disk and the resulting percentage used.  This 'used' count
          includes the VOLUME and DIRECTORY sectors.
       - CHANGED: default 'model' is now HP-85B instead of HP-85A, since
          the HP-85A is unable to use the 5MB hard disk.
       - CHANGED: bumped the minimum .INI file so that old .INI files will
          NOT be recognized by this new revision.  That's because of
          massive changes to the way HPIB and devices are now handled, and
          it just wasn't worth the effort to write a crap-load of code to
          handle the 'upgrade' for the few folks using the emulator.
          Sorry 'bout that.  Hopefully, going forward, I won't have to
          invalidate the .INI file any further.
       
==========================================================================
CONTACT INFORMATION
==========================================================================
MAIL:
    EVERETT KASER
    35405 SPRUCE ST SW
    ALBANY OR 97321-7507
PHONE:
    1-541-928-5259
EMAIL:
    everett@kaser.com
WWW:
    www.kaser.com
