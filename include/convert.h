#ifndef	_CONVERT_H_
#define	_CONVERT_H_

#include "saturn.h"


extern	void	set_8( bit_8, i_8);
extern	void	set_16(bit_16, i_16);
extern	void	set_32(bit_32, i_32 );

extern	i_8	get_8(bit_8);
extern	i_16	get_16(bit_16);
extern	i_32	get_32(bit_32);
extern	b_8	get_u8(ubit_8);
extern	b_16	get_u16(ubit_16);
extern	b_32	get_u32(ubit_32);
#endif
