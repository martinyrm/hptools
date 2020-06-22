#ifndef _OUTPUT_H_
#define _OUTPUT_H_


#define REFPERBLK SYMBS_PER_RECORD		/* number of ref/syms per block */

extern i_16 out_position;

extern void write_header(void);
extern O_SYMBOLPTR next_obj(void);
#define flushobject outputflushobject
extern void flushobject(void);
extern void func_symbol(SYMBOLPTR);
extern void write_symbols(void);
extern void write_output(void);

#endif
