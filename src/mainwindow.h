
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QApplication>
#include <QKeyEvent>

#include "ui_mainwindow.h"
#include "bitmap.h"
#include "mach85.hh"


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0) : QMainWindow(parent)
    {
      ui.setupUi(this);

      setFocusPolicy(Qt::StrongFocus);
    }

    class CrtWidget* getCRTWidget() { return ui.crt; }

    void keyPressEvent(QKeyEvent *event);

    QtTapeDrive* getQtTapeDrive() { return ui.tapeDrive; }

private slots:
  void test() { printf("test\n"); }
  void quit() { printf("quit\n"); }

private:
  Ui::MainWindow ui;
};

#endif
