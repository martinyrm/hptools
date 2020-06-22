#include "config.h"

#include "convert.h"

#ifdef dll
#include "dllfile.h"
#endif

/* This works on every system (little endian, big endian) and not only 
 * little endian as Mogzy wrote
 */


void set_32(bit_32 destp, i_32 src)
{
  b_8 * d = (b_8 *) destp + 4;
  b_32 s = src;

  * -- d = (char) s & 0xFF;
  * -- d = (char) (s >>= 8) & 0xFF;
  * -- d = (char) (s >>= 8) & 0xFF;
  * -- d = (char) (s >> 8) & 0xFF;
}

void set_16(bit_16 destp, i_16 src)
{
  b_8 * d = (b_8 *) destp + 2;
  b_16 s = src;
  * -- d = (char) s & 0xFF;
  * -- d = (char) (s >>= 8) & 0xFF;
}

void    set_8( bit_8 destp, i_8 src )
{
  *destp = (char) src;
}



i_32 get_32(bit_32 src)
{
  i_32 v;

  v = (i_32) ((src[0])&0xFF) << 24 | ((src[1])&0xFF) << 16
    | ((src[2])&0xFF) << 8 | ((src[3])&0xFF);
  return v;
}

i_16 get_16(bit_16 src)
{
  i_16 v;
	
  v = (i_16) ( (src[0]) << 8 | (src[1]) );
  return v;
}

i_8 get_8(bit_8 src)
{
  return (i_8) *src;
}

b_32 get_u32(ubit_32 src)
{
  b_32 v;

  v = (i_32) ((src[0])&0xFF) << 24 | ((src[1])&0xFF) << 16
    | ((src[2])&0xFF) << 8 | ((src[3])&0xFF);
  return v;
}

b_16 get_u16(ubit_16 src)
{
  b_16 v;
	
  v = (b_16) ( (src[0]) << 8 | (src[1]) );
  return v;
}

b_8 get_u8(ubit_8 src)
{
  return (b_8) *src;
}
