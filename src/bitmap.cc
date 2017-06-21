
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
