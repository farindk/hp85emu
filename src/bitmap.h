
#ifndef BITMAP_H
#define BITMAP_H

#include <inttypes.h>


class Bitmap
{
 public:
  virtual ~Bitmap() { }

  virtual void FillRect(int x,int y, int w,int h, int32_t fill, int32_t border) = 0;
  virtual void DrawPoint(int x,int y, int32_t brightness) = 0;
  virtual void Label85(int x,int y, int rowH, uint8_t* data, int n, int32_t brightness) = 0;
  virtual void Flush(bool graphicsScreen) = 0;
};



class Bitmap_Console85 : public Bitmap
{
 public:
  Bitmap_Console85(int w,int h) { }
  ~Bitmap_Console85() { }

  void FillRect(int x,int y, int w,int h, int32_t fill, int32_t border) { }
  void DrawPoint(int x,int y, int32_t brightness) { }
  void Label85(int x,int y, int rowH, uint8_t* data, int n, int32_t brightness);
  void Flush(bool graphicsScreen);

 private:
  char mem[32*16];
};


#endif
