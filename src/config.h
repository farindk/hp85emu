
#ifndef CONFIG_H
#define CONFIG_H

#include <inttypes.h>

#define TODO 0

typedef uint8_t BYTE;
typedef bool BOOL;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef int LONG;

typedef int PBMB;
//typedef int DEV;

typedef struct _RECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT;

#define FALSE false
#define TRUE true

#define	PATHLEN	512

#define DEVELOP 1

#if DEVELOP
#define logging printf
#else
#define logging null_func
inline void null_func(const char* fmt, ...) { }
#endif

#endif
