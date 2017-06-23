
#ifndef QTTAPEDRIVE_H
#define QTTAPEDRIVE_H

#include <QLabel>
#include <QPainter>

#include "tape.h"


class QtTapeDrive : public QLabel
{
    Q_OBJECT

public:
    QtTapeDrive(QWidget *parent = 0) : QLabel(parent) {
      mPixmap[0][0] = QPixmap("../images/tape-nocart-off.png");
      mPixmap[0][1] = QPixmap("../images/tape-nocart-on.png");
      mPixmap[1][0] = QPixmap("../images/tape-cart-off.png");
      mPixmap[1][1] = QPixmap("../images/tape-cart-on.png");

      setPixmap(mPixmap[1][0]);
    }

    void updateDriveState(TapeDrive& drive) {
      int power = drive.isPowerOn() ? 1 : 0;
      int cartridge = drive.isCartridgeInserted() ? 1 : 0;

      setPixmap(mPixmap[cartridge][power]);
    }

private slots:

private:
  QPixmap mPixmap[2][2]; // [cartridge][power]
};

#endif
