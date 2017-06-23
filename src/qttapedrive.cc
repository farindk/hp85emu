
#include "qttapedrive.h"

#include <QFileDialog>


void QtTapeDrive::mousePressEvent(QMouseEvent* event)
{
  printf("tape drive clicked\n");

  if (mTapeDrive->isCartridgeInserted()) {
    mTapeDrive->EjectTape();
  }
  else {
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Tape Cartridge"), "",
                                                    0);

    printf("filename: %s\n",fileName.toUtf8().constData());

    auto tape = std::make_shared<Tape>();
    tape->Load(fileName.toUtf8().constData());
    mTapeDrive->InsertTape(tape);
  }
}
