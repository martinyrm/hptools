#ifndef _CODE_H_
#define _CODE_H_

#include "sload.h"
#include "debug.h"

#define NIB_BLOCKBITS 14
#define NIB_BLOCKSIZE (((long) 1) << NIB_BLOCKBITS)
#define BYTE_BLOCKSIZE (NIB_BLOCKSIZE / 2)
#define NIB_OFFSETMASK (NIB_BLOCKSIZE - 1)

extern char	*getblock(void);
extern char *getnullptr(void);
extern int getnib(i_32);
extern void putnib(i_32, int);
extern void putbyte(i_32, int);
#define readcode codereadcode
extern void readcode(i_32, i_32);
extern void writecode(i_32, i_32);
extern i_32 computecrc(i_32, i_32, i_32, i_32);
extern i_32 computekcsum(i_32, i_32);
extern i_32 computekcrc(i_32, i_32, i_32, i_32);
extern int chksum(int, int);
extern i_32 computeecrc(i_32, i_32);

#endif
