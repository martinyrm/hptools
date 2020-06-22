#include "sasm.h"
#include "debug.h"
#include <time.h>

#include "envfile.h"
#include "symbols.h"
#include "opcode.h"
#include "expr.h"
#include "version.h"

#ifdef dll
#include "dllfile.h"
#endif

/* algorithm:
 *
 * (TBA)
 *
 */

/********** declarations for this module **********/

union   {
  char    buffer[256];
  struct  SATURN saturn;
} headerout;

i_32 nibble_offset;
i_32 out_offset;
i_32 out_position;
char out[256];

char listversion[] = VERSTRING;

i_32 currentline = 0, mainline;
int column, ccolumn = COMMENT_COLUMN;
int stdinread, linelength;
int singlefile;
i_32 first_error, last_error, first_warning, last_warning, warnings = TRUE;
int no_bin = FALSE;
i_32 abase = 0, errorcount, warningcount, cum_errors = 0;
int pagelength = 0xFFFFFF, lineofpage, page, pagewidth = 0;
int listing = TRUE, pass1_listing = FALSE, listflags, listthisline, listaddress;
i_32 listcount;
int assembling;
int blanks_in_expr = FALSE;
int errors_to_stderr = FALSE;
int extended_errors = FALSE;
int codeonly = FALSE, hexcode = FALSE;
int addingmacro;
int reading, begin_pseudos_ok; /* END sets reading == FALSE */
int rel_expressions_ok = FALSE, quote_ok = TRUE;
int linewidth = 5, codewidth = 5, source_col;
int code_level = LEVEL2;
int force_global = FALSE;
int pass1, pass2;
int verbose = FALSE;
unsigned char flag[NUMBER_OF_FLAGS];
int absolute;	/* set TRUE by ABS */
int have_start_addr = FALSE;	/* set TRUE by -S option */
i_32 pc, codestart=0; /* ABS, REL set pc=codestart=nnnnn */
i_32 pc_offset;
b_16 ext_symbols, ext_fillrefs;
char line[LINE_LEN+1], label[LINE_LEN+1], mnemonic[LINE_LEN+1];
char title[41], subtitle[41], date[26];
char prog_symbol[SYMBOL_LEN+1];
unsigned char charset[256];
char basefmt[LINE_LEN+2],addrfmt[20],inclfmt[10],codefmt[25],lastcodefmt[15];


/* Compatibility with the previous GNU Tools */
i_32	loops[LOOP_STACK_MAX][3];
int	insideloop = FALSE;
int	label_index_m2, label_index_m1, label_index_p1, label_index_p2;

/* Compatibility with MASD */
int	index_c1, index_c2, index_c3, level_stack;
MSTACK	masd_stack[MAX_STACK];

IMACRO imstack[IMSTACKSIZE];
int imptr, impending, imtype;

char *pptr[IMSTACKSIZE][9]; /* parameter pointers for macros */

char *inname, *inbase;
char *listname, *listprefix, *listsuffix = ".l";
int listdefault = TRUE;

int combos = TRUE, masd = TRUE, sasm_only = FALSE;

char *opname;
char *objname, *objbase, *objprefix, *objsuffix = ".o";
int objdefault = TRUE;
char *msgname;
int msg_append = FALSE;
char *curr_name;
FILE *include, *temp, *infile, *listfile, *opfile, *objfile;
char *ptr, *linestart, *progname;
char ErrorString[1000];

MACROPTR macroptr; /* Pointer to the current macro line when macro active */

#ifndef dll
int debug = FALSE;	/* always defined because it is tested in places */
#endif

char	*dbgname;
FILE	*dbgfile;


/********** External declarations **********/



#ifndef __MSDOS__
#ifndef __STDC__
extern void *memset();
#endif  /* __STDC__ */
#endif  /* MSDOS */

/* Stuff in opcode.c */

/********** Start of code in this module **********/


void putlist(char *lineout)
{
  char	*buffer;
  int length = strlen(lineout) - 1;

  if ( (buffer = (char *)calloc(1,length + 2)) == NULL)
    error("error getting memory for %s in putlist", lineout);
  strcpy(buffer, lineout);
  if (++lineofpage > pagelength)
    doheader();
  
  if (pagewidth && length >= pagewidth)
    length = pagewidth-1;
  
  while (length >= 0 && buffer[length] == ' ')
    --length;
  buffer[++length] = '\0';
  
  if (fputs(buffer, listfile) < 0 || fputc('\n', listfile) < 0)
    error("error writing to %s",  listname);
  free(buffer);
}


void doheader()
{
  char buffer[LINE_LEN+1];

  if (listing) {
    lineofpage = 1;
    (void) sprintf(buffer, "%cSaturn Assembler    %-34.34s %s", FF,
		   title, date);
    putlist(buffer);
    (void) sprintf(buffer, "%-21.21s %-34.34s %-14.14s Page %4d",
		   listversion + 21, subtitle, inbase, ++page);
    putlist(buffer);
    putlist("");
  }
}



void sasmusage()
{
  (void) fprintf(stderr,
		 "Usage: %s [options] ... [sourcefile] ...\n", progname);
  (void) fprintf(stderr, "options: (use -v? for full list)\n");
  (void) fprintf(stderr,
		 "-A		write an annotated listing to stdout [default for -s]\n");
  (void) fprintf(stderr,
		 "-a listfile	write an annotated listing to listfile\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-b		blanks OK within expressions [default = end of expression]\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-B		Disallow binary digits in expressions\n");
  (void) fprintf(stderr,
		 "-c column	fields starting after column 'column' are ignored\n");
  (void) fprintf(stderr,
		 "-D symbol[=value]	define a symbol [default value = 1]\n");
#ifdef	DEBUG
  if (verbose)
    (void) fprintf(stderr,
		   "-d debugfile	debug mode, specified output file\n");
#endif	/*DEBUG*/
  (void) fprintf(stderr,
		 "-E		write C-like error messages to stderr\n");
  (void) fprintf(stderr,
		 "-e		write error messages to stderr\n");
  (void) fprintf(stderr,
		 "-f flaglist	set assembly flags selected by flaglist (comma-separated)\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-G		force all entry points to be global (overrides SASM_GLOBAL=0)\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-g		cancels forcing globals (overrides SASM_GLOBAL=1)\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-H		suppress object file header and symbol (write raw code)\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-h		suppress object file header and symbol (write hex code)\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-L		create a pass 1 listing if listing is enabled\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-M msgfile	redirect messages to msgfile (append to msgfile)\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-m msgfile	redirect messages to msgfile (overwrite msgfile)\n");
  (void) fprintf(stderr,
		 "-N		do not produce an annotated listing\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-n nibls	put 'nibls' nibbles of code per list line [default=%d]\n",
		   codewidth);
  (void) fprintf(stderr,
		 "-o objfile	write the object file to objfile [default=<name>.o]\n");
  (void) fprintf(stderr,
		 "-P codetype	set processor (0=1LF2, 1=1LK7, 2=1LR2 [default=%d])\n",
		 code_level / LEVEL1);
  (void) fprintf(stderr,
		 "-p pagelen	do a page break each 'pagelen' lines [default=%d]\n",
		 pagelength);
  if (verbose)
    (void) fprintf(stderr,
		   "-q		allow quote ['] as ASCII text delimiter [default unless -s]\n");
  (void) fprintf(stderr,
		 "-s		force SASM compatibility mode\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-S nnnnn	override start address to hex nnnnn\n");
#ifdef	OK_OPTABLE
  if (verbose)
    (void) fprintf(stderr,
		   "-t optable	select another opcode file [default=%s]\n", opname);
#endif	/*OK_OPTABLE*/
  (void) fprintf(stderr,
		 "-v		verbose mode (print a message when starting a new file)\n");
  (void) fprintf(stderr,
		 "-w width	set output page width to 'width' columns [default=80]\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-W		warnings off\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-x		allow expressions as operand of REL(n) and GOTO\n");
  if (verbose)
    (void) fprintf(stderr,
		   "-z		disallow MASD instruction set\n");
}

void errmsg(char *message)
{
  char msgbuffer[LINE_LEN+1];

  if (listing && (pass2 || pass1_listing)) {
    (void) sprintf(msgbuffer, "*** ERROR: %s ***", message);
    putlist(msgbuffer);
    listthisline = TRUE;
  }
  if (pass2) {
    if (errors_to_stderr) {
      if (extended_errors == TRUE) {
	/* '"<filename>", line <lineno>: <error message>' */
	if (fprintf(stderr, "\"%s\", line %ld: %s\n",
		    curr_name,
		    currentline != 0 ? (long) currentline
		    : (long) mainline,
		    message) < 0)
	  error("%s",  "error writing to stderr");
      } else {
	/* '*** ERROR on line <lineno>: <error message> ***' */
	if (fprintf(stderr, "%s ERROR *** %s ""%s""\n",
			ErrorString,
		    message, linestart) < 0)
	  error("%s",  "error writing to stderr");
      }
    }
    if (last_error != currentline && errorcount++ == 0)
      first_error = mainline;
    last_error = mainline;
  }
}

void warningmsg(char *message)
{
  char msgbuffer[LINE_LEN+1];
  if (!warnings)
    return;
  
  if (listing && (pass2 || pass1_listing)) {
    (void) sprintf(msgbuffer, "*** WARNING: %s ***", message);
    putlist(msgbuffer);
    listthisline = TRUE;
  }
  if (pass2) {
    if (errors_to_stderr) {
      if (extended_errors == TRUE) {
	/* '"<filename>", line <lineno>: <warning message>' */
	if (fprintf(stderr, "\"%s\", line %ld: %s\n",
		    curr_name,
		    currentline != 0 ? (long) currentline
		    : (long) mainline,
		    message) < 0)
	  error("%s", "error writing to stderr");
      } else {
	/* '*** WARNING on line <lineno>: <error message> ***' */
	if (fprintf(stderr, "%s WARNING ***\n%s ""%s""\n",
			ErrorString,
		    message, linestart) < 0)
	  error("%s", "error writing to stderr");
      }
    }
    if (last_warning != currentline && warningcount++ == 0)
      first_warning = mainline;
    last_warning = mainline;
  }
}

void sasmputnib(int value)
{
  i_32 offset;
  i_16 shift;

  value &= 0xF;
  if (hexcode) {
    if (fputc("0123456789ABCDEF"[value], objfile) == EOF)
      error("error writing to %s", objname);
    return;
  }
  while (nibble_offset >= (out_offset + 512)) {
    if (fwrite(out, 256, 1, objfile) != 1)
      error("error writing to %s", objname);
    out_offset += 512;
  }
  if (nibble_offset >= out_offset && nibble_offset < MAX_NIBBLES) {
    offset = nibble_offset++ - out_offset;
    if (codeonly)	/* if true, must be -H (hexcode returned above) */
      /* Put nibbles in most reasonable order for binary output */
      shift = (offset & 1) ? 4 : 0;
    else
      /* Put nibbles in order required for Saturn type file */
      shift = (offset & 1) ? 0 : 4;
    offset >>= 1;
    out[offset] &= 0xF << (4 - shift);
    out[offset] += value << shift;
  } else {
    if (nibble_offset >= MAX_NIBBLES)
      error("too many nibbles in output file %s", objname);
    else
      error("can't back up %s", objname);
  }
}

void flushnibs()
{
  if (codeonly == FALSE) {
    while (nibble_offset % 512 != 0) /* putnib() increments nibble_offset */
      sasmputnib(0);
  } else {
    /* only fill out to a byte boundary */
    if (hexcode)	/* Nothing to flush for hexcode */
      return;
    if (nibble_offset % 2)
      sasmputnib(0);
  }
  /* now actually write the data to the file */
  if (nibble_offset > out_offset &&
      fwrite(out, (int)((nibble_offset-out_offset)/2), 1, objfile) != 1)
    error("error writing to %s",objname);
}

void writeheader()
{
  char buffer[LINE_LEN+1];

  if (codeonly == TRUE)
    return;
  (void) strncpy(headerout.saturn.id, SAT_ID, sizeof(SAT_ID));
  set_16((char *)&headerout.saturn.length,
	 (b_16) (
		 /* one block for the Saturn header itself */
		 1
		 /* 'n' blocks of code (512 nibbles per block) */
		 + ( ((get_u32(headerout.saturn.nibbles)+511) >> 9) & 0xFFFF )
		 /* 'm' blocks for symbol records */
		 + (ext_symbols+ext_fillrefs+SYMBS_PER_RECORD-1) / SYMBS_PER_RECORD
		 )
	 );
  set_32((char *)&headerout.saturn.nibbles, (i_32) (pc+pc_offset-codestart));
  set_16((char *)&headerout.saturn.symbols, (b_16) ext_symbols);
  set_16((char *)&headerout.saturn.references, (b_16) ext_fillrefs);
  set_32((char *)&headerout.saturn.start, codestart);
  set_8((char *)&headerout.saturn.absolute, (b_8) absolute);
  set_8((char *)&headerout.saturn.filler, (b_8) 0);
  (void) sprintf(buffer, "%-26s", date);
  (void) strncpy(headerout.saturn.date, buffer, 26);
  (void) sprintf(buffer, "%-40s", title);
  (void) strncpy(headerout.saturn.title, buffer, 40);
  (void) strncpy(headerout.saturn.softkeys, "                    ", 20);
  (void) sprintf(buffer, "%-26s", version);
  (void) strncpy(headerout.saturn.version, buffer, 26);
  set_32((char *)&headerout.saturn.romid, (b_32) 0);
  rewind(objfile);
  if (fwrite(headerout.buffer, 256, 1, objfile) != 1)
    error("error writing to %s", objname);
}

void putobject(union SFC *sym_rec)
{
  int i;

  if (out_position == 0) {
    (void) strncpy(out, "Symb", 4);
    out_position = 4;
  }

  for (i = 0; i < SYMRECLEN; ++i)
    out[out_position + i] = sym_rec->c[i];
  out_position += SYMRECLEN;

  if (out_position + SYMRECLEN > 256) {
    if (fwrite(out, 256, 1, objfile) != 1)
      error("error writing to %s",objname);
    out_position = 0;
  }
}

void flushobject()
{
  if (out_position > 4)
    if (fwrite(out, 256, 1, objfile) != 1)
      error("error writing to %s",objname);
}

void dosymbol(SYMBOLPTR sym_ptr)
{
  O_SYMBOL symref;
  O_FILLREF fill_ref;
  FILLREFPTR fillptr;
  b_8 flags;
  b_16 fillcount;
  i_32 value;
  int i;

  fillptr = sym_ptr->s_fillref;
  flags = sym_ptr->s_flags;
  if (!((S_RESOLVED & flags) && (S_ENTRY & flags))
      && fillptr == (FILLREFPTR) NULL)
    return;
  for (i=0; i < SYMBOL_LEN && sym_ptr->s_name[i]; ++i)
    symref.os_name[i] = sym_ptr->s_name[i];
  while (i < SYMBOL_LEN)
    symref.os_name[i++] = ' ';
  fillcount = sym_ptr->s_fillcount & OS_REFERENCEMASK;
  value = sym_ptr->s_value;
  if (flags & S_RESOLVED && !(flags & S_RDSYMB)) {
    fillcount |= OS_RESOLVED;
    if (flags & S_RELOCATABLE) {
      fillcount |= OS_RELOCATABLE;
      value -= codestart;
    }
  }
  set_32((char *)&symref.os_value, value);
  set_16((char *)&symref.os_fillcount, (b_16) fillcount);
  putobject((union SFC *)&symref);
  ++ext_symbols;
  while (fillptr != (FILLREFPTR) NULL) {
    set_8((char *)&fill_ref.of_class, (b_8) fillptr->f_class);
    set_8((char *)&fill_ref.of_subclass, (b_8) fillptr->f_subclass);
    set_32((char *)&fill_ref.of_address, (i_32) (fillptr->f_address - codestart));
    set_32((char *)&fill_ref.of_bias, fillptr->f_bias);
    set_16((char *)&fill_ref.of_length, fillptr->f_length);
    putobject((union SFC *)&fill_ref);
    ++ext_fillrefs;
    fillptr = fillptr->f_next;
  }
}

void writesymbols()
{
  if (codeonly == TRUE)
    return;
  /* objects in sys_root do not get written to the object file */
  allsymbols(dosymbol, s_root->s_right);
  flushobject();
}

void writeopcode2(b_32 opcode_len, char *opcode_nibs)
{
  int n;
  opcode_len /= 2;
  while (opcode_len-- > 0) {
    n = *opcode_nibs & 0xF;
    sasmputnib(n);
    n = (*opcode_nibs >> 4) & 0xF;
    sasmputnib(n);
    opcode_nibs++;
  }
  begin_pseudos_ok = FALSE;
}

void writeopcode(b_32 opcode_len, char *opcode_nibs)
{
  int n;

  while (opcode_len-- > 0) {
    if ((n = *opcode_nibs)) {
      ++opcode_nibs;
      n = isdigit(n) ? n - '0' : n - 'A' + 10;
    }
    sasmputnib(n);
  }

  begin_pseudos_ok = FALSE;
}

int iscomment()
{
	int i;
	if (ptr[0]=='*' && ptr[1]==' ' && ptr[2]=='F' && ptr[3]=='i' && ptr[4]=='l' && ptr[5]=='e' && ptr[6]=='\t')
	{
		i=7;
		while (ptr[i]!='\t' && ptr[i]!='\0')
		{
			ErrorString[i-7]=ptr[i];
			i++;
		}
		if (ptr[i]!=0)
		{
			ErrorString[i-7]=':';
			ErrorString[i-6]=' ';
			while (ptr[i]!='\0')
			{
				ErrorString[i-5]=ptr[i];
				i++;
			}
			ErrorString[i-5]=':';
			ErrorString[i-4]='\0';
		}

	}
  return (*ptr == '*' || *ptr == '\0');
}

int isfield()
{
  return ((column < ccolumn) && *ptr);
}

int getfield2(char *fieldp, int maxlength, char *separators)	/* list of valid string terminators */
{
  char *tmp;
  int length = 0;

  if (isfield()) {  /* check for valid start of field*/
    if (strchr(separators,*ptr))
      ptr++;
    while(*ptr && strchr(" ",*ptr))
      ptr++;
    tmp = ptr;
    if ((ptr = strpbrk(ptr, separators)) == (char *) NULL)
      ptr = &linestart[linelength];
    column += (length = ptr - tmp);
    
    if (length > maxlength) {
      errmsg("Field too long");
      length = maxlength;
    }
    
    (void) strncpy(fieldp, tmp, length);
  }
  fieldp[length] = '\0';
  return (length != 0);
}

int getfield(char *fieldp, int maxlength, char *separators)	/* list of valid string terminators */
{
  char *tmp;
  int length = 0;

  if (isfield()) {  /* check for valid start of field*/
    if (strchr(separators, *ptr) == (char *) NULL) { /* NOT a separator */
      tmp = ptr;
      if ((ptr = strpbrk(ptr, separators)) == (char *) NULL)
	ptr = &linestart[linelength];
      column += (length = ptr - tmp);

      if (length > maxlength) {
	errmsg("Field too long");
	length = maxlength;
      }

      (void) strncpy(fieldp, tmp, length);
    }
  }
  fieldp[length] = '\0';
  return (length != 0);
}

void skipwhite()
{
  char c;

  while ((c = *ptr) == ' ' || c == '\t') {
    ++ptr;
    if (c == ' ')
      ++column;
    else
      column += 8 - (column - 1) % 8;
  }
}

char *sasmgetline(char *lineptr, FILE *file)
{
  char *pt;

  if (fgets(lineptr, LINE_LEN, file) == (char *) NULL)
    return (char *) NULL;
  /* Check if the line is longer than LINE_LEN */
  if (strlen(lineptr) == LINE_LEN-1 && lineptr[LINE_LEN-1] != '\n') {
    char tempbuf[LINE_LEN];

    /* This line is too long */
    errmsg("Input line too long (extra characters ignored)");
    do {
      pt = fgets(tempbuf, LINE_LEN, file);
    } while (pt != (char *) NULL && strlen(tempbuf) == LINE_LEN-1
	     && tempbuf[LINE_LEN-1] != '\n');
  }
  if ((pt=strchr(lineptr, '\n')) != (char *) NULL) /* '\n' -> end of string */
    *pt = '\0';
  /* Check for a CR at the end of the line */
  if ((pt=strrchr(lineptr, '\r')) != (char *) NULL && pt[1] == '\0')
    *pt = '\0';
  if (*lineptr == '\032')	/* ^Z at start of line */
    *lineptr = '\0';	/* Ignore this line */
  return lineptr;
}

void copyline(char *outline, char *inpline, int length)
{
  int col = 0;
  char c;
  static char blank[] = "        ";

  while ((c = *inpline++) && col < length) {
    if (c != '\t') {
      /* regular character */
      *outline++ = c;
      ++col;
    } else {
      /* found a tab */
      (void) strcpy(outline, &blank[col % 8]);
      outline += 8 - col % 8;
      col += 8 - col % 8;
    }
  }
  *outline = '\0';
}


void putsymbolref(SYMBOLPTR sym_ptr)
{
  char linebuf[LINE_LEN+1];
  int col;
  LINEREFPTR lineptr = sym_ptr->s_lineref;
  int pwidth = (pagewidth!=0) ? pagewidth : 80;

  if (debug == FALSE) {
    if (lineptr == (LINEREFPTR) NULL)
      return;

    /* Unreferenced RDSYMB values */
    if (sym_ptr->s_flags & S_RESOLVED && sym_ptr->s_flags & S_RDSYMB
	&& lineptr->l_next == (LINEREFPTR) NULL)
      return;

    /* Unreferenced command-line definitions */
    if (lineptr->l_line == 0 && lineptr->l_next == (LINEREFPTR) NULL)
      return;
  }
  (void) sprintf(linebuf, "%c%-32.32s  ",
		 sym_ptr->s_flags & S_ENTRY ? '=' : ' ',
		 sym_ptr->s_name);
  if (sym_ptr->s_flags & S_RESOLVED)
    (void) sprintf(&linebuf[35], "%3.3s%8ld #%8.8lX -",
		   (sym_ptr->s_flags & S_RELOCATABLE) ? "Rel" : "Abs",
		   (long) sym_ptr->s_value,
		   (long) sym_ptr->s_value);
  else
    (void) sprintf(&linebuf[35], "%s",
		   (sym_ptr->s_flags & S_EXTERNAL)
		   ? "Ext                   -" : "    ******* ********* -");
  col = strlen(linebuf);
  while (lineptr != (LINEREFPTR) NULL) {
    /* Note that if the line number requires more than 5 digits, this */
    /* test may allow a pagewidth overflow (1 or 2 characters max) */
    if (col + 6 > pwidth) {
      putlist(linebuf);
      col = sprintf(linebuf, "%38s", "");
    }
    col += sprintf(&linebuf[col], " %5ld", lineptr->l_line);
    lineptr = lineptr->l_next;
  }
  putlist(linebuf);
}

void listsymbols()
{

  lineofpage = pagelength;	/* set up to do header before first line */
  (void) strcpy(subtitle, "Symbol Table");
  allsymbols(putsymbolref, s_root->s_right);
  if (debug) {
    lineofpage = pagelength; /* set up to do header before first line */
    (void) strcpy(subtitle, "Symbols Defined on Command Line");
    allsymbols(putsymbolref, sys_root->s_right);
  }
}

void liststatistics()
{
  int i, len, pwidth = (pagewidth!=0)?pagewidth:80;

  (void) strcpy(subtitle, "Statistics");
  doheader();
  putlist("Input Parameters\n");
  (void) sprintf(line, "  Source file name is %s\n", inname);
  putlist(line);

  (void) sprintf(line, "  Listing file name is %s\n", listname);
  putlist(line);

  (void) sprintf(line, "  Object file name is %s\n", objname);
  putlist(line);

  /* Show initial flag settings */
  (void) sprintf(line, "  Flags set on command line");
  len = -1;
  for (i=0; i<NUMBER_OF_FLAGS; ++i) {
    if (flag[i] & FLAG_CMDLINE) {
      /* See if room for this flag (and possible ',' after it) */
      if ((len + 3 + (i > 9)) >= pwidth) {
	line[len++] = ',';
	line[len] = '\0';
	len = -1;
      }
      if (len < 0) {
	putlist(line);
	(void) sprintf(line, "    %d", i);
      } else
	(void) sprintf(&line[len], ", %d", i);
      len = strlen(line);
    }
  }
  putlist(line);
  if (len < 0)
    putlist(         "    None");

  if (debug) {
    /* Show final flag settings */
    (void) sprintf(line, "\n  Flags set when assembly completed");
    len = -1;
    for (i=0; i<NUMBER_OF_FLAGS; ++i) {
      if (flag[i] & FLAG_SET) {
	/* See if room for this flag (and possible ',' after it) */
	if ((len + 3 + (i > 9)) >= pwidth) {
	  line[len++] = ',';
	  line[len] = '\0';
	  len = -1;
	}
	if (len < 0) {
	  putlist(line);
	  (void) sprintf(line, "    %d", i);
	} else
	  (void) sprintf(&line[len], ", %d", i);
	len = strlen(line);
      }
    }
    putlist(line);
    if (len < 0)
      putlist(         "    None");
  }
  putlist("\nWarnings:\n");

  if (warningcount <= 1) {
    if (warningcount == 0)
      (void) strcpy(line,  "  None");
    else
      (void) sprintf(line, "  1 warning (line %ld)", (long) first_warning);
  } else {
    (void) sprintf(line, "  %ld warnings (first %ld/last %ld)",
		   (long) warningcount, (long) first_warning, (long) last_warning);
  }
  putlist(line);

  putlist("\nErrors:\n");

  if (errorcount <= 1) {
    if (errorcount == 0)
      (void) strcpy(line,  "  None");
    else
      (void) sprintf(line, "  1 error (line %ld)", (long) first_error);
  } else {
    (void) sprintf(line, "  %ld errors (first %ld/last %ld)",
		   (long) errorcount, (long) first_error, (long) last_error);
  }
  putlist(line);
}

void definesymbol(char *symbol)
{
  char *cp;
  char symbname[SYMBOL_LEN+1];
  long value = 1;	/* default value is one */
  int c = S_RESOLVED | S_SETPASS2;
  int len;

  if (*symbol == ':')
    ++symbol;
  else if (*symbol == '=') {
    ++symbol;
    c |= S_ENTRY;
  }
  if ((cp=strrchr(symbol, '=')) != (char *) NULL)
    len = cp - symbol;
  else
    len = strlen(symbol);

  if (len > SYMBOL_LEN)
    len = SYMBOL_LEN;

  (void) strncpy(symbname, symbol, len);
  symbname[len] = '\0';	/* terminate the name with a null */
  /* Figure out value, if any given */
  if (cp!=(char *) NULL && cp[1]!='\0' && sscanf(cp+1, "%ld", &value) != 1) {
    (void) fprintf(stderr, "Non-numeric expression for -D (%s)\n", cp + 1);
    ++errorcount;
    return;
  }
  (void) sys_addsymbol(symbname, (i_32) value, c);
}

void sasmget_options(int argc, char **argv)
{
  int i;
  char *cp;
  
  extern char *optarg;
  extern int optind, opterr;
  
  while ((i=getopt(argc,argv,"Aa:bBc:D:d:Eef:GgHhLM:m:Nn:o:P:p:qS:st:vw:Wxz"))!=EOF)
    switch (i) {
    case 'A':	/* Enable listing to [stdout] */
      listing = TRUE;
      setname(&listname, "-");
      listdefault = FALSE;
      break;
    case 'a':	/* Enable listing to a listfile */
      listing = TRUE;
      setname(&listname, optarg);
      /* A listfile name starting with "." is a default suffix */
      /* If the name is a path whose basename starts with '.', */
      /* set the prefix and suffix from the path.              */
      /* A basename containing DIRC is NOT a default suffix.   */
      if ((cp=strrchr(listname, DIRC)) != (char *) NULL)
	++cp;
      else
	cp = listname;
      /* cp points to the start of the basename now */
      if (listprefix != (char *) NULL) {
	free(listprefix);
	DBG2("Address %ld: free'd memory", (long) listprefix)
	  }
      if (cp[0] == '.' && strchr(cp, DIRC) == (char *) NULL) {
	listdefault = TRUE;
	listsuffix = cp;
	if (cp != listname) {
	  listprefix = (char *) calloc(1, (unsigned) (cp - listname + 1));
	  if (listprefix == (char *) NULL)
	    error("error getting memory for %s", listname);
	  DBG3("Address %ld: allocated %d bytes", (long) listprefix,
	       cp - listname + 1)
	    (void) strncpy(listprefix, listname, cp - listname);
	}
      } else {
	listdefault = FALSE;
      }
      break;
    case 'b':	/* Blanks OK in an expression */
      blanks_in_expr = TRUE;
      break;
    case 'B':
      no_bin = TRUE;
      break;
    case 'c':	/* Set comment column */
      if (sscanf(optarg, "%d", &ccolumn) != 1) {
	(void) fprintf(stderr, "Non-numeric comment column (%s)\n",
		       optarg);
	++errorcount;
      }
      if (ccolumn < 0) {
	(void) fprintf(stderr,
		       "Comment column must be non-negative (%ld)\n",
		       (long) ccolumn);
	ccolumn = COMMENT_COLUMN;
	++errorcount;
      }
      if (ccolumn == 0)
	ccolumn = LINE_LEN+1;	/* "Infinite" */
      break;
    case 'D':	/* Define a symbol */
      (void) definesymbol(optarg);
      break;

    case 'd':	/* Select a debug file, enable debug mode */
      setname(&dbgname, optarg);
      if (dbgfile != (FILE *) NULL && dbgfile != stderr)
	(void) fclose(dbgfile);
      if (strcmp(dbgname, "-") == 0) {
	setname(&dbgname, "[stderr]");
	dbgfile = stderr;
      } else {
	if ((dbgfile = fopen(dbgname, TEXT_WRITE)) == (FILE *) NULL)
	  error("error creating %s for writing",dbgname);
      }
      /* Set to unbuffered output */
      setbuf(dbgfile, (char *) NULL);
      debug = TRUE;
      break;

    case 'E':	/* Enable C-like error listing */
      errors_to_stderr = TRUE;
      extended_errors = TRUE;
      break;
    case 'e':	/* Enable error listing */
      errors_to_stderr = TRUE;
      extended_errors = FALSE;
      break;
    case 'f':	/* Set flags */
      i = -1;
      while (i > -2)
	switch (*optarg) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  i = (i < 0) ? 0 : i * 10;
	  i += *optarg++ - '0';
	  break;
	case '\0':	/* End of optarg */
	case ' ': case ',': case ';': case ':':	/* terminator */
	  if (i >= 0 && i < NUMBER_OF_FLAGS) {
	    DBG2("Setting flag %d", i)
	      flag[i] = FLAG_CMDLINE;
	  } else {
	    if (i >= 0) {
	      (void) fprintf(stderr,
			     "Flag number out of range (%d)\n", i);
	      ++errorcount;
	    }
	  }
	  if (*optarg) {
	    ++optarg;
	    i = -1;
	  } else
	    i = -2;
	  break;
	default:
	  (void) fprintf(stderr, "Invalid flag digit (%c)\n",
			 *optarg++);
	  ++errorcount;
	  break;
	}
      break;
    case 'G':	/* Force all entry points global */
      force_global = TRUE;
      break;
    case 'g':	/* Do not force all entry points global */
      force_global = FALSE;
      break;
    case 'H':	/* Write only binary code to object file */
      codeonly = TRUE;
      hexcode = FALSE;
      break;
    case 'h':	/* Write (ASCII) hex code to object file */
      codeonly = TRUE;
      hexcode = TRUE;
      break;
    case 'L':	/* Pass 1 listing */
      pass1_listing = TRUE;
      break;
    case 'M':	/* Set message destination (append) */
      setname(&msgname, optarg);
      msg_append = TRUE;
      break;
    case 'm':	/* Set message destination (overwrite) */
      setname(&msgname, optarg);
      msg_append = FALSE;
      break;
    case 'N':	/* Disable listing */
      listing = FALSE;
      break;
    case 'n':	/* Nibbles of code to print per line */
      if (sscanf(optarg, "%d", &codewidth) != 1) {
	(void) fprintf(stderr,"Non-numeric code field width (%s)\n",
		       optarg);
	++errorcount;
      }
      if (codewidth <= 0) {
	(void) fprintf(stderr,
		       "Code field width must be greater than 0 (%ld invalid)\n",
		       (long) codewidth);
	codewidth = 5;
	++errorcount;
      }
      /*
       * (sizeof(basefmt) - 13) because the format of 'basefmt' requires
       *   codewidth <= (sizeof(basefmt) - 13)
       *
       * NOTE: allow extra space for possibility of expanding
       *       <linewidth> for a line # > 9999
       */
      if (codewidth > (sizeof(basefmt) - 13 - 2)) {
	(void) fprintf(stderr,
		       "Code field width must be less than %ld (%d invalid)\n",
		       (long) (sizeof(basefmt)-13-2), codewidth);
	codewidth = 5;
	++errorcount;
      }
      break;
    case 'o':	/* Set object file name */
      setname(&objname, optarg);
      /* A objfile name starting with "." is a default suffix */
      /* If the name is a path whose basename starts with '.', */
      /* set the prefix and suffix from the path.              */
      /* A basename containing DIRC is NOT a default suffix.   */
      if ((cp=strrchr(objname, DIRC)) != (char *) NULL)
	++cp;
      else
	cp = objname;
      /* cp points to the start of the basename now */
      if (objprefix != (char *) NULL) {
	free(objprefix);
	DBG2("Address %ld: free'd memory", (long) objprefix)
	  }
      if (cp[0] == '.' && strchr(cp, DIRC) == (char *) NULL) {
	objdefault = TRUE;
	objsuffix = cp;
	if (cp != objname) {
	  objprefix = (char *) calloc(1, (unsigned) (cp - objname + 1));
	  if (objprefix == (char *) NULL)
	    error("error getting memory for %s", objname);
	  DBG3("Address %ld: allocated %d bytes", (long) objprefix,
	       cp - objname + 1)
	    (void) strncpy(objprefix, objname, cp - objname);
	}
      } else {
	objdefault = FALSE;
      }
      break;
    case 'P':	/* Set processor type */
      if (*optarg >= '0' && *optarg <= '3' && optarg[1] == '\0')
	code_level = LEVEL1 * (*optarg - '0');
      else
	(void) fprintf(stderr, "Invalid code generation level (%s)\n",
		       optarg);
      break;
    case 'p':	/* Set page length */
      if (sscanf(optarg, "%d", &pagelength) != 1) {
	(void) fprintf(stderr, "Non-numeric page length (%s)\n",optarg);
	++errorcount;
	break;
      }
      if (pagelength < 4) {
	(void) fprintf(stderr, "Page length must be at least 4 (%d)\n",
		       pagelength);
	pagelength = 60;
	++errorcount;
      }
      break;
    case 'S':	/* Set start address */
      have_start_addr = TRUE;
      if (strspn(optarg,"0123456789abcdefABCDEF") != strlen(optarg) ||
	  sscanf(optarg, "%lx", &codestart) != 1 ||
	  codestart >= MAX_NIBBLES || codestart < 0) {
	codestart = 0;
	have_start_addr = FALSE;
	++errorcount;
	(void) fprintf(stderr, "Invalid start address (%s)\n",
		       optarg);
      }
      break;
    case 's':	/* Set SASM-compatible mode */
      blanks_in_expr = FALSE;
      ccolumn = COMMENT_COLUMN;
      codeonly = FALSE;
      extended_errors = FALSE;
      quote_ok = FALSE;
      rel_expressions_ok = FALSE;
      linewidth = 5;
      codewidth = 4;
      listing = TRUE;
      pass1_listing = FALSE;
      /* Default listing file is [stdout] */
      setname(&listname, "-");
      listdefault = FALSE;
      /* Default message file is [stderr] */
      setname(&msgname, "-");
      /* Default object file is "code" */
      setname(&objname, "code");
      objdefault = FALSE;
      verbose = FALSE;
      combos = FALSE;
      masd = FALSE;
      sasm_only = TRUE;
      warnings = FALSE;
      no_bin = TRUE;
      break;
    case 'q':	/* Allow quote for string constants */
      quote_ok = TRUE;
      break;
#ifdef OK_OPTABLE
    case 't':	/* Select an alternate opcode table */
      setname(&opname, optarg);
      break;
#endif
    case 'v':	/* Verbose mode */
      verbose = TRUE;
      break;
    case 'w':	/* Set page width */
      if (sscanf(optarg, "%d", &pagewidth) != 1) {
	(void) fprintf(stderr, "Non-numeric page width (%s)\n", optarg);
	++errorcount;
	break;
      }
      /* The minimum page width is set by the minumum width necessary */
      /* to print the symbol table with one line number (38 + 6) = 44 */
      if (pagewidth < 44) {
	(void) fprintf(stderr, "Page width must be at least 44 (%d)\n",
		       pagewidth);
	pagewidth = 0;
	++errorcount;
      }
      break;
    case 'W':
      warnings = FALSE;
      break;

    case 'x':	/* Allow relative expressions for REL(n), GOTO, GOSUB */
      rel_expressions_ok = TRUE;
      break;
    case 'z': /* Disallow MASD instruction set */
      masd = FALSE;
      break;
    case '?':	/* bad option */
    default:
      ++errorcount;
      break;
    }
}

void copy_sys(SYMBOLPTR symb)
{
  SYMBOLPTR newsymb;
  char name[SYMBOL_LEN+1];

  /* addsymbol() may alter the name field */
  (void) strncpy(name, symb->s_name, SYMBOL_LEN);
  name[SYMBOL_LEN] = '\0';
  newsymb = addsymbol(name, symb->s_value, symb->s_flags);
  addlineref(newsymb);
}

void dopptr()
{
  int i;
  char *cp;

  if (pptr[imptr][0] != (char *) NULL)	/* check if already initialized */
    return;
  cp = imstack[imptr].im_buffer;
  for (i=0; i<9 && cp != (char *) NULL; ++i) {
    if (*cp == '<' && strchr(cp, '>') != (char *) NULL) {
      /* First character is '<' AND a '>' appears later in the line */
      pptr[imptr][i] = &cp[1];
      cp = strchr(cp, '>');
      *cp++ = '\0';
      if ((cp=strchr(cp, ',')) != (char *) NULL)	/* Find next comma */
	++cp;				/* (and skip it) */
      continue;				/* get next parameter */
    }
    /* First character is not '<' OR there is not '>' to match the '<' */
    pptr[imptr][i] = cp;
    cp = strchr(cp, ',');
    if (cp != (char *) NULL)
      *cp++ = '\0';
    else {
      cp = strpbrk(pptr[imptr][i], " \t");	/* BLANK TAB */
      if (cp != (char *) NULL)
	*cp++ = '\0';
      cp = NULL;
    }
  }
  while (i<9)
    pptr[imptr][i++] = NULL;
}

/* Resulting macro line is pointed to by 'ptr' */
void get_macroline()
{
  char c, *mptr;
  int space;

  /* macroptr is the macro read pointer (points to this macro structure) */
  if (macroptr != (MACROPTR) NULL) {
    ptr = macroptr->m_line;
    macroptr = macroptr->m_next;
    /* do macro substitutions */
    mptr = line;
    /* set the destination buffer to all nulls */
    (void) memset(line, '\0', sizeof(line));
    DBG2("Macro line: %s", ptr);
    if (*ptr == '!') {
      /* Ignore leading '!' in MACRO lines */
      ++ptr;
    }
    while ((c = *ptr++) != '\0' && (space=(LINE_LEN-(mptr-line)-6)) >= 0) {
      if (c == '$') {
	switch (c = *ptr++) {
	case '$':
	  *mptr = '$';
	  break;
	case '0':
	  (void) sprintf(mptr, "%ld", (long) currentline);
	  break;
	case '1': case '2': case '3':
	case '4': case '5': case '6':
	case '7': case '8': case '9':
	  /* if NULL pointer, nothing is copied */
	  if (pptr[imptr][c-'1'] != (char *) NULL) {
	    if (space >= (int)strlen(pptr[imptr][c-'1']))
	      (void) strcpy(mptr, pptr[imptr][c-'1']);
	    else
	      (void) strncpy(mptr, pptr[imptr][c-'1'], space);
	  }
	  break;
	case '<':
	  /* Current file name */
	  if (space >= (int)strlen(curr_name))
	    (void) strcpy(mptr, curr_name);
	  else
	    (void) strncpy(mptr, curr_name, space);
	  break;
	case '(':
	  /* indirect parameter (fill in value) */
	  if (isdigit(*ptr) && *ptr != '0') {
	    /* save *ptr in digit, get next char */
	    int digit = *ptr++ - '1';
	    int fmtvalue = -1;
	    char fmtchar = 'd';
	    char fmtstring[10];
	    char *save_ptr;
	    char *char_in  = "HXDOUhxdou";
	    char *char_out = "XXdouxxdou";
	    EXPRPTR exprp = NULL;

	    while ((c = *ptr++) != '\0' && c != ')') {
	      if (isdigit(c)) {
		if (fmtvalue == -1)
		  fmtvalue = 0;
		fmtvalue = fmtvalue * 10 + c - '0';
	      } else {
		if ((save_ptr=strchr(char_in,c))!=(char *)NULL){
		  /* valid format character */
		  fmtchar = char_out[save_ptr-char_in];
		} else {
		  if (c != ':' || fmtvalue != -1) {
		    /* invalid format character -  */
		    /* give an error and ignore it */
		    errmsg("Invalid format character");
		  }
		}
	      }
	    }
	    if (c == '\0') {
	      errmsg("Invalid parameter indirection");
	      break;	/* Break from switch() "$(" */
	    }

	    if (fmtvalue <= 0)
	      fmtvalue = 1;
	    /* make sure this won't exceed max line */
	    if (space < fmtvalue) {
	      fmtvalue = space;
	      errmsg("Parameter too big (truncated)");
	    }

	    /* build the format string now */
	    (void) sprintf(fmtstring, "%%0%dl%c",
			   fmtvalue, fmtchar);

	    if (pptr[imptr][digit] != (char *) NULL) {
	      /* save the current value of ptr */
	      save_ptr = ptr;
	      /* set expression pointer to pptr[] */
	      ptr = pptr[imptr][digit];
	      /* skip any leading whitespace */
	      skipwhite();
	      /* set column to avoid accidental "commenting" */
	      /* 'column' will get set back to 1 in main loop */
	      column = 1;
	      /* evaluate the expression */
	      exprp = doexpr();
	      /* restore the value of ptr */
	      ptr = save_ptr;
	    }
	    (void) sprintf(mptr, fmtstring,
			   (exprp != (EXPRPTR) NULL)
			   ? (long) exprp->e_value : 0L);
	  } else {
	    /* just copy the source data */
	    (void) strcpy(mptr, "$(");
	  }
	  break;
	default:
	  *mptr++ = '$';	/* write the dollar sign */
	  *mptr = c;	/* write the next character */
	  break;
	}
	mptr += strlen(mptr);
      } else
	*mptr++ = c; /* copy this character */
    }
    if (c != '\0') {
      /* line is being truncated */
      errmsg("Expanded macro line too long (truncated)");
    }
    *mptr = '\0';
    DBG2("Substitute: %s", line);
    ptr = line;
  } else {
    /* No more lines in this macro */
    ptr = NULL;
  }
}

void imget()
{
  char string[LINE_LEN];
  SYMBOLPTR symbptr;

  /************************************************************/
  /* imtype in [START_INCLUDE, NO_IM, START_MACRO]		*/
  /* impending can be START_INCLUDE (add include),		*/
  /* START_MACRO (add macro), END_INCLUDE (end include),	*/
  /* or END_MACRO (end macro)					*/
  /************************************************************/
  /* Note that the current imtype entry is stored in imstack[imptr] */

  do {
    if (impending == START_INCLUDE || impending == START_MACRO) {
      if (imtype == START_INCLUDE) {
	imstack[imptr].im_addr = ftell(include);
	(void) fclose(include);
      }
      if (imtype == START_MACRO) {
	/* need to save current macro pointer (in im_filename) */
	imstack[imptr].im_filename = (char *) macroptr;
      }
      /* allocate a stack level for this entry */
      if (++imptr < IMSTACKSIZE) {
	imstack[imptr].im_type = impending;
	pptr[imptr][0] = NULL;
	imstack[imptr].im_flagptr = f_ptr;
	imstack[imptr].im_listflags = listflags;
	if (impending == START_INCLUDE) {
	  /* Add INCLUDE */
	  imstack[imptr].im_line = currentline;
	  currentline = 0;
	  imstack[imptr].im_addr = 0;
	  imstack[imptr].im_filename = curr_name;
	  (void) strcpy(imstack[imptr].im_buffer, symname);
	  curr_name = imstack[imptr].im_buffer;
	} else {
	  /* Add MACRO */
	  /* Set im_filename to be the macro pointer */
	  imstack[imptr].im_filename = (char *) entry->op_macroptr;
	  (void) strcpy(imstack[imptr].im_buffer, ptr);
	}
	impending = DO_STACK;
      } else {
	errmsg("INCLUDE or MACRO nested too deeply");
	--imptr;	/* undo ++ above */
	/* Ignore this request - can't stack it */
	impending = NO_IM;
      }
    }

    if ((impending==END_INCLUDE || impending==END_MACRO) && imtype==NO_IM) {
      errmsg("ENDM and EXITM not permitted outside of a macro");
      impending = NO_IM;
    }
    if (impending == END_INCLUDE) {
      if (imtype == START_INCLUDE) {
	if (include != (FILE *) NULL)
	  (void) fclose(include);
	currentline = imstack[imptr].im_line;
	curr_name = imstack[imptr].im_filename;
      }
      if (imtype == START_MACRO) {
	while (imptr > -1 && imstack[imptr].im_type == START_MACRO)
	  --imptr;	/* Throw away any pending MACROs */
      }
      if (imptr == -1) {
	/* END_INCLUDE, no INCLUDE active means END */
	reading = FALSE;
      }
    }
    if (impending == END_MACRO && imtype == START_INCLUDE) {
      errmsg("ENDM and EXITM not permitted outside of a macro");
      impending = NO_IM;
    }

    if (impending == END_INCLUDE || impending == END_MACRO) {
      f_ptr = imstack[imptr].im_flagptr;

      /* Restore the CODE/MACRO/PSEUDO/INCLUDE bits from im_listflags */
      listflags &= ~ LIST_ALL;	/* Clear listing state bits */
      listflags |= imstack[imptr].im_listflags & LIST_ALL;

      symbptr = addsymbol("*LISTFLAGS", (i_32) 0, S_RESOLVED);
      symbptr->s_value = (i_32) (listflags & LIST_ALL);
      if (imptr > -1)
	--imptr;
      impending = DO_STACK;
    }

    /* Set up the top item on the stack */

    if (impending == DO_STACK) {
      impending = NO_IM;
      /* To have reached this line, we must have been assembling */
      assembling = TRUE;
      if (imptr > -1) {
	imtype = imstack[imptr].im_type;
      } else {
	imtype = NO_IM;
      }
      if (imtype == START_INCLUDE) {
	/*
	 * BIN_READONLY mode used to avoid problems on MS-DOS with
	 * ftell() and *seek() on TEXT files (trouble if file ends
	 * with a ^Z )
	 */
	if ((include=openfile(imstack[imptr].im_buffer, INCLUDE_ENV,
			      INCLUDE_PATH, BIN_READONLY, NULL)) == (FILE *) NULL) {
	  currentline = imstack[imptr].im_line;
	  curr_name = imstack[imptr].im_filename;
	  (void) sprintf(string, "Unable to open INCLUDE file (%s)",
			 imstack[imptr].im_buffer);
	  errmsg(string);
	  impending = END_INCLUDE;
	} else {
	  if (fseek(include, imstack[imptr].im_addr, 0) != 0)
	    error("error setting location in include file %s",
		  nameptr);
	  free(nameptr);
	  DBG2("Address %ld: free'd memory", (long) nameptr)
	    }
      }
      if (imtype == START_MACRO) {
	/* Restore macro pointer from im_filename */
	macroptr = (MACROPTR) imstack[imptr].im_filename;
	dopptr();	/* set up pptr[] values */
      }
    }

    /********************************************************/
    /* Now actually get the line from the appropriate file  */
    /********************************************************/
    if (impending == NO_IM) {
      if (imtype == START_INCLUDE) {
	/* processing an include file */
	if ((ptr=sasmgetline(line, include)) != (char *) NULL) {
	  ++currentline;
	  DBG2("Include line: %s", ptr);
	} else {
	  /* end of this file */
	  impending = END_INCLUDE;
	  if (addingmacro) {
	    errmsg("End of file while defining macro");
	    addingmacro = FALSE;
	  }
	}
      }
      if (imtype == START_MACRO) {
	/* processing a macro now */
	get_macroline();
	if (ptr == (char *) NULL) {
	  /* end of macro -- exit from this macro     */
	  impending = END_MACRO;
	}
      }
    }
  } while (impending != NO_IM);
}

char *makename(char *base, char *prefix, char *suffix)
{
  char *destname;
  char *cp;
  unsigned namlen = strlen(base) + strlen(suffix) + 1;

  /* namlen is worst-case destname length */
  if (prefix != (char *) NULL)
    namlen += strlen(prefix);

  /* create a space to build the object name */
  if ((destname= (char *) calloc(1, namlen)) == (char *) NULL)
    error("error getting memory for %s in makename", base);
  DBG3("Address %ld: allocated %d bytes", (long) destname, namlen)
    if (prefix != (char *) NULL && *prefix != '\0') {
      (void) strcpy(destname, prefix);
      cp = strrchr(base, DIRC);
      base = (cp == (char *) NULL) ? base : cp + 1;
    }
  (void) strcat(destname, base);

  /* If there is a '.' in the name after the last DIRC, drop '.*' */
  if ((cp=strrchr(destname, DIRC)) != (char *) NULL)
    ++cp;
  else
    cp = destname;

  if ((cp=strrchr(cp, '.')) != (char *) NULL) {		
    /* trim off the source name suffix */
    *cp = '\0';
  }
  /* append "suffix" to the filename */
  (void) strcat(destname, suffix);
  return destname;
}

void setup_formats()
{
  if (listing) {
    source_col = linewidth + codewidth + 8;	/* 5 for addr, 3 blanks */

    /* old: basefmt =	"%5ld            "	*/
    /* new: basefmt =	"%4ld             "	*/
    (void) sprintf(basefmt, "%%%dld", linewidth);

    /* linewidth is always a single digit (in the range 4 through 6) */
    (void) memset(&basefmt[4], ' ', codewidth+8);
    basefmt[codewidth+12] = '\0';

    /* old: addrfmt =	"%05lX %-4.4s "	*/
    /* new: addrfmt =	"%05lX %-5.5s "	*/
    (void) sprintf(addrfmt, "%%05lX %%-%d.%ds ", codewidth, codewidth);

    /* old: inclfmt =	"     +"	*/
    /* new: inclfmt =	"    +"	*/
    (void) sprintf(inclfmt, "%*s+", linewidth, "");

    /* old: codefmt =	"            %-4.4s"	*/
    /* new: codefmt =	"           %-5.5s"	*/
    (void) sprintf(codefmt, "%*s%%-%d.%ds",linewidth+7,"",codewidth,
		   codewidth);
    /* old: lastcodefmt =	"            %s"	*/
    /* new: lastcodefmt =	"           %s"	*/
    (void) sprintf(lastcodefmt, "%*s%%s", linewidth+7, "");
  }
}

#ifdef dll
int sasmmain(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif

{
  char listline[sizeof(line)+17];	/* 17 is length of added fields */
  OPCODEPTR opcode;
  SYMBOLPTR symbptr;
  int i, field;
  b_32 oplen;
  char *opptr, *cp;

  char *current_date;

  long clock;
  struct tm *time_struct;

  progname = argv[0];
  listfile = stdout;

  sys_initialize();	/* Initialize system symbol table */

  setname(&opname, OPC_NAME);
  setname(&msgname, "-");

  if ((cp = getenv("SASM_GLOBAL")) != (char *) NULL ) /* check env for */
    force_global = (*cp == '1');		/* setting:  non-null and 1 */
	    
  sasmget_options(argc,argv);	/* Process command line options */

  if (strcmp(msgname, "-") != 0) {
    if (freopen(msgname, msg_append ? TEXT_APPEND : TEXT_WRITE, stderr)
	== (FILE *) NULL)
      error("error creating %s for writing", msgname);
    /* Set to unbuffered output */
    setbuf(stderr, (char *) NULL);
  }

  /* C'est chiant de lire ca a chaque fois...
     (void) fputs(copyr1, stderr);
     (void) fputs(copyr2, stderr);
  */

  if (errorcount > 0) {
    sasmusage();
    exit(-3);
  }

  /* Creation of the mnemonics table */
  readoptable();


  /* Set up list file format strings */
  setup_formats();


  if (argc - optind > 1 && (objdefault == FALSE || listdefault == FALSE)) {
    (void) fprintf(stderr,
		   "Options -A, -a, and -o not allowed with multiple files\n");
    sasmusage();
    exit(-4);
  }

  singlefile = (argc - optind <= 1);

  while (singlefile || argc > optind) {
    /* [Re-] initialize the user symbol table */
    s_initialize();
    /* Remove macros from previous file, if any */
    cleanmacros();

    stdinread = FALSE;

    if (singlefile) {
      if (argc == optind || strcmp("-", argv[optind]) == 0) {
	setname(&inname, "[stdin]");
	infile=stdin;
	stdinread = TRUE;
	if ((temp=tmpfile()) == (FILE *) NULL)
	  error("%s", "error creating a temporary file");
	++optind;
      }
      singlefile = FALSE;
    }

    if (stdinread == FALSE) {
      /* argc must be greater than optind, or the previous test	*/
      /* would have matched, setting inname = "[stdin]"		*/
      setname(&inname, argv[optind]);
      DBG2("input file name is %s",inname);
      if ((infile=freopen(inname,TEXT_READONLY,stdin)) == (FILE *) NULL) {
	/* open() failed - try adding the default source name suffix */
	/* First release memory allocated by setname() */
	free(inname);
	DBG2("Address %ld: free'd memory", (long) inname)
	  inname = makename(argv[optind], "", ".a");
	DBG2("Try input file name with .a appended (%s)", inname);
	if ((infile=freopen(inname,TEXT_READONLY,stdin))==(FILE *) NULL)
	  error("error opening %s for reading",argv[optind]);
      }
      ++optind;
      DBG2("input file %s opened successfully",inname);
    }

    if (listing) {
      if (listdefault == TRUE) {
	/* default list name */
	if (objdefault == TRUE && stdinread == FALSE) {
	  /* if no objname specified, default is <base of source>.l */
	  cp = listname;
	  listname = makename(inname, listprefix, listsuffix);
	  if (cp != (char *) NULL) {
	    free(cp);
	    DBG2("Address %ld: free'd memory", (long) cp)
	      }
	} else {
	  /* if objname specified, default list name is "-" */
	  /* if source is [stdin], default list name is "-" */
	  setname(&listname, "-");
	}
      }

      if (strcmp(listname, "-") == 0) {
	setname(&listname, "[stdout]");
	listfile = stdout;
      } else {
	if ((listfile = fopen(listname, TEXT_WRITE)) == (FILE *) NULL)
	  error("error creating %s for writing",listname);
      }
    }

    if (objdefault == TRUE) {
      if (stdinread == FALSE) {
	/* default object file name is <base of source>.o */
	cp = objname;
	objname = makename(inname, objprefix, objsuffix);
	if (cp != (char *) NULL) {
	  free(cp);
	  DBG2("Address %ld: free'd memory", (long) cp)
	    }
      } else {
	/* default object file name is "code" for [stdin] */
	setname(&objname, "code");
      }
    }

    /* Copy command-line symbols to symbol table */
    currentline = 0;	/* for line reference in copy_sys() */
    allsymbols(copy_sys, sys_root->s_right);

    /* Add processor-related symbols to symbol table */
    if (code_level >= LEVEL3) {
      (void) addsymbol("*LEVEL3", (i_32) 1, S_RESOLVED | S_SETPASS2);
    }
    if (code_level >= LEVEL2) {
      (void) addsymbol("*LEVEL2", (i_32) 1, S_RESOLVED | S_SETPASS2);
      (void) addsymbol("*1LR2", (i_32) 1, S_RESOLVED | S_SETPASS2);
    }
    if (code_level >= LEVEL1) {
      (void) addsymbol("*LEVEL1", (i_32) 1, S_RESOLVED | S_SETPASS2);
      (void) addsymbol("*1LK7", (i_32) 1, S_RESOLVED | S_SETPASS2);
    }
    if (code_level >= LEVEL0) {
      (void) addsymbol("*LEVEL0", (i_32) 1, S_RESOLVED | S_SETPASS2);
      (void) addsymbol("*1LF2", (i_32) 1, S_RESOLVED | S_SETPASS2);
    }
    (void) addsymbol("*LEVEL", (i_32) (code_level/LEVEL1),
		     S_RESOLVED | S_SETPASS2);

    if (time(&clock) < 0)
      error("%s","unable to read current time");
    current_date = ctime(&clock);
    (void) strncpy(date, current_date, 26);
    date[24] = '\0';	/* kill the trailing NL */

    time_struct = localtime(&clock);
    /* Add symbols with current time/date info */

    i = S_RESOLVED | S_SETPASS2;	/* Shorthand for readability */
    (void) addsymbol("*TM_SECOND", (i_32) time_struct->tm_sec, i);
    (void) addsymbol("*TM_MINUTE", (i_32) time_struct->tm_min, i);
    (void) addsymbol("*TM_HOUR", (i_32) time_struct->tm_hour, i);
    (void) addsymbol("*TM_DAYMONTH", (i_32) time_struct->tm_mday, i);
    (void) addsymbol("*TM_MONTH", (i_32) (time_struct->tm_mon + 1), i);
    (void) addsymbol("*TM_YEAR", (i_32) time_struct->tm_year, i);
    (void) addsymbol("*TM_DAYWEEK", (i_32) time_struct->tm_wday, i);
    (void) addsymbol("*TM_DAYYEAR", (i_32) (time_struct->tm_yday + 1), i);

    if ((inbase=strrchr(inname, DIRC)) != (char *) NULL)
      ++inbase;
    else
      inbase = inname;

    if ((objbase=strrchr(objname, DIRC)) != (char *) NULL)
      ++objbase;
    else
      objbase = objname;

    *prog_symbol = '*';
    (void) strncpy(&prog_symbol[1], objbase, SYMBOL_LEN-1);

    pass1 = TRUE;
    pass2 = FALSE;
    absolute = FALSE;
    ext_symbols = ext_fillrefs = 0;
    *title = '\0';
    *subtitle = '\0';

    /* Define "*START" to be the starting address of the file */
    /* If ABS is specified, "*START" will be changed to not relocatable */
    (void) addsymbol("*START", codestart, S_RESOLVED|S_SETPASS2|S_RELOCATABLE);

    if (verbose) {
      if (fprintf(stderr, "Assembling %s\n    object file = %s",
		  inname, objname) < 0)
	error("%s", "error writing to stderr");
      if (listing) {
	if (fprintf(stderr, ", list file = %s\n", listname) < 0)
	  error("%s", "error writing to stderr");
      } else {
	if (fprintf(stderr, " (No listing)\n") < 0)
	  error("%s", "error writing to stderr");
      }
    }

    while (pass1 || pass2) {
      DBG2("Starting pass %ld", (long) (pass1 + 2*pass2));
      for (i=0; i<NUMBER_OF_FLAGS; ++i) {
	flag[i] = (flag[i] & FLAG_CMDLINE)  ? flag[i] | FLAG_SET
	  : flag[i] & FLAG_CLEAR;
      }
      f_ptr = -1;	/* Reset flag stack pointer */
      carry = OP_CHGCARRY & 0xF;	/* Reset carry indicator */
      curr_name = inname;
      begin_pseudos_ok = TRUE;
      imptr = -1;
      impending = imtype = NO_IM;
      listflags = LIST_CODE | LIST_PSEUDO;
      listcount = 0;
      symbptr = addsymbol("*LISTFLAGS", (i_32) 0, S_RESOLVED);
      symbptr->s_value = (i_32) (listflags & LIST_ALL);
      reading = TRUE;
      assembling = TRUE;
      addingmacro = FALSE;
      *subtitle = '\0';
      lineofpage = pagelength;
      page = 0;

      label_index_m2 = label_index_m1 = 0;
      label_index_p2 = label_index_p1 = 0;
      index_c1 = index_c2 = index_c3 = level_stack = 0;
      abase = 0L;

      if (pass2) {
	if (strcmp(objname, "-") == 0) {
	  if (codeonly == FALSE)
	    error("%s", "object file cannot be [stdout]");
	  if (listing && listfile == stdout) {
	    (void) fprintf(stderr,
			   "List and code files both [stdout] (list disabled)\n");
	    listing = FALSE;
	  }
	  setname(&objname, "[stdout]");
	  objfile = stdout;
	  DBG1("Object file is [stdout]");
	} else {
	  if ((objfile = fopen(objname,
			       (hexcode ?TEXT_WRITE :BIN_WRITE))) == (FILE *) NULL)
	    error("error opening %s for writing",objname);
	  if (codeonly) {
	    DBG2("Opened object file (code only): %s", objname);
	  } else {
	    DBG2("Opened object file %s; writing header now",
		 objname);
	    writeheader();
	    DBG1("Done writing object file header");
	  }
	}
	nibble_offset = 0;
	out_offset = 0;
	out_position = 0;
	if (codeonly) {
	  DBG2("Object code type: %s", hexcode ? "Text" : "Binary");
	}
      }
      first_error = last_error = errorcount = warningcount = 0;
      pc = codestart;
      pc_offset = 0;
      mainline = currentline = 0;
      for (i=0; i < 256; ++i)
	charset[i] = (unsigned char) i;
      while (reading) {
	if (impending != NO_IM || imtype != NO_IM)
	  imget();
	/* Check that the INCLUDE/MACRO line didn't cause END */
	if (!reading)
	  break;
	if (imtype == NO_IM) {
	  /* not processing a macro or include now */
	  if ((ptr=sasmgetline(line, infile)) == (char *) NULL) {
	    if (addingmacro) {
	      errmsg("End of file while defining macro");
	      addingmacro = FALSE;
	    }
	    break; /* EOF -- stop reading now */
	  }
	  mainline = ++currentline;
	  if (stdinread
	      && (fputs(line,temp)==EOF || fputc('\n',temp)==EOF))
	    error("error writing to %s", "[intermediate file]");
	}
	linestart = ptr;
	column = 1;
	oplen = 0;
	if (pass2 || pass1_listing) {
	  opptr = "";
	  *mnemonic = '\0';
	  *label = '\0';
	  listflags &= ~ LIST_SUPPRESS;
	  if (imtype == (NO_IM && (listflags & LIST_CODE)
			 || (imtype==START_MACRO && (listflags&LIST_MACRO))
			 || (imtype==START_INCLUDE && (listflags&LIST_INCLUDE))
			 ))
	    listflags |= LIST_WOULD;
	  else
	    listflags &= ~ LIST_WOULD;
	  listthisline = assembling ? (listflags&LIST_WOULD) != 0 : 0;
	  listaddress = FALSE;
	}
	if (!iscomment()) {
	  linelength = strlen(linestart);
	  if (*ptr == ' ') {		/* ignore one leading blank */
	    ++ptr;
	    ++column;
	  }
	  if (getfield(label, sizeof(label)-1, SEPARATORS)) {
	    /* 'label' is big enough to allow for leading '=' */
	    listaddress = TRUE;
	  }
	  skipwhite();

	  /* We handle the special label */
	  /* We must do it here, since some instructions depend on these labels */
	  i = FALSE; /* Flag to tell if a special label is present */
		    

	  field = getfield(mnemonic, sizeof(mnemonic)-1, SEPARATORS);

	  /* Jazz label structure */
	  if (!was_test && *label && assembling && !addingmacro) {
	    if(!strcmp(label,"--")) {
	      sprintf(label, "lBm2%X", label_index_m2++);
	      i = TRUE;
	    }
	    else if(!strcmp(label,"-")) {
	      sprintf(label, "lBm1%X", label_index_m1++);
	      i = TRUE;
	    }
	    else if(!strcmp(label,"+")) {
	      sprintf(label, "lBp1%X", label_index_p1++);
	      i = TRUE;
	    }
	    else if(!strcmp(label,"++")) {
	      sprintf(label, "lBp2%X", label_index_p2++);
	      i = TRUE;
	    }
				
				/* MASD Skip structure */
	    if (masd) {
	      if (!strcmp(label,"{")) {
		pushopenlabel(label);
		i = TRUE;
	      }
	      else
		if (!strcmp(label,"}")) {
		  if (level_stack <= 0)
		    errmsg("Mismatched }");
		  else {
		    level_stack--;
		    createcloselabel(label,masd_stack[level_stack].c2);
		    /* For all } we create two labels (One for the SKELSE, one for END */
		    addlabel(label, pc, S_RESOLVED
			     | (force_global ? S_ENTRY : 0)
			     | (absolute ? 0 : S_RELOCATABLE));
		    createskiplabel(label,masd_stack[level_stack].c3);
		    i = TRUE;
		  }
		}
	    }
	    if (i) { /* If it's a special label, add it in the list */
	      DBG2("Adding a new label: %s", label);
	      addlabel(label, pc, S_RESOLVED
		       | (force_global ? S_ENTRY : 0)
		       | (absolute ? 0 : S_RELOCATABLE));
	      if (*mnemonic == '\0') {
		DBG1("Setting carry bits (stand-alone label)");
		carry = OP_C | OP_NC;
	      }
	      *label = '\0'; /* We won't add this label later */
	    }
	  }
		    
	  if (field  || addingmacro) {
	    /* If a line contains a mnemonic, then we don't count the columns */
	    column = 1;

	    DBG2("Got mnemonic: %s", mnemonic);
	    /* Truncate to MNEMONIC_LEN characters */
	    mnemonic[MNEMONIC_LEN] = '\0';
	    skipwhite();
	    if (pass1 && !pass1_listing)
	      oplen = opcodelen(mnemonic);
	    else {
	      listaddress = TRUE;
	      opcode = getopcode(mnemonic);
	      oplen = opcode->length;
	      if (opcode->variable)
		opptr = "     Not Listed";
	      else
		opptr = opcode->valuefield.value;
	      if (pass2 && oplen > 0) {
		if (opcode->variable) {
		  writeopcode2(oplen, opcode->valuefield.valuebyte);
		  free(opcode->valuefield.valuebyte);
		}
		else
		  writeopcode(oplen, opptr);
	      }
	    }
	  }
	  if (*label && assembling && !addingmacro) {
	    DBG2("Adding a new label: %s", label);
	    addlabel(label, pc, S_RESOLVED
		     | (force_global ? S_ENTRY : 0)
		     | (absolute ? 0 : S_RELOCATABLE));
	    if (*mnemonic == '\0') {
	      DBG1("Setting carry bits (stand-alone label)");
	      carry = OP_C | OP_NC;
	    }
	  }
	}
	/* listline:
	 * <linewidth> digit line #
	 * 1 blank (+ if expanded macro line)
	 * 5 digit address (non-comment line)
	 * 1 blank
	 * <codewidth> digits of code (non-comment line)
	 * 1 blank
	 * input line(expanded)
	 */
	if (pass2 || pass1_listing) {
	  if (listflags & LIST_SUPPRESS) {
	    listthisline = FALSE;
	  } else {
	    if (listflags & LIST_FORCE) {
	      if (--listcount >= 0) {
		listthisline = TRUE;
	      } else {
		listcount = 0;
		listflags &= ~ LIST_FORCE;
	      }
	    }
	  }
	  if (listing && listthisline) {
	    if (currentline > 99999 && linewidth < 6) {
	      ++linewidth;
	      setup_formats();
	    }
	    (void) sprintf(listline, basefmt, currentline);
	    if (listaddress)	/* print only if data on line */
	      (void) sprintf(&listline[linewidth+1], addrfmt, pc,
			     opptr);
	    listline[source_col - 1] = ' '; /* fix MSC bug */
	    if (imtype != NO_IM) {
	      if (imtype == START_INCLUDE) {
				/* add a '-' after the line number */
		listline[linewidth] = '-';
	      } else {
				/* blank out the line number */
		(void) strncpy(listline, inclfmt, linewidth+1);
	      }
	    }
	    copyline(&listline[source_col], linestart,
		     (int) (sizeof(listline)-1-source_col));
	    putlist(listline);
	    if ((int)strlen(opptr) > codewidth) {
	      opptr += codewidth;
	      if ((int)strlen(opptr) > codewidth) {
		(void) sprintf(listline, codefmt, opptr);
		putlist(listline);
		opptr += codewidth;
	      }
	      (void) sprintf(listline, lastcodefmt, opptr);
	      putlist(listline);
	      if ((int)strlen(opptr) > (codewidth+1))
		putlist("");
	    }
	  }
	  /* Set flag so NEXT line is first line forced */
	  if (listcount != 0)
	    listflags |= LIST_FORCE;
	}
	pc += oplen;
	if (pc >= MAX_NIBBLES) {
	  errmsg("PC wrapped around to 00000");
	  pc -= MAX_NIBBLES;
	  pc_offset += MAX_NIBBLES;
	}
      }

      if (ferror(infile))
	error("error reading from %s", inname);

      if (stdinread) {
	stdinread = FALSE;
	(void) fclose(infile);
	infile = temp;
      }
      if (pass1) {
	DBG2("Rewinding input file: %s", inname);
	rewind(infile);
	pass1 = FALSE;
	pass2 = TRUE;
	/* Define "*END" to be the ending address of the file */
	(void) addsymbol("*END", pc, S_RESOLVED | S_SETPASS2
			 | (absolute ? 0 : S_RELOCATABLE));
      } else
	pass2 = FALSE;
    }

    DBG1("Flushing nibbles remaining in object file buffer");
    flushnibs();
    /* # of nibbles in file is (pc + pc_offset - codestart) */
    DBG1("Writing symbol table");
    writesymbols();
    DBG1("Rewriting object file header");
    writeheader();
    DBG1("Done rewriting object file header");
    /********************** listings **********************/
    if (listing) {
      listsymbols();
      liststatistics();
    }

    /* cum_errors is the error count for exit() */
    cum_errors += errorcount;

    /* close open files (except infile, as it is freopened from stdin) */
    if (infile != stdin)
      (void) fclose(infile);
    if (objfile != stdout)
      (void) fclose(objfile);
    if (listing && listfile != stdout)
      (void) fclose(listfile);
  }

  exit((int) cum_errors);
  /*NOTREACHED*/
#ifdef dll
  return (0);
#endif
}
