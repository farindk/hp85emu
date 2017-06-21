
#ifndef BITMAP_QT_H
#define BITMAP_QT_H

#include <inttypes.h>

#include <QWidget>
#include <QPainter>

#include "bitmap.h"


class Bitmap_Qt : public Bitmap
{
 public:
 Bitmap_Qt(int w,int h) : mImage(w,h, QImage::Format_Grayscale8) { }
  ~Bitmap_Qt() { }

  void FillRect(int x,int y, int w,int h, int32_t fill, int32_t border);
  void DrawPoint(int x,int y, int32_t brightness);
  void Label85(int x,int y, int rowH, uint8_t* data, int n, int32_t brightness);
  void Flush(bool graphicsScreen) { }

  QImage getImage() { return mImage; }

 private:
  QImage mImage;
};


#endif
