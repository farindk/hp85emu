
#include "bitmap.h"
#include "mach85.hh"

#include <stdio.h>

static char char85[128+1]=
  "<?xnabcn^o^nu rpPOdAaAaOoUuAa2FX"
  " !\"#$%&'()*+,-./0123456789:;<=>?"
  "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmnopqrstuvwxyzP|>S+";

void Bitmap_Console85::Label85(int x,int y, int rowH, uint8_t* data, int n, int32_t brightness)
{
  x /= 8;
  y /= 12;

  for (int i=0;i<n;i++) {
    mem[x+i+y*32] = data[i];
  }
}

void Bitmap_Console85::Flush(bool graphicsScreen)
{
  printf("--------------------------------\n");
  for (int y=0;y<16;y++) {
    for (int x=0;x<32;x++) {
      unsigned char c = mem[x+y*32];

      if (c >= 128) {
        c='_';
      }
      else {
        c = char85[c];
      }

      printf("%c",c);
    }
    printf("\n");
  }
}




void Bitmap_Qt::FillRect(int x,int y, int w,int h, int32_t fill, int32_t border)
{
  int bpl = mImage.bytesPerLine();
  uint8_t* p = &mImage.bits()[x + y*bpl];

  for (int y=0;y<h;y++)
    for (int x=0;x<w;x++) {
      p[x+y*bpl] = fill;
    }
}

void Bitmap_Qt::DrawPoint(int x,int y, int32_t brightness)
{
  mImage.bits()[x+  y*mImage.bytesPerLine()] = (brightness & 0xFF);
}

void Bitmap_Qt::Label85(int x,int y, int rowH, uint8_t* data, int n, int32_t brightness)
{
  int bpl = mImage.bytesPerLine();
  uint8_t* p = &mImage.bits()[x + y*bpl];

  brightness=0xff;

  for (int i=0;i<n;i++) {
    uint8_t c = data[i];

    //printf("label85: %d %d   %d\n",x+i,y,c);

    for (int r=0;r<8;r++) {
      uint8_t pattern = HP85font[c & 0x7F][r];

      uint8_t bit=0x80;
      for (int dy=0 ; dy<8 ; dy++,bit>>=1) {
        if (pattern & bit) {
          p[8*i+r+dy*bpl] = brightness;
        }
        else {
          p[8*i+r+dy*bpl] = 0;
        }
      }
    }

    uint8_t underline = (c>=128) ? brightness : 0;

    for (int dx=0;dx<8;dx++) {
      p[8*i+dx+8*bpl] = (dx<7) ? underline : 0;
      p[8*i+dx+9*bpl] = (dx<7) ? underline : 0;

      p[8*i+dx+10*bpl] = 0;
      p[8*i+dx+11*bpl] = 0;
    }
  }
}
