
#include "mainwindow.h"
#include "main.hh"

#include <ctype.h>


struct KeyMapping {
  enum Qt::Key qtKey;
  int16_t normal;
  int16_t shift;
  int16_t control;
  int16_t shiftControl;
};


static KeyMapping keys_hp85[] = {
  { Qt::Key_At,    64, 64,  0,  0 },
  { Qt::Key_A,     97, 65,  1,  1 },
  { Qt::Key_B,     98, 66,  2,  2 },
  { Qt::Key_C,     99, 67,  3,  3 },
  { Qt::Key_D,    100, 68,  4,  4 },
  { Qt::Key_E,    101, 69,  5,  5 },
  { Qt::Key_F,    102, 70,  6,  6 },
  { Qt::Key_G,    103, 71,  7,  7 },
  { Qt::Key_H,    104, 72,  8,  8 },
  { Qt::Key_I,    105, 73,  9,  9 },
  { Qt::Key_J,    106, 74, 10, 10 },
  { Qt::Key_K,    107, 75, 11, 11 },
  { Qt::Key_L,    108, 76, 12, 12 },
  { Qt::Key_M,    109, 77, 13, 13 },
  { Qt::Key_N,    110, 78, 14, 14 },
  { Qt::Key_O,    111, 79, 15, 15 },
  { Qt::Key_P,    112, 80, 16, 16 },
  { Qt::Key_Q,    113, 81, 17, 17 },
  { Qt::Key_R,    114, 82, 18, 18 },
  { Qt::Key_S,    115, 83, 19, 19 },
  { Qt::Key_T,    116, 84, 20, 20 },
  { Qt::Key_U,    117, 85, 21, 21 },
  { Qt::Key_V,    118, 86, 22, 22 },
  { Qt::Key_W,    119, 87, 23, 23 },
  { Qt::Key_X,    120, 88, 24, 24 },
  { Qt::Key_Y,    121, 89, 25, 25 },
  { Qt::Key_Z,    122, 90, 26, 26 },
  { Qt::Key_Space, 32, 32, 32, 32 },
  { Qt::Key_BracketLeft, 91,91,91,91 },
  { Qt::Key_Backslash,   92,92,92,92 },
  { Qt::Key_BracketRight,93,93,93,93 },
  { Qt::Key_AsciiCircum, 94,0246,94,0246 }, // + RESULT
  { Qt::Key_Underscore,  95,95,95,95 },
  { Qt::Key_Escape, 0226,0140,0226,0140 },
  { Qt::Key_Slash,  47,0173,47,0173 },
  { Qt::Key_Bar,   124,124,124,124 },
  { Qt::Key_Minus, 55,0175,55,0175 },
  { Qt::Key_Asterisk, 42,0176,42,0176 },
  { Qt::Key_Plus,     43,0177,43,0177 },
  { Qt::Key_F1,     0200,0204,0200,0204 },
  { Qt::Key_F2,     0201,0205,0201,0205 },
  { Qt::Key_F3,     0202,0206,0202,0206 },
  { Qt::Key_F4,     0203,0207,0203,0207 },
  { Qt::Key_F5,     0204,0204,0204,0204 },
  { Qt::Key_F6,     0205,0205,0205,0205 },
  { Qt::Key_F7,     0206,0206,0206,0206 },
  { Qt::Key_F8,     0207,0207,0207,0207 },
  // TODO: HP85 key codes 0210-0255

  // TODO: REW
  { Qt::Key_Backspace, 0231,0233,0231,0233 },
  { Qt::Key_Return,    0232,0232,0232,0232 },
  { Qt::Key_Left,      0234,0223,0234,0223 },
  { Qt::Key_Up,        0241,0245,0241,0245 },
  { Qt::Key_Right,     0235,0211,0235,0211 },
  { Qt::Key_Down,      0242,0254,0242,0254 },
  // TODO: Paper Adv.
  // TODO: RESET
  // TODO: INIT
  // TODO: RUN
  // TODO: PAUSE
  // TODO: CONT
  // TODO: STEP
  { Qt::Key_F12,       0221,0221,0221,0221 }, // TEST
  // TODO: CLEAR SCREEN
  // TODO: LIST
  // TODO: PLIST
  { Qt::Key_PageUp,    0236,0236,0236,0236 }, // ROLL UP
  { Qt::Key_PageDown,  0237,0237,0237,0237 }, // ROLL DOWN
  { Qt::Key_Home,      0245,0245,0245,0245 },
  { Qt::Key_End,       0232,0232,0232,0232 }, // END LINE
  // TODO: CLR LINE
  { Qt::Key_Insert,    0243,0243,0243,0243 }, // INS/RPL
  { Qt::Key_Delete,    0244,0244,0244,0244 }, // DEL CHR
  // TODO: DELETE
  // TODO: STORE
  // TODO: LOAD
  // TODO: AUTO
  // TODO: SCRATCH
  { (Qt::Key)0,0,0,0 }
};


void MainWindow::keyPressEvent(QKeyEvent *event) {
  fprintf(stderr,"keypress %c %x, modifier: %x\n",event->key(), event->key(),
          event->modifiers());

  bool shiftPressed   = !!(event->modifiers() & Qt::ShiftModifier);
  bool controlPressed = !!(event->modifiers() & Qt::ControlModifier);

  int key = event->key();

  int16_t hpKey = -1;

  if (key >= 32 && key <= 63) {
    hpKey = key;
  }

  KeyMapping* mapping = keys_hp85;
  for (int i=0; mapping[i].qtKey; i++) {
    if (mapping[i].qtKey == key) {
      if (shiftPressed && controlPressed) {
        hpKey = mapping[i].shiftControl;
      }
      else if (shiftPressed) {
        hpKey = mapping[i].shift;
      }
      else if (controlPressed) {
        hpKey = mapping[i].control;
      }
      else {
        hpKey = mapping[i].normal;
      }
    }
  }


  if (hpKey != -1) {
    HP85OnChar(hpKey);
  }
}
