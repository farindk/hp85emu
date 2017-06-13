/*******************************************************
  DLG.H - dialog header file for HP-85 emulator.
  Copyright 2006-2017 Everett Kaser
  All rights reserved
 See HP85EM.TXT for legal usage.
 *******************************************************/

#ifndef DLG_INCLUDED
 #define DLG_INCLUDED

#include	"ioDEV.h"	// make sure this is included BEFORE dlg.h, so DEVTYPE's are defined

//***********************************
// DLG.C dialog MESSAGES
#define	DLG_KEYDOWN		1
#define	DLG_KEYUP		2
#define	DLG_TIMER		3
#define	DLG_CHAR		4
#define	DLG_LBUTTDOWN	5
#define	DLG_LBUTTUP		6
#define	DLG_DRAW		7
#define	DLG_ACTIVATE	8
#define	DLG_CREATE		9
#define	DLG_KILLFOCUS	10
#define	DLG_SETFOCUS	11
#define	DLG_PTRMOVE		12
#define	DLG_SETTEXT		13

//*************************************
// DLG.C dialog/window/control structures
typedef struct __wnd {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives
	} WND;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		WND		*wndLCfocus;	// ptr to last child window to have focus
	} DLG;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		char	*pText;		// pointer to text label for this button
	} BUTT;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		char	*pText;		// pointer to text label for this radiobutton
	} RADIO;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		char	*pText;		// pointer to text label for this checkbox
	} CHECK;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		char	*pText;		// pointer to text label for this groupbox
	} GROUPBOX;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		char	Text[256];	// buffer for text being edited (max 255 chars with NUL)
		DWORD	buflen;	// len of edit buffer
		DWORD	textlim;// # of chars (excluding the terminating NUL) allowed in the edit buffer
		DWORD	winS;	// 0-based index into edit buffer of first visible character in edit line window
		DWORD	selS;	// 0-based index into edit buffer of start of selected text
		DWORD	selE;	// 0-based index into edit buffer of end of selected text
		DWORD	caret;	// 0-based index into edit buffer of position of edit cursor
		DWORD	shiftcaret;// 0-based index into edit buffer of position where SHIFT went down
		DWORD	lastBlinkTime;	// time of last redraw of ctl (for cursor blinking purposes)
		DWORD	cursorOn;	// non-0 if cursor is drawn, 0 if it's not drawn
	} EDIT;

typedef struct {
		WORD	id;		// unique application provided identification number
		WORD	type;	// DLG, BUTT, RADIO, etc
		long	xoff;	// x-offset from Parent's xoff, or absolute x-location if no parent
		long	yoff;	// y-offset from Parent
		long	w;		// width of window
		long	h;		// height of window
		struct __wnd	*prevsib;// pointer to WND of previous sibling (NULL if none)
		struct __wnd	*nextsib;// pointer to WND of next sibling (NULL if none)
		struct __wnd	*child1; // pointer to first child WND (NULL if none)
		struct __wnd	*parent; // pointer to parent (NULL if root/main WND)
		DWORD	flags;	// WF_ flags
		DWORD	(*wndproc)();	// pointer to window procedure function
		WORD	altchar;	// ALT-char code for shortcut to activate this CTL
		KWND	*kWnd;		// pointer to the KWND in which this dialog/CTL lives

		char	*pText;		// pointer to text label for this button
	} TEXT;

//****************************************
// DLG/CONTROL 'flags' bits
#define	WF_NOFOCUS		0x00000004	// set if window refuses to have the focus
#define	WF_CTL			0x00000008	// set if window is a control (button, checkbox, listbox, etc)

#define	CF_DEFAULT		0x00000100	// ctl is the default pushbutton for ENTER in a dialog

#define	CF_HELD_DOWN	0x00000200	// ctl is checked or in the "on" state temporarily by LEFT mouse button
#define	CF_CHECKED		0x00000400	// ctl is checked or in the "on" state
#define	CF_DOWN			0x00000600	// either CF_HELD_DOWN or CF_CHECKED is true
#define	CF_DISABLED		0x00000800	// ctl is disabled, cannot be activated until enabled
#define	CF_FOCUS		0x00001000	// ctl gets focus when dialog is created
#define	CF_TABSTOP		0x00004000	// tab'ing through a dialog will stop on this control
#define	CF_GROUP		0x00008000	// start of a group of ctls that are grouped together for tab'ing and cursoring
#define	CF_OPAQUE		0x00010000	// TEXT controls that get updated need background to be erased when redrawn
#define	CF_LEFT			0x00000000	// set nothing for left-justified TEXT control
#define	CF_CENTER		0x00020000	// center TEXT control (this bit is shared with CF_VERSCROLL)
#define	CF_RIGHT		0x00040000	// right-justify TEXT control (this bit is shared with CF_HORSCROLL)

//***************************************
// DLG/CONTROL 'type' values
#define	CTL_DLG			1
#define	CTL_BUTT		2
#define	CTL_CHECK		3
#define	CTL_RADIO		4
#define	CTL_GROUP		5
#define	CTL_EDIT		6
#define	CTL_TEXT		7

// DrawCtlText() ALIGN defines
#define	CTL_TOP_LEFT		0
#define	CTL_TOP_RIGHT		1
#define	CTL_TOP_CENTER		2
#define	CTL_BOTTOM_LEFT		4
#define	CTL_BOTTOM_RIGHT	5
#define	CTL_BOTTOM_CENTER	6
#define	CTL_MIDDLE_LEFT		8
#define	CTL_MIDDLE_RIGHT	9
#define	CTL_MIDDLE_CENTER	10

#define	CTL_TEST_UPDOWN		12
#define	CTL_TEST_LEFTRIGHT	3
#define	CTL_IS_LEFT			0
#define	CTL_IS_RIGHT		1
#define	CTL_IS_CENTER		2
#define CTL_IS_TOP			0
#define	CTL_IS_BOTTOM		4
#define	CTL_IS_MIDDLE		8

//**********************************************
// DLG and CTL IDs
#define	ID_CED				1
#define	ID_CED_CLOSE		2
#define	ID_CED_SAVE			3
#define	ID_CED_ADDRTEXT		4
#define	ID_CED_ADDREDIT		5
#define	ID_CED_LABELTEXT	6
#define	ID_CED_LABELEDIT	7
#define	ID_CED_REMTEXT		8
#define	ID_CED_REMEDIT		9
#define	ID_CED_DEL			10

#define	ID_CED_TYPEGROUP	20
#define	ID_CED_CODRADIO		21
#define	ID_CED_BYTRADIO		22
#define	ID_CED_ASCRADIO		23
#define	ID_CED_ASPRADIO		24
#define	ID_CED_BSZRADIO		25
#define	ID_CED_DEFRADIO		26
#define	ID_CED_VALRADIO		27
#define	ID_CED_RAMRADIO		28
#define	ID_CED_IORADIO		29
#define	ID_CED_DRPRADIO		30

#define	ID_CED_SIZETEXT		40
#define	ID_CED_SIZEEDIT		41

#define	ID_CED_KEEPOPEN		42

#define	ID_VAD				60
#define	ID_VAD_VIEW			61
#define	ID_VAD_CANCEL		62
#define	ID_VAD_ADDRTEXT		63
#define	ID_VAD_ADDREDIT		64
#define	ID_VAD_ROMGROUP		65
#define	ID_VAD_SYS			66
#define ID_VAD_GRAPHICS		67
#define	ID_VAD_PD			68
#define	ID_VAD_MIKSAM		69
#define	ID_VAD_LANG			70
#define	ID_VAD_EXT			71
#define	ID_VAD_ASM			72
#define	ID_VAD_STRUCT		73
#define	ID_VAD_FOR			74
#define	ID_VAD_MAT			75
#define	ID_VAD_MAT2			76
#define	ID_VAD_IO			77
#define	ID_VAD_EMS			78
#define	ID_VAD_MS			79
#define	ID_VAD_ED			80
#define	ID_VAD_SERV			81
#define	ID_VAD_AP2			82
#define	ID_VAD_AP			83
#define	ID_VAD_PP			84

#define	ID_ROMD				500
#define	ID_ROMD_NOCARE		501
#define	ID_ROMD_OK			502
#define	ID_ROMD_CANCEL		503

#define ID_MODEL_GROUP		504
#define	ID_MODEL_85A		505
#define	ID_MODEL_85B		506
#define	ID_MODEL_87			507

#define	ID_ROMD_ROMGROUP	508
#define	ID_ROMD_GRAPHICS	509
#define	ID_ROMD_PD			510
#define	ID_ROMD_MIKSAM		511
#define	ID_ROMD_LANG		512
#define	ID_ROMD_EXT			513
#define	ID_ROMD_ASM			514
#define	ID_ROMD_STRUCT		515
#define	ID_ROMD_FOR			516
#define	ID_ROMD_MAT			517
#define	ID_ROMD_MAT2		518
#define	ID_ROMD_IO			519
#define	ID_ROMD_EMS			520
#define	ID_ROMD_MS			521
#define	ID_ROMD_ED			522
#define	ID_ROMD_SERV		523
#define	ID_ROMD_AP2			524
#define	ID_ROMD_AP			525
#define	ID_ROMD_PP			526

#define	ID_ROMD_RAMGROUP	530
#define	ID_ROMD_CPURAM		531
#define	ID_ROMD_16			532
#define	ID_ROMD_32			533
#define	ID_ROMD_EXTENDED	534
#define	ID_ROMD_0E			535
#define	ID_ROMD_32E			536
#define	ID_ROMD_64E			537
#define	ID_ROMD_128E		538
#define	ID_ROMD_256E		539
#define	ID_ROMD_512E		540
#define	ID_ROMD_1024E		541

#define ID_ROMD_DEVICES		550

#define	ID_OPTD				610
#define	ID_OPTD_OPTGROUP	611
#define	ID_OPTD_DOUBLE		612
#define	ID_OPTD_AUTORUN		613
#define	ID_OPTD_NATURAL		614
#define	ID_OPTD_3INCH		615

#define	ID_OPTD_BASEGROUP	620
#define	ID_OPTD_8RADIO		621
#define	ID_OPTD_10RADIO		622
#define	ID_OPTD_16RADIO		623

#define	ID_OPTD_COLORS_GROUP		624
#define	ID_OPTD_MAINCLR_TEXT		625
#define	ID_OPTD_MAINCLR_EDIT		626
#define	ID_OPTD_MAINTEXTCLR_TEXT	627
#define	ID_OPTD_MAINTEXTCLR_EDIT	628
#define	ID_OPTD_MENUSELBACKCLR_TEXT	629
#define	ID_OPTD_MENUSELBACKCLR_EDIT	630
#define	ID_OPTD_MENUSELTEXTCLR_TEXT	631
#define	ID_OPTD_MENUSELTEXTCLR_EDIT	632
#define	ID_OPTD_MENUHICLR_TEXT		633
#define	ID_OPTD_MENUHICLR_EDIT		634
#define	ID_OPTD_MENULOCLR_TEXT		635
#define	ID_OPTD_MENULOCLR_EDIT		636
#define	ID_OPTD_MENUBACKCLR_TEXT	637
#define	ID_OPTD_MENUBACKCLR_EDIT	638
#define	ID_OPTD_MENUTEXTCLR_TEXT	639
#define	ID_OPTD_MENUTEXTCLR_EDIT	640
#define	ID_OPTD_PRINTCLR_TEXT		641
#define	ID_OPTD_PRINTCLR_EDIT		642

#define	ID_OPTD_OK			650
#define	ID_OPTD_CANCEL		651
#define	ID_OPTD_DEFAULT		652


#define	ID_COMD				130
#define	ID_COMD_OK			131
#define	ID_COMD_CANCEL		132
#define	ID_COMD_REMEDIT		133
#define	ID_COMD_REMTEXT		134
#define	ID_COMD_DEL			135
#define	ID_COMD_KEEPOPEN	136

#define	ID_EVD				140
#define	ID_EVD_VALTEXT		141
#define	ID_EVD_VALEDIT		142
#define	ID_EVD_OK			143
#define	ID_EVD_CANCEL		144


#define	ID_TAPD				150
#define	ID_TAPD_DELETE		151
#define	ID_TAPD_CLOSE		152
#define	ID_TAPD_WP			153
#define	ID_TAPD_TAPEGROUP	154
#define	ID_TAPD_NEW			155
#define	ID_TAPD_RENAME		156

#define ID_TAPD_TAPE0RADIO	160
#define	ID_TAPD_TAPE1RADIO	161
#define	ID_TAPD_TAPE2RADIO	162
#define	ID_TAPD_TAPE3RADIO	163
#define	ID_TAPD_TAPE4RADIO	164
#define	ID_TAPD_TAPE5RADIO	165
#define	ID_TAPD_TAPE6RADIO	166
#define	ID_TAPD_TAPE7RADIO	167
#define	ID_TAPD_TAPE8RADIO	168
#define	ID_TAPD_TAPE9RADIO	169
#define	ID_TAPD_TAPE10RADIO	170
#define	ID_TAPD_TAPE11RADIO	171
#define	ID_TAPD_TAPE12RADIO	172
#define	ID_TAPD_TAPE13RADIO	173
#define	ID_TAPD_TAPE14RADIO	174
#define	ID_TAPD_TAPE15RADIO	175
#define	ID_TAPD_TAPE16RADIO	176
#define	ID_TAPD_TAPE17RADIO	177
#define	ID_TAPD_TAPE18RADIO	178
#define	ID_TAPD_TAPE19RADIO	179
#define	ID_TAPD_TAPE20RADIO	180
#define	ID_TAPD_TAPE21RADIO	181
#define	ID_TAPD_TAPE22RADIO	182
#define	ID_TAPD_TAPE23RADIO	183
#define	ID_TAPD_TAPE24RADIO	184
#define	ID_TAPD_TAPE25RADIO	185
#define	ID_TAPD_TAPE26RADIO	186
#define	ID_TAPD_TAPE27RADIO	187
#define	ID_TAPD_TAPE28RADIO	188
#define	ID_TAPD_TAPE29RADIO	189
#define	ID_TAPD_TAPE30RADIO	190




#define	ID_HELPD			300
#define	ID_HD_NOCARE		301
#define	ID_HD_CLOSE			302


#define	ID_REND				310
#define	ID_REND_OK			311
#define	ID_REND_CANCEL		312
#define	ID_REND_NEWTEXT		313
#define	ID_REND_NEWEDIT		314
#define	ID_REND_OLDTEXT		315
#define	ID_REND_OLDEDIT		316

#define	ID_DSKD_FOLDERGROUP	339
#define	ID_DSKD_FOLDER0		340
#define	ID_DSKD_FOLDER1		341
#define	ID_DSKD_FOLDER2		342
#define	ID_DSKD_FOLDER3		343
#define	ID_DSKD_FOLDER4		344
#define	ID_DSKD_FOLDER5		345
#define	ID_DSKD_FOLDER6		346
#define	ID_DSKD_FOLDER7		347
#define	ID_DSKD_FOLDER8		348
#define	ID_DSKD_FOLDER9		349

#define	ID_DSKD				350
#define	ID_DSKD_DELETE		351
#define	ID_DSKD_CLOSE		352
#define	ID_DSKD_WP			353
#define	ID_DSKD_DISKGROUP	354
#define	ID_DSKD_NEW			355
#define	ID_DSKD_RENAME		356

#define ID_DSKD_DISK0RADIO	360
#define	ID_DSKD_DISK1RADIO	361
#define	ID_DSKD_DISK2RADIO	362
#define	ID_DSKD_DISK3RADIO	363
#define	ID_DSKD_DISK4RADIO	364
#define	ID_DSKD_DISK5RADIO	365
#define	ID_DSKD_DISK6RADIO	366
#define	ID_DSKD_DISK7RADIO	367
#define	ID_DSKD_DISK8RADIO	368
#define	ID_DSKD_DISK9RADIO	369
#define	ID_DSKD_DISK10RADIO	370
#define	ID_DSKD_DISK11RADIO	371
#define	ID_DSKD_DISK12RADIO	372
#define	ID_DSKD_DISK13RADIO	373
#define	ID_DSKD_DISK14RADIO	374
#define	ID_DSKD_DISK15RADIO	375
#define	ID_DSKD_DISK16RADIO	376
#define	ID_DSKD_DISK17RADIO	377
#define	ID_DSKD_DISK18RADIO	378
#define	ID_DSKD_DISK19RADIO	379
#define	ID_DSKD_DISK20RADIO	380
#define	ID_DSKD_DISK21RADIO	381
#define	ID_DSKD_DISK22RADIO	382
#define	ID_DSKD_DISK23RADIO	383
#define	ID_DSKD_DISK24RADIO	384
#define	ID_DSKD_DISK25RADIO	385
#define	ID_DSKD_DISK26RADIO	386
#define	ID_DSKD_DISK27RADIO	387
#define	ID_DSKD_DISK28RADIO	388
#define	ID_DSKD_DISK29RADIO	389
#define	ID_DSKD_DISK30RADIO	390

#define	ID_RENDD			310
#define	ID_RENDD_OK			311
#define	ID_RENDD_CANCEL		312
#define	ID_RENDD_NEWTEXT	313
#define	ID_RENDD_NEWEDIT	314
#define	ID_RENDD_OLDTEXT	315
#define	ID_RENDD_OLDEDIT	316

#define	ID_IMPD				400
#define	ID_IMPD_OSTEXT		401
#define	ID_IMPD_OSEDIT		402
#define	ID_IMPD_LIFTEXT		403
#define	ID_IMPD_LIFEDIT		404
#define	ID_IMPD_OK			405
#define	ID_IMPD_CANCEL		406
#define	ID_IMPD_TITLETEXT	407

#define	ID_EXPD				410
#define	ID_EXPD_OSTEXT		411
#define	ID_EXPD_OSEDIT		412
#define	ID_EXPD_LIFTEXT		413
#define	ID_EXPD_LIFEDIT		414
#define	ID_EXPD_OK			415
#define	ID_EXPD_CANCEL		416
#define	ID_EXPD_TITLETEXT	417

#define	ID_TYPD				420
#define	ID_TYPD_TEXT		421
#define	ID_TYPD_EDIT		422
#define	ID_TYPD_OK			423
#define	ID_TYPD_CANCEL		424
#define	ID_TYPD_TITLETEXT	425

#define	ID_DWSHOW0			700
#define	ID_DWSHOW1			701

#define	ID_DEVD_OK			800
#define	ID_DEVD_CANCEL		801
#define	ID_DEVD				802
#define	ID_DEVD_DEVGROUP	803
#define	ID_DEVD_DEVSC		804
#define	ID_DEVD_DEVCA		805

#define	ID_DEVD_DEV00		810
#define	ID_DEVD_DEV01		811
#define	ID_DEVD_DEV02		812
#define	ID_DEVD_DEV03		813
#define	ID_DEVD_DEV04		814
#define	ID_DEVD_DEV05		815
#define	ID_DEVD_DEV06		816
#define	ID_DEVD_DEV07		817
#define	ID_DEVD_DEV08		818
#define	ID_DEVD_DEV09		819
#define	ID_DEVD_DEV10		820
#define	ID_DEVD_DEV11		821
#define	ID_DEVD_DEV12		822
#define	ID_DEVD_DEV13		823
#define	ID_DEVD_DEV14		824
#define	ID_DEVD_DEV15		825
#define	ID_DEVD_DEV16		826
#define	ID_DEVD_DEV17		827
#define	ID_DEVD_DEV18		828
#define	ID_DEVD_DEV19		829
#define	ID_DEVD_DEV20		830
#define	ID_DEVD_DEV21		831
#define	ID_DEVD_DEV22		832
#define	ID_DEVD_DEV23		833
#define	ID_DEVD_DEV24		834
#define	ID_DEVD_DEV25		835
#define	ID_DEVD_DEV26		836
#define	ID_DEVD_DEV27		837
#define	ID_DEVD_DEV28		838
#define	ID_DEVD_DEV29		839
#define	ID_DEVD_DEV30		840
#define	ID_DEVD_DEV31		841

#define	ID_DEVD_SC00		850
#define	ID_DEVD_SC01		851
#define	ID_DEVD_SC02		852
#define	ID_DEVD_SC03		853
#define	ID_DEVD_SC04		854
#define	ID_DEVD_SC05		855
#define	ID_DEVD_SC06		856
#define	ID_DEVD_SC07		857
#define	ID_DEVD_SC08		858
#define	ID_DEVD_SC09		859
#define	ID_DEVD_SC10		860
#define	ID_DEVD_SC11		861
#define	ID_DEVD_SC12		862
#define	ID_DEVD_SC13		863
#define	ID_DEVD_SC14		864
#define	ID_DEVD_SC15		865
#define	ID_DEVD_SC16		866
#define	ID_DEVD_SC17		867
#define	ID_DEVD_SC18		868
#define	ID_DEVD_SC19		869
#define	ID_DEVD_SC20		870
#define	ID_DEVD_SC21		871
#define	ID_DEVD_SC22		872
#define	ID_DEVD_SC23		873
#define	ID_DEVD_SC24		874
#define	ID_DEVD_SC25		875
#define	ID_DEVD_SC26		876
#define	ID_DEVD_SC27		877
#define	ID_DEVD_SC28		878
#define	ID_DEVD_SC29		879
#define	ID_DEVD_SC30		880
#define	ID_DEVD_SC31		881

#define	ID_DEVD_CA00		890
#define	ID_DEVD_CA01		891
#define	ID_DEVD_CA02		892
#define	ID_DEVD_CA03		893
#define	ID_DEVD_CA04		894
#define	ID_DEVD_CA05		895
#define	ID_DEVD_CA06		896
#define	ID_DEVD_CA07		897
#define	ID_DEVD_CA08		898
#define	ID_DEVD_CA09		899
#define	ID_DEVD_CA10		900
#define	ID_DEVD_CA11		901
#define	ID_DEVD_CA12		902
#define	ID_DEVD_CA13		903
#define	ID_DEVD_CA14		904
#define	ID_DEVD_CA15		905
#define	ID_DEVD_CA16		906
#define	ID_DEVD_CA17		907
#define	ID_DEVD_CA18		908
#define	ID_DEVD_CA19		909
#define	ID_DEVD_CA20		910
#define	ID_DEVD_CA21		911
#define	ID_DEVD_CA22		912
#define	ID_DEVD_CA23		913
#define	ID_DEVD_CA24		914
#define	ID_DEVD_CA25		915
#define	ID_DEVD_CA26		916
#define	ID_DEVD_CA27		917
#define	ID_DEVD_CA28		918
#define	ID_DEVD_CA29		919
#define	ID_DEVD_CA30		920
#define	ID_DEVD_CA31		921

#define	ID_DEVD_DEL00		930
#define	ID_DEVD_DEL01		931
#define	ID_DEVD_DEL02		932
#define	ID_DEVD_DEL03		933
#define	ID_DEVD_DEL04		934
#define	ID_DEVD_DEL05		935
#define	ID_DEVD_DEL06		936
#define	ID_DEVD_DEL07		937
#define	ID_DEVD_DEL08		938
#define	ID_DEVD_DEL09		939
#define	ID_DEVD_DEL10		940
#define	ID_DEVD_DEL11		941
#define	ID_DEVD_DEL12		942
#define	ID_DEVD_DEL13		943
#define	ID_DEVD_DEL14		944
#define	ID_DEVD_DEL15		945
#define	ID_DEVD_DEL16		946
#define	ID_DEVD_DEL17		947
#define	ID_DEVD_DEL18		948
#define	ID_DEVD_DEL19		949
#define	ID_DEVD_DEL20		950
#define	ID_DEVD_DEL21		951
#define	ID_DEVD_DEL22		952
#define	ID_DEVD_DEL23		953
#define	ID_DEVD_DEL24		954
#define	ID_DEVD_DEL25		955
#define	ID_DEVD_DEL26		956
#define	ID_DEVD_DEL27		957
#define	ID_DEVD_DEL28		958
#define	ID_DEVD_DEL29		959
#define	ID_DEVD_DEL30		960
#define	ID_DEVD_DEL31		961

/// NOTE: These IDs are assumed to have the same order and relative number sequencing as DEVTYPE_ to make id-math easy
#define	ID_DEVD_ADD_DISK525		(970+DEVTYPE_5_25)
#define	ID_DEVD_ADD_DISK35		(970+DEVTYPE_3_5)
#define	ID_DEVD_ADD_DISK8		(970+DEVTYPE_8)
#define	ID_DEVD_ADD_DISK5MB		(970+DEVTYPE_5MB)

#define	ID_DEVD_ADD_FILEPRT		(970+DEVTYPE_FILEPRT)
#define	ID_DEVD_ADD_HARDPRT		(970+DEVTYPE_HARDPRT)

//*****************************
// DLG.C globals
extern DLG *ActiveDlg;
extern BYTE	Reg[64];
extern int	NextDevID;	// unique device ID that starts at 0 when program is first run, gets incremented with each device read in and with each device added
extern BOOL	DlgNoCap;

//**********************************************************************
// DLG.C function prototypes
DWORD SendMsg(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
void CreateTypeASCIIDlg();
void CreateCodeEditDlg();
void CreateViewAddressDlg();
void CreateCommentDlg();
void CreateRomDlg();
void CreateOptionsDlg();
void CreateHelpDlg();
DWORD EditReg(DWORD r, DWORD v);
void CreateTapeDlg();
void CreateDiskDlg(int sc, int tla, int d, int devType);
void CreateImportDlg();
void CreateImportAllDlg();
void CreateExportDlg();
void SetCheck(WND *pWnd, DWORD checked);
DWORD GetCheck(WND *pWnd);

DWORD ButtonProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD TextProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD CheckProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD EditProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD GroupboxProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD RadioButtonProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD DefDialogProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD COMDokProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);
DWORD COMDdelProc(WND *pWnd, DWORD msg, DWORD arg1, DWORD arg2, DWORD arg3);

#endif // DLG_INCLUDED

