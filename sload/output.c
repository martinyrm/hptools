#ifdef dll
#include "dllfile.h"
#endif
#include "sload.h"
#include "debug.h"

#include "output.h"
#include "symb.h"
#include "code.h"



i_16 out_position = -1;
struct {			/* symbol block buffer */
    char	Symb[4];
    O_SYMBOL	rec[REFPERBLK];
} out = { {'S','y','m','b'}};

void write_header(void)
{
    char buffer[LINE_LEN+1];
    OBJHEAD headerout;
    b_16 out_length = 1;	/* 1 block for header */
    b_16 out_symbols = symbols, out_fillrefs = fillrefs;
    b_16 temp = 0;
    b_32 codelen;

    if (codeonly) {
	/* Don't write symbols if -h or -H specified */
	return;
    }

    if (code_option & OPTION_CODE) {
	/* add blocks for code (512 nibbles per block) */
	out_length += (b_16) (((codelen=next_addr - start_addr) + 511) >> 9) & 0xFFFF;
    } else codelen=0;
    if (code_option & OPTION_SYMBOLS) {
	/* symbols and references are to be written */
	/* add blocks for symbol records */
	temp += symbols;
    } else out_symbols = 0;
    if (code_option & OPTION_REFS) {
	/* symbols and references are to be written */
	/* add blocks for symbol records */
	temp += fillrefs;
    } else out_fillrefs = 0;
    out_length += (temp+SYMBS_PER_RECORD-1) / SYMBS_PER_RECORD;

    (void) memcpy(headerout.id, SAT_ID, sizeof(headerout.id));
    set_16((char *)&headerout.length, out_length);
    set_32((char *)&headerout.nibbles, (i_32) codelen);
    set_16((char *)&headerout.symbols, out_symbols);
    set_16((char *)&headerout.references, out_fillrefs);
    set_32((char *)&headerout.start, start_addr);
    set_8((char *)&headerout.absolute, (b_8) (! relocatable));
    set_8((char *)&headerout.filler, (b_8) 0);
    (void) sprintf(buffer, "%-26s", date);
    (void) memcpy(headerout.date, buffer, sizeof(headerout.date));
    (void) sprintf(buffer, "%-40s", title);
    (void) memcpy(headerout.title, buffer, sizeof(headerout.title));
    (void) memset(headerout.softkeys, ' ', sizeof(headerout.softkeys));
    (void) sprintf(buffer, "%-26s", version);
    (void) memcpy(headerout.version, buffer, sizeof(headerout.version));
    set_32((char *)headerout.romid, IDvalue);
    (void) memset(headerout.reserved, '\0', sizeof(headerout.reserved));
    rewind(objfile);
    if (fwrite((char *) &headerout, 256, 1, objfile) != 1)
	error("WR_HD: error writing to %s", objname);
}

O_SYMBOLPTR next_obj(void)
{
    if (++out_position >= REFPERBLK) {
	if (fwrite(&out, sizeof(out), 1, objfile) != 1)
	    error("NX_OB: error writing to %s",objname);
	out_position = 0;
    }

/*
    if (out_position == 0) {
	(void) memcpy(out.Symb, "Symb", 4);
    }
*/

    return &out.rec[out_position];
}

void flushobject(void)
{
    if (out_position >= 0)
	if (fwrite(&out, sizeof(out), 1, objfile) != 1)
	    error("FL_OB: error writing to %s",objname);
}

void func_symbol(SYMBOLPTR sym_ptr)
{
    O_SYMBOLPTR symref= (O_SYMBOLPTR) next_obj();
    O_FILLREFPTR fill_ref;
    FILLREFPTR fillptr = sym_ptr->s_fillref;
    b_16 fillcount;
    i_32 value;
    int i,j;

    /* Copy the name field (blank filled) */
    j = strlen(sym_ptr->s_name);
    for (i=0; i < j; ++i)
	symref->os_name[i] = sym_ptr->s_name[i];
    while (i < SYMBOL_LEN)
	symref->os_name[i++] = ' ';

    fillcount = sym_ptr->s_fillcount;
    value = sym_ptr->s_value;
    if (fillcount & S_RELOCATABLE)
	value -= start_addr;

    if ((code_option & OPTION_REFS) == 0) {	/* allow refs to be killed */
	fillcount &= ~ (i_16) OS_REFERENCEMASK;
	fillptr = (FILLREFPTR) NULL;
    }

    set_32((char *)&symref->os_value, value);
    set_16((char *)&symref->os_fillcount, fillcount);
    fillcount &= OS_REFERENCEMASK;

    /* Now process fill references */
    while (fillptr != (FILLREFPTR) NULL) {
	fill_ref = (O_FILLREFPTR) next_obj();
	set_32((char *)&fill_ref->of_address, (i_32) (fillptr->f_address - start_addr));
	set_32((char *)&fill_ref->of_bias, fillptr->f_bias);
	set_8((char *)&fill_ref->of_class, fillptr->f_class);
	set_8((char *)fill_ref->of_subclass, fillptr->f_subclass);
	set_16((char *)fill_ref->of_length, fillptr->f_length);
	fillptr = fillptr->f_next;
	--fillcount;
    }
    if (fillcount != 0)
	error("WR_SY: reference count != list length:  %s",sym_ptr->s_name);
}

void write_symbols(void)
{
    if (codeonly) {
	/* Don't write symbols if -h or -H specified */
	return;
    }

    allsymbols(func_symbol);
    flushobject();
}

void write_output(void)
{
  if ((objfile=openfile(objname, OUTPUT_ENV, OUTPUT_PATH, BIN_WRITE, NULL))
      == (FILE *) NULL)
    error("WR_OU: unable to open %s as object file", objname);
  DBG2("Object file name set to '%s'", objname);
  
  if (code_option == OPTION_UNDEFINED)
    code_option = OPTION_RELOCATABLE;
  
  /* write the object file now */
  write_header();	/* write the header */
  
  /* Write the code record(s) */
  if (code_option & OPTION_CODE)
    writecode(start_addr, (i_32) (next_addr - start_addr));
  
  /* Write the symbol records */
  if (code_option & OPTION_SYMBOLS)
    write_symbols();
}
