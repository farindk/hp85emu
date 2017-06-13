
#ifndef CRTWIDGET_H
#define CRTWIDGET_H

#include <QWidget>
#include <QPainter>

#include "bitmap.h"


class CrtWidget : public QWidget, public Bitmap
{
    Q_OBJECT

public:
    CrtWidget(QWidget *parent = 0) : QWidget(parent) {
    }

    ~CrtWidget() {
      delete mBitmap;
    }

    void paintEvent(QPaintEvent *event) {
      QPainter* painter = new QPainter(this);

      //mBitmap->getImage().save("test.png","PNG");

      if (mGraphicsScreen) {
        painter->drawImage(QRect(0,0,256,192), mBitmap->getImage(), QRect(0,192,256,192));
      }
      else {
        painter->drawImage(QRect(0,0,256,192), mBitmap->getImage(), QRect(0,0,256,192));
      }

      painter->setPen(QColor(255,0,0));
      //painter->drawLine(0,0,200,100);

      delete painter;
    }


    void createBitmap(int w,int h) {
      if (mBitmap) delete mBitmap;
      mBitmap = new Bitmap_Qt(w,h);
    }

    void FillRect(int x,int y, int w,int h, int32_t fill, int32_t border) {
      mBitmap->FillRect(x,y,w,h,fill,border);
    }

    void DrawPoint(int x,int y, int32_t brightness) {
      mBitmap->DrawPoint(x,y,brightness);
    }

    void Label85(int x,int y, int rowH, uint8_t* data, int n, int32_t brightness) {
      mBitmap->Label85(x,y,rowH,data,n,brightness);
    }

    void Flush(bool graphicsScreen) {
      mGraphicsScreen = graphicsScreen;
      update();
    }


private slots:

private:
  bool mGraphicsScreen=false;
  Bitmap_Qt* mBitmap = nullptr;
};

#endif
