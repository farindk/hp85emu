
#include "mainwindow.h"
#include "main.hh"

#include <ctype.h>


void MainWindow::keyPressEvent(QKeyEvent *event) {
  fprintf(stderr,"keypress %c\n",event->key());

  enum HP85_Key k = (enum HP85_Key)-1;

  int key = event->key();

  switch (key) {
  case Qt::Key_Backspace: k=HP85_Key_Back;   break;
  case Qt::Key_Return:    k=HP85_Key_Return; break;
  case Qt::Key_Left:      k=HP85_Key_Left;   break;
  case Qt::Key_Up:        k=HP85_Key_Up;     break;
  case Qt::Key_Right:     k=HP85_Key_Right;  break;
  case Qt::Key_Down:      k=HP85_Key_Down;   break;

  default:
    if (key>=0 && key<=255) {
      HP85OnChar(key);
    }
  }

  if (k>=0) {
    getMachine()->processKeyCode(k);
  }
}
