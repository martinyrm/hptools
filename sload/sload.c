#ifdef dll
#include "dllfile.h"
#endif
#include "sload.h"
#include "debug.h"

#include "code.h"
#include "symb.h"
#include "input.h"
#include "output.h"
#include "getopt.h"


/* Data declarations for the loader */
b_16 flag[NUMBER_OF_FLAGS];	/* Flag array */
char line[LINE_LEN];		/* Current input line */
char *ptr;			/* Global line pointer */
char *inname = NULL;		/* Input file name */
char *objname = NULL;		/* Object file name */
char *msgname = NULL;		/* Message file name */
char *listname = NULL;		/* Listing file name */
char *promptname = NULL;	/* Prompt file name */
char *curname = NULL;		/* Current object file name */
				/* File streams (correspond to *name above) */
FILE *infile, *objfile, *msgfile, *listfile, *promptfile, *curfile;
int cmd_mode = TRUE;		/* Processing commands from input file */
int abortf = FALSE;		/* Abort this processing */
int abortcode = 0;		/* Code to return to caller */
int need_prompt = FALSE;	/* Prompt for input from terminal */
char *promptstring = NULL;	/* Prompt string for input from terminal */
int msg_append = FALSE;		/* Append message to file */
char *progname;			/* Current program name */
int forceflag = FALSE;		/* FOrce command specified */
i_32 next_addr = 0;		/* Next address */
i_32 start_addr = 0;		/* Start address */
int start_specified = FALSE;	/* STart command specified */
char unresolved_sym[SYMBOL_LEN+1]; /* Unresolved symbol name */
i_32 unresolved_value = 0;	/* Unresolved symbol value (filled by resolve)*/
int unresolved_specified = FALSE;/* Unresolved symbol name provided by user */
char timestamp[SYMBOL_LEN+1];	/* TimeStamp symbol name */
char tempsymbol[SYMBOL_LEN+1];	/* Temporary use symbol name */
b_16 listflags = LIST_SUMMARY | LIST_XREF | LIST_UNRESOLVED; /* Listing flags */
i_32 IDvalue = 0;		/* ID value for object file */
int locate_next = FALSE;	/* [LO or BO] specified for next module */
i_32 locate_addr;		/* Address for locate if locate_next is true */
int suppress_resolve = FALSE;	/* Resolve references after ENd command */
i_32 errorcount = 0;		/* Count of errors encountered */
MODULEPTR lastmodule = NULL;	/* pointer to the most recent module */
MODULEPTR modulelist = NULL;	/* pointer to module list */
MODULEPTR refmodule;		/* pointer to module (used by *xref) */
MODULEPTR symbmodule;		/* pointer to symbol (used by *xref) */
CRCPTR crclist = NULL;		/* pointer to CRC list */
CHECKSUMPTR cklist = NULL;	/* pointer to checksum list */
int firstletter;		/* First letter of last symbol printed */

MODULE define_module;		/* Module for Defined */

/* Global symbols used to resolve references (resolve_refs and func_resolve) */
i_32 unres_value = 0;
i_32 nibs_saved = 0;

int errors_to_msgfile = FALSE;	/* Copy all error messages to msgfile */
long pagelength = 0xFFFFFF, lineofpage, page, pagewidth = 80;
int listing = TRUE;
int codeonly = FALSE, hexcode = FALSE;
int code_option = OPTION_UNDEFINED;
time_t clocktime;			/* time reference */
char date[26];			/* Date in object file */
char *tstamp;			/* time stamp info */
char title[40];			/* Object file title */
char softkeyfile[20];		/* Soft keys filename */
int relocatable = TRUE;		/* Object file is relocatable */
int force_update = FALSE;	/* Duplicate entries resolve to last */

char string[LINE_LEN];		/* Temporary use storage (LISTITx, ERRMSGx) */
char string2[LINE_LEN];		/* Temporary use storage (LISTITx, ERRMSGx) */

char print_vers[] = "@(#) Saturn Loader, Ver. version 3.0.8, 12/07/02\n";
char *print_version = &print_vers[5];

char tohex[] = "0123456789ABCDEF";

int compile_absolute = FALSE;

#ifndef dll
int debug = FALSE;		/* debug control flag (set by -d) */
#endif
FILE *dbgfile;			/* debug file stream */
char *dbgname;			/* debug file name */

void sloadusage(void)
{
  MSG2("Usage: %s [options] ... [sourcefile]", progname);
  MSG1("options:");
  MSG1("-a	 	do not fill with zeros from zero to START when compiling in absolute");
  MSG1("-D symb[=val]	define a symbol [default value = 1]");
  MSG1("-d debugfile	debug mode, specified output file");
  MSG1("-e		write error messages to message file");
  MSG1("-f flaglist	set flags selected by flaglist (comma-separated)");
  MSG1("-H		suppress object file header and symbol (write raw code)");
  MSG1("-h		suppress object file header and symbol (write hex code)");
  MSG1("-l listfile	write the listing to listfile");
  MSG1("-M msgfile	redirect messages to msgfile (append to msgfile)");
  MSG1("-m msgfile	redirect messages to msgfile (overwrite msgfile)");
  MSG1("-N		do not produce an annotated listing");
  MSG1("-o objfile	write the object file to objfile [default=<name>.o]");
  MSG1("-p pagelen	do a page break each 'pagelen' lines [default=60]");
  MSG1("-P		prompt for input");
  MSG1("-w pagewidth	set output page width to 'width' columns [default=80]");
  MSG1("-u		force all files to update mode");
}

void doheader(void)
{
  /* Go to the top of the next page */
  if (listing) {
    lineofpage = 1;
    if (putc('\014', listfile) == EOF)	/* Form feed */
      error("error writing to list file (%s)", listname);
  }
}

void listit(char *lineout)
{
  int length = strlen(lineout) - 1;
  char *buffer;

  if (listing == FALSE)
    return;
 
  if ( (buffer = (char *)calloc(1,length + 2)) == NULL)
    error("error getting memory for %s in listit", lineout);
  strcpy(buffer, lineout);
 
  if (++lineofpage > pagelength)
    doheader();
  
  while (length >= 0 && buffer[length] == ' ')
    --length;
  buffer[++length] = '\0';
  
  if (fputs(buffer, listfile) == EOF || fputc('\n', listfile) == EOF)
    error("error writing to list file (%s)", listname);
  free(buffer);
}

void newpage(void)
{
  /* Force a new page with the next line printed */
  lineofpage = pagelength;
}

void errmsg(char *msg)
{
  char errstring[LINE_LEN];

  ++errorcount;

  (void) sprintf(errstring, "%s%s", promptstring, msg);

  if (errors_to_msgfile) {
    MSG1(errstring);
  }

  listit(errstring);
}

void prompt(char *msg)
{
  if (promptfile != (FILE *) NULL && fprintf(promptfile, "%s", msg) == EOF)
    error("error writing to prompt file (%s)", promptname);
}

void openprompt(void)
{
  setname(&promptname, "[stdout]");
  DBG2("Prompt file name set to '%s'", promptname);
  promptfile = stdout;
}

int check_union(i_32 s1, i_32 l1, i_32 s2, i_32 l2)
{
  return (s1+l1 > s2) && (s2+l2 > s1);
}

i_32 my_abs(i_32 value)
{
  return (value >= 0) ? value : -value;
}

int onenibble(i_32 value, int nibble)
{
  switch (nibble) {
  default: value >>= 4;
  case 7: value >>= 4;
  case 6: value >>= 4;
  case 5: value >>= 4;
  case 4: value >>= 4;
  case 3: value >>= 4;
  case 2: value >>= 4;
  case 1: value >>= 4;
  case 0: ;
  }
  return (value & 0xF);
}

int fromhex(char c)
{
  return (isdigit(c) ? c - '0' : toupper(c) - 'A' + 10);
}

char * mybasename(char *np)
{	/* point to last part of path */
  register char * sp=np;
  while (*sp)
    if (*sp++==DIRC) np=sp;
  return np;
}

char *fileaddr(i_32 value, MODULEPTR modptr)
{
  static char f_addr[LINE_LEN];

  (void) sprintf(f_addr, "%s(%.5lX)", mybasename(modptr->m_filespec),
		 (long) (value - modptr->m_start));
  return f_addr;
}

char *addrfileaddr(i_32 value, MODULEPTR modptr)
{
  static char af_addr[LINE_LEN];

  (void) sprintf(af_addr, "%.5lX %s", (long) value, fileaddr(value, modptr));
  return af_addr;
}

void fillin_ref(i_32 location, i_32 value, int len)
{
  register i_32 val = value;
  register i_32 loc = location;

  if (loc & 1) {		/* get to the next even address */
    putnib(loc++, val);
    len--;
    val >>= 4;
  }

  if (! len)			/* quit here for length of 0 */
    return;

  switch (len) {		/* go for in-line code here! */
  default: error("fillin_ref called with len = %d",len);
    break;

  case 7: putbyte(loc, val);
    loc += 2;
    val >>= 8;
  case 5: putbyte(loc, val);
    loc += 2;
    val >>= 8;
  case 3: putbyte(loc, val);
    loc += 2;
    val >>= 8;
  case 1: putnib(loc, val);
    break;

  case 8: putbyte(loc, val);
    loc += 2;
    val >>= 8;
  case 6: putbyte(loc, val);
    loc += 2;
    val >>= 8;
  case 4: putbyte(loc, val);
    loc += 2;
    val >>= 8;
  case 2: putbyte(loc, val);
  }
}

void func_resolve(SYMBOLPTR symb)
{
  FILLREFPTR slink_ptr = NULL;	/* pointer to SLINK reference */
  register FILLREFPTR ref = symb->s_fillref;
  i_32 inc_counter = 0;
  i_32 link_value = 0;
  register i_32 rel_addr;
  i_32 maxoffset;
  int i;

  while (ref != (FILLREFPTR) NULL) {
    switch (ref->f_class) {
    case 0:
      if ((symb->s_fillcount & S_RESOLVED) == 0) {
	/* Symbol is not resolved -- fill in unres_value */
	fillin_ref(ref->f_address, unres_value, (i_32) ref->f_length);
	break;
      }

      fillin_ref(ref->f_address, (i_32) (symb->s_value + ref->f_bias),
		 (i_32) ref->f_length);
      if ((listflags & LIST_SHORTENABLE) && ref->f_subclass == 1) {
	/************************************************************/
	/* Check shortenable absolute reference if GOVLNG or GOSBVL */
	/*                                                          */
	/* The exact distance to check varies depending on three    */
	/* conditions:                                              */
	/*                                                          */
	/*  1. Whether this is a forward or backward reference.     */
	/*  2. Whether this is a GOVLNG or GOSBVL reference.        */
	/*  3. For forward references, whether the destination will */
	/*     move if the instruction is shortened.                */
	/*                                                          */
	/* Note that conditions 2 and 3 cannot be determined by the */
	/* loader, because of insufficient information.  This code  */
	/* assumes worst case, so that every reported shortenable   */
	/* reference can actually be shortened, no matter what the  */
	/* answers to conditions 2 and 3 may be.                    */
	/*                                                          */
	/* The following table summarizes the effects of the above  */
	/* conditions, showing the effect on the distance to the    */
	/* destination:                                             */
	/*----------------------------------------------------------*/
	/* Destination affected? --> NO             YES             */
	/*                      /-----------\  /-----------\        */
	/* (Effect on distance)+------+------++------+------+       */
	/*                     |GOVLNG|GOSBVL||GOVLNG|GOSBVL|       */
	/*                     +======+======++======+======+       */
	/* Forward reference:  |      |      ||      |      |       */
	/*       GOLONG/GOSUBL |   0  |  -4  ||  -1  |  -2  |       */
	/*---------------------+------+------++------+------+       */
	/*       GOTO/GOSUB    |  -1  |  -2  ||  -2  |  -5  |       */
	/*=====================+======+======++======+======+       */
	/* Backward reference: |      |      ||      |      |       */
	/*       GOLONG/GOSUBL |   0  |  +4  ||   0  |  +4  |       */
	/*---------------------+------+------++------+------+       */
	/*       GOTO/GOSUB    |  -1  |  +2  ||  -1  |  +2  |       */
	/*                     +------+------++------+------+       */
	/*                                                          */
	/*----------------------------------------------------------*/
	/*                                                          */
	/* The offset for GOLONG/GOSUBL is [-32768,32767]           */
	/* The offset for GOTO/GOSUB is [-2048,2047]                */
	/*                                                          */
	/* The offset considering the above calculations is:        */
	/*                                                          */
	/*    GOLONG/GOSUBL = [-(32768-4),32767-0] = [-32764,32767] */
	/*    GOTO/GOSUB = [-(2048-2),2047-1] = [-2046,2046]        */
	/*                                                          */
	/************************************************************/
	if (ref->f_length == 5) {
	  rel_addr = symb->s_value + ref->f_bias - ref->f_address;
	  if (rel_addr >= -32764 && rel_addr <= 32767) {
	    /* Definitely shortenable -- check 1 or 3 nibbles */
	    i = 1;
	    if (rel_addr >= -2046 && rel_addr <= 2046)
	      i = 3;	/* GOTO/GOSUB will reach */
	    LISTIT4("Shorten(%d) ref to \"%s\" in %s",i,symb->s_name,
		    fileaddr(ref->f_address, ref->f_module));
	    nibs_saved += i;
	  }
	}
      }
      break;
    case 1:
      if ((symb->s_fillcount & S_RESOLVED) == 0) {
	/* Symbol is not resolved -- fill in unres_value */
	fillin_ref(ref->f_address, unres_value, (i_32) ref->f_length);
	break;
      }

      rel_addr = symb->s_value + ref->f_bias - ref->f_address;
      if (ref->f_subclass == 1) {
	/* Offset is relative to end of reference */
	rel_addr -= ref->f_length;
      }
      fillin_ref(ref->f_address, rel_addr, (i_32) ref->f_length);

      /* Check if the reference is in range */
      maxoffset = (((i_32) 1) << (ref->f_length * 4 - 1)) - 1;
      if (rel_addr+1 >= (-maxoffset) && rel_addr <= maxoffset) {
	/************************************************************/
	/* Check shortenable relative reference if GOLONG or GOSUBL */
	/* (4 nibble reference)                                     */
	/*                                                          */
	/* The exact distance to check varies depending on three    */
	/* conditions:                                              */
	/*                                                          */
	/*  1. Whether this is a forward or backward reference.     */
	/*  2. Whether this is a GOLONG or GOSUBL reference.        */
	/*  3. For forward references, whether the destination will */
	/*     move if the instruction is shortened.                */
	/*                                                          */
	/* Note that condition 3 cannot be determined by the loader */
	/* because of insufficient information.  This code assumes  */
	/* worst case, so that every reported shortenable reference */
	/* can actually be shortened, whether the destination will  */
	/* move or not (condition 3).                               */
	/*                                                          */
	/* The following table summarizes the effects of the above  */
	/* conditions, showing the effect on the distance to the    */
	/* destination:                                             */
	/*----------------------------------------------------------*/
	/* Destination affected? --> NO             YES             */
	/*                      /-----------\  /-----------\        */
	/* (Effect on distance)+------+------++------+------+       */
	/*                     |GOLONG|GOSUBL||GOLONG|GOSUBL|       */
	/*                     +======+======++======+======+       */
	/* Forward reference:  |      |      ||      |      |       */
	/*       (GOTO/GOSUB)  |  +1  |  +2  ||  -1  |   0  |       */
	/*=====================+======+======++======+======+       */
	/* Backward reference: |      |      ||      |      |       */
	/*       (GOTO/GOSUB)  |  -1  |  -2  ||  -1  |  -2  |       */
	/*                     +------+------++------+------+       */
	/*                                                          */
	/*----------------------------------------------------------*/
	/*                                                          */
	/* The offset for GOTO is [-2048,2047]                      */
	/* The offset for GOSUB is [-2048,2047]                     */
	/*                                                          */
	/* The offset considering the above calculations is:        */
	/*   GOTO = [-(2048+1),2047-1] = [-2049,2046]               */
	/*   GOSUB = [-(2048+2),2047-2] = [-2050,2045]              */
	/*                                                          */
	/************************************************************/
	/* Check if a 2-nibble reference would reach */
	if ((listflags & LIST_SHORTENABLE) && ref->f_length == 4
	    && ((ref->f_subclass == 0
		 && rel_addr >= -2049 && rel_addr <= 2046)
		|| (ref->f_subclass == 1
		    && rel_addr >= -2050 && rel_addr <= 2045))) {
	  /* Shortenable reference - report it */
	  LISTIT3("Shorten(2) ref to \"%s\" in %s",symb->s_name,
		  fileaddr(ref->f_address, ref->f_module));
	  nibs_saved += 2;
	}
      } else {
	/* Reference is out of range */
	char string2[LINE_LEN];
	i = sprintf(string2, "Range Error: \"%s\" %s in ",
		    symb->s_name,
		    fileaddr(symb->s_value, symb->s_module));
	(void) sprintf(&string2[i], "%s D=%05lX",
		       addrfileaddr(ref->f_address, ref->f_module),
		       (long) (symb->s_value - ref->f_address));
	/* don't report 5 nibble range errors */
	if (ref->f_length != 5) ERRMSG1(string2);
      }
      break;
    case 2:	/* LINK or SLINK reference */
      if (ref->f_subclass == 0) {
	/* This is an SLINK reference */
	if (slink_ptr == (FILLREFPTR) NULL)
	  slink_ptr = ref;
	else {
	  /* More than one SLINK reference */
	  ERRMSG2("Multiple SLINK references to symbol \"%s\"",
		  symb->s_name);
	}
      } else {
	/* This is a LINK reference */
	fillin_ref(ref->f_address,(i_32) (link_value + ref->f_bias),
		   (i_32) ref->f_length);
	link_value = ref->f_address + ref->f_length;
      }
      break;
    case 3:	/* INC(n) reference */
      fillin_ref(ref->f_address,(i_32) (inc_counter + ref->f_bias),
		 (i_32) ref->f_length);
      ++inc_counter;
      break;
    default:
      ERRMSG5(
	      "Reference of Unknown Type. Symbol(\"%s\") at %.5lX Reference type is %d.%d",
	      symb->s_name,ref->f_address,ref->f_class,ref->f_subclass);
      break;
    }
    ref = ref->f_next;
  }
  if (slink_ptr != (FILLREFPTR) NULL) {
    /* Fill in link value */
    fillin_ref(slink_ptr->f_address,(i_32) (link_value+slink_ptr->f_bias),
	       (i_32)slink_ptr->f_length);
  } else {
    if (link_value != 0) {
      /* LINK without SLINK */
      ERRMSG2("Missing SLINK reference to symbol \"%s\"", symb->s_name);
    }
  }
}

void resolve_refs(void)
{
  SYMBOLPTR symb;

  if (unresolved_specified) {
    /* Find value for unresolved references */
    symb = findsymbol(unresolved_sym);
    if (symb != (SYMBOLPTR) NULL && (symb->s_fillcount & S_RESOLVED)) {
      unres_value = symb->s_value;
    } else {
      /* UNresolved symbol not found */
      ERRMSG2("Unresolved symbol \"%s\" not defined", unresolved_sym);
    }
  }
  allsymbols(func_resolve);

  /* Finish up with messages, etc */
  if (listflags & LIST_SHORTENABLE) {
    /* Print summary of shortenable references */
    LISTIT2("Shortening references would save %5ld nibbles",
	    (long) nibs_saved);
  }

  /* Do timestamp, if any */
  symb = findsymbol(timestamp);
  if (symb != (SYMBOLPTR) NULL && symb->s_value >= start_addr) {
    /* Fill in the timestamp value */
    char *cp = tstamp;
    char c;
    i_32 addr;

    for (addr = symb->s_value; (c = *cp++) && addr < MAX_NIBBLES-1;
	 addr += 2) {
      putnib(addr, onenibble((i_32) c,0));
      putnib((addr + 1), onenibble((i_32) c, 1));
    }
    if (tstamp == date && addr < MAX_NIBBLES-3) {	/* finish blanking */
      putnib(addr++, 0);	/* write ' ' */
      putnib(addr++, 2);
      putnib(addr++, 0);	/* write ' ' */
      putnib(addr++, 2);
    }
  }
}

void compute_checksums(CHECKSUMPTR ckhead)
{
  SYMBOLPTR symb;
  unsigned int total;
  i_32 fill_addr, ckaddr;

  while (ckhead != (CHECKSUMPTR) NULL) {
    /* Find the symbol */
    symb = findsymbol(ckhead->ck_symbol);
    if (symb != (SYMBOLPTR) NULL) {
      /* Found it -- do the checksum */
      total = 0;
      fill_addr = symb->s_value;

      if (fill_addr + 1 > MAX_NIBBLES) {
	ERRMSG2("CHecksum address too large (%s)", symb->s_name);
	continue;	/* Try next CHecksum location */
      }
      /* Make sure the initial location is zeroed */
      putnib(fill_addr, 0);
      putnib((i_32) (fill_addr+1), 0);
      for (ckaddr=ckhead->ck_start; ckaddr<=ckhead->ck_end; ++ckaddr) {
	if (ckaddr & 1) {
	  /* ODD address */
	  total += (getnib(ckaddr) << 4);
	} else {
	  total += getnib(ckaddr);
	}
	if (total > 0xFF)
	  total = (total & 0xFF) + total / 0x100;
      }

      /* Compute value to store as checksum */
      if (total > 1)
	total = ((~ total) & 0xFF) + 1;
      else
	total = 1 - total;

      /*Store the value in the checksum location */
      putnib(fill_addr,            onenibble((i_32) total, 0));
      putnib((i_32) (fill_addr+1), onenibble((i_32) total, 1));
    } else {
      /* Checksum symbol not found */
      ERRMSG2("Checksum symbol %s not found", ckhead->ck_symbol);
    }

    /* Get next checksum item in chain */
    ckhead = ckhead->ck_next;
  }
}

void compute_crcs(CRCPTR crcptr)
{
  i_32 crcaddr, crcvalue;
  MODULEPTR mod;

  while (crcptr != (CRCPTR) NULL) {
    if (crcptr->crc_type == ERNI_CRC || crcptr->crc_type == KCRC_SUM)
      crcaddr = crcptr->crc_offset;
    else
      crcaddr = crcptr->crc_start
	+ (2 * crcptr->crc_sectors - 1) * crcptr->crc_offset
	+ crcptr->crc_reads * READSIZE - 4;

    if (crcaddr + 3 > MAX_NIBBLES) {
      ERRMSG1("CRc address error");
      continue;	/* Try next CRc location */
    }

    /* Make sure CRC is included in file */
    if (crcaddr + 4 > next_addr)
      next_addr = crcaddr + 4;

    /* Pre-store 0xFFFF in crc location */
    putnib(crcaddr, 15);
    putnib((i_32) (crcaddr+1), 15);
    putnib((i_32) (crcaddr+2), 15);
    putnib((i_32) (crcaddr+3), 15);

    switch(crcptr->crc_type) {
    case LEWIS_CRC:
      crcvalue = computecrc(crcptr->crc_start, crcptr->crc_offset,
			    crcptr->crc_reads, crcptr->crc_sectors);
      break;
    case KERMIT_CRC:
      crcvalue = computekcrc(crcptr->crc_start, crcptr->crc_offset,
			     crcptr->crc_reads, crcptr->crc_sectors);
      break;
    case KCRC_SUM:
      crcvalue = computekcsum(crcptr->crc_start, crcptr->crc_offset);
      break;
    case ERNI_CRC:
      crcvalue = computeecrc(crcptr->crc_start, crcaddr);
      break;
    }
    mod = modulelist;
    while (crcptr->crc_type != KCRC_SUM && mod != (MODULEPTR) NULL) {
      if (check_union(mod->m_start, mod->m_length, crcaddr, (i_32) 4)) {
	/* Found an overlap */
	ERRMSG2(
		"Warning:  Module %s overlaps following CRC fill location",
		mybasename(mod->m_filespec));
      }
      mod = mod->m_next;
    }
    LISTIT3("Filling 4 nibble CRC check value %04lX at address %.5lX",
	    (long) crcvalue, (long) crcaddr);
    putnib(       crcaddr,     onenibble(crcvalue, 3));
    putnib((i_32) (crcaddr+1), onenibble(crcvalue, 2));
    putnib((i_32) (crcaddr+2), onenibble(crcvalue, 1));
    putnib((i_32) (crcaddr+3), onenibble(crcvalue, 0));
    crcptr = crcptr->crc_next;
  }
}

void list_summary(void)
{
  MODULEPTR modptr;
  b_16 out_symbols = 0, out_fillrefs = 0;
  i_32 codesize = 0;

  LISTIT1(print_version);
  LISTIT1("");
  if (objname != (char *) NULL) {
    /* Object file specified */

    if (code_option & OPTION_SYMBOLS) {
      /* Symbol records are to be written - set counts */
      out_symbols = symbols;
    }

    if (code_option & OPTION_REFS) {
      /* Symbol records are to be written - set counts */
      out_fillrefs = fillrefs;
    }

    if (code_option & OPTION_CODE) {
      /* Code records are to be written - set codesize */
      codesize = (next_addr>start_addr) ? next_addr - start_addr : 0;
    }

    LISTIT2("Output Module:\nModule=%s", objname);
    if (codesize != 0) {
      /* Output file has code */
      LISTIT6(
	      "  Start=%.5lX End=%.5lX Length=%.5lX   Symbols=%4ld References=%4ld",
	      (long) start_addr, (long) (next_addr - 1), (long) codesize,
	      (long) out_symbols, (long) out_fillrefs);
    } else {
      /* No code in output file */
      LISTIT4(
	      "  Start=%.5lX Module Contains No Code  Symbols=%4ld References=%4ld",
	      (long) start_addr, (long) out_symbols, (long) out_fillrefs);
    }
    LISTIT3("  Date=%-26s Title=%s", date, title);
    if (unresolved_specified) {
      LISTIT3("  Unresolved references filled with value of %s (%ld)",
	      unresolved_sym, unresolved_value);
    }
    LISTIT1("");
  }
  LISTIT1("");
  if (modulelist != (MODULEPTR) NULL) {
    LISTIT1("Source modules:");

    modptr = modulelist;
    while (modptr != (MODULEPTR) NULL) {
      LISTIT2("Module=%s", modptr->m_filespec);
      if (modptr->m_error) {
	/* Error reading module */
	LISTIT1("  ***** Error while reading this module *****");
      }
      if (modptr->m_length > 0) {
	/* Module has code */
	LISTIT4("  Start=%.5lX End=%.5lX Length=%.5lX",
		(long) modptr->m_start,
		(long) (modptr->m_start + modptr->m_length - 1),
		(long) modptr->m_length);
      } else {
	/* No code in output file */
	LISTIT2("  Start=%.5lX Module Contains No Code",
		(long) modptr->m_start);
      }
      LISTIT3("  Date=%-26s Title=%s",modptr->m_date,modptr->m_title);
      LISTIT1("");
      modptr = modptr->m_next;
    }
  }
}

void func_xref(SYMBOLPTR symb)
{
  char listline[LINE_LEN];
  FILLREFPTR refptr;
  int col;

  if (firstletter != *symb->s_name) {
    /* New letter -- write a blank line */
    LISTIT1("");
    firstletter = *symb->s_name;
  }
  col = sprintf(listline, "%-32s %c ", symb->s_name,
		(symb->s_fillcount & S_RELOCATABLE) ? '*' : '=');
  if (symb->s_fillcount & S_RESOLVED)
    col += sprintf(&listline[col], "%.5lX -", symb->s_value);
  else
    col += sprintf(&listline[col], "      -");
  refptr = symb->s_fillref;
  while (refptr != (FILLREFPTR) NULL) {
    /* List the references to this symbol */
    if (col + 6 > pagewidth) {
      LISTIT1(listline);
      col = sprintf(listline, "%34s","+");
    }
    col += sprintf(&listline[col], " %.5lX", refptr->f_address);
    refptr = refptr->f_next;
  }
  LISTIT1(listline);
}

void print_longsymb(SYMBOLPTR symb)
{
  char listline[LINE_LEN];
  int col;
  int is_reloc = (symb->s_fillcount & S_RELOCATABLE) != 0;

  col = sprintf(listline, "%-32s %c ", symb->s_name, is_reloc ? '*' : '=');
  if (symb->s_fillcount & S_RESOLVED) {
    col += sprintf(&listline[col], "%.5lX ", symb->s_value);
    (void) strcpy(&listline[col],
		  is_reloc ? fileaddr(symb->s_value, symb->s_module)
		  : mybasename(symb->s_module->m_filespec));
  }
  LISTIT1(listline);
}

void print_longref(SYMBOLPTR symb, FILLREFPTR refptr)
{
  char listline[LINE_LEN];
  int col;

  col = sprintf(listline,
		"%-15s%-28s Ty/Nib=%d.%d/%d", "", addrfileaddr(refptr->f_address,
							       refptr->f_module),
		(int) refptr->f_class, (int) refptr->f_subclass,
		(int) refptr->f_length);
  if (refptr->f_class == 1) {
    col += sprintf(&listline[col], " Dist=%05lX",
		   (long) my_abs((i_32) (refptr->f_address - symb->s_value)));
  }
  if (refptr->f_bias != 0) {
    col += sprintf(&listline[col], " Offset= %3ld",(long) refptr->f_bias);
  }
  LISTIT1(listline);
}

void func_longxref(SYMBOLPTR symb)
{
  FILLREFPTR refptr = symb->s_fillref;

  if (firstletter != *symb->s_name) {
    /* New letter -- write a blank line */
    LISTIT1("");
    firstletter = *symb->s_name;
  }

  /* List the symbol record */
  print_longsymb(symb);

  /* List the references to this symbol */
  while (refptr != (FILLREFPTR) NULL) {
    print_longref(symb, refptr);
    refptr = refptr->f_next;
  }
}

void func_symbrefs(SYMBOLPTR symbptr)
{
  FILLREFPTR refptr = symbptr->s_fillref;
  int first_ref = TRUE;

  if (symbptr->s_module == symbmodule) {
    /* Symbol module matches */
    while (refptr != (FILLREFPTR) NULL) {
      if (refptr->f_module == refmodule) {
	if (first_ref) {
	  firstletter = *symbptr->s_name; /* At least 1 line printed*/
	  print_longsymb(symbptr);
	  first_ref = FALSE;
	}
	print_longref(symbptr, refptr);
      }
      refptr = refptr->f_next;
    }
  }
}

void func_unresolved(SYMBOLPTR symb)
{
  if ((symb->s_fillcount & S_RESOLVED) == 0) {
    /* This symbol is not resolved */

    if (firstletter == -1) {
      /* This is the first unresolved reference found */
      firstletter = 0;
      newpage();
      LISTIT1("Unresolved References");
      LISTIT1("");
    }

    /* If room, add it to the line; else print it */
    if (firstletter + 17 > pagewidth) {
      LISTIT1(string);
      firstletter = 0;
    }
    firstletter += sprintf(&string[firstletter], "%-32s(%3d)   ",
			   symb->s_name,
			   (int) (symb->s_fillcount & S_REFERENCEMASK));
  }
}

void list_xref(void)
{
  newpage();
  LISTIT1("Saturn Cross Reference Listing");
  firstletter = -1;
  allsymbols(func_xref);
  LISTIT1("");
}

void list_longxref(void)
{
  newpage();
  LISTIT1("Saturn Long Cross Reference Listing");
  firstletter = -1;
  allsymbols(func_longxref);
  LISTIT1("");
}

void list_references(void)
{
  refmodule = modulelist;

  while (refmodule != (MODULEPTR) NULL) {
    newpage();
    symbmodule = modulelist;
    LISTIT2("Saturn Reference Listing For Module %s",refmodule->m_filespec);
    LISTIT1("");
    while (symbmodule != (MODULEPTR) NULL) {
      firstletter = -1;
      allsymbols(func_symbrefs);
      if (firstletter != -1) {
	LISTIT1("");	/* At least one symbol printed */
      }
      symbmodule = symbmodule->m_next;
    }
    refmodule = refmodule->m_next;
  }
  LISTIT1("");
}

void list_symbols(void)
{
  symbmodule = modulelist;

  while (symbmodule != (MODULEPTR) NULL) {
    newpage();
    refmodule = modulelist;
    LISTIT2("Saturn Symbol Listing For Module %s",symbmodule->m_filespec);
    LISTIT1("");
    while (refmodule != (MODULEPTR) NULL) {
      firstletter = -1;
      allsymbols(func_symbrefs);
      if (firstletter != -1) {
	LISTIT1("");	/* At least one symbol printed */
      }
      refmodule = refmodule->m_next;
    }
    symbmodule = symbmodule->m_next;
  }
  LISTIT1("");
}

void list_unresolved(void)
{
  firstletter = -1;
  allsymbols(func_unresolved);
  if (firstletter != -1) {
    /* At least one unresolved symbol found */
    LISTIT1(string);	/* Flush the buffered items */
    LISTIT1("");
  }
}

void list_code(void)
{
  i_32 addr;
  int col = 0;

  newpage();
  LISTIT1("Saturn Hex Code Listing");
  for (addr = start_addr & (~ (i_32) 0x3F); addr < next_addr; ++addr) {
    if ((addr & 0x3F) == 0) {
      /* 64-nibble address boundary */
      string[col] = '\0';	/* Terminate the string */
      LISTIT1(string);
      if ((addr & 0xFF) == 0) {
	LISTIT1("");	/* Extra blank every 0x100 nibbles */
      }
      col = sprintf(string, "%.5lX -", (long) addr);
    }

    if ((addr & 0x7) == 0)
      string[col++] = ' ';

    string[col++] = (addr < start_addr) ? 'x' : tohex[getnib(addr)];
  }
  string[col] = '\0';	/* Terminate the string */
  LISTIT1(string);
  LISTIT1("");
}

void write_list(void)
{
  if (listing == FALSE) {
    /* Listing not enabled - don't bother processing it */
    return;
  }

  if (listflags & LIST_SUMMARY)
    list_summary();
  if (listflags & LIST_XREF)
    list_xref();
  if (listflags & LIST_LONGXREF)
    list_longxref();
  if (listflags & LIST_REFERENCES)
    list_references();
  if (listflags & LIST_SYMBOLS)
    list_symbols();
  if (listflags & LIST_UNRESOLVED)
    list_unresolved();
  if (listflags & LIST_CODE)
    list_code();
}

int findflag(i_16 name)
{
  int i;

  for (i=0; i < NUMBER_OF_FLAGS; ++i)
    if (flag[i] == name)
      return TRUE;
  return FALSE;
}

int addflag(i_16 name)
{
  int i;

  if (findflag(name))
    return TRUE;		/* flag is already there */

  for (i=0; i < NUMBER_OF_FLAGS; ++i) {
    if (flag[i] == (i_16) 0) {
      flag[i] = name;
      return TRUE;	/* found a place to add it */
    }
  }
  return FALSE;		/* no place to add this flag */
}

int dropflag(i_16 name)
{
  int i;

  for (i=0; i < NUMBER_OF_FLAGS; ++i) {
    if (flag[i] == name)
      flag[i] = 0;
  }
  return FALSE;
}

int force(void)
{
  int result = forceflag;

  forceflag = FALSE;
  return result;
}

/* Start of command processing routines */

void do_abort(void)
{
  cmd_mode = FALSE;
  abortf = TRUE;
  abortcode = 0;
}

void do_annotate(void)
{
  copyline(string, ptr, (int) LINE_LEN);
  LISTIT1(string);
}

void do_boundary(void)
{
  i_32 boundary = 0;
  locate_next = TRUE;
  boundary = read_addr();
  if (boundary > 0)
    locate_addr = ((next_addr + boundary - 1) / boundary) * boundary;
  else
    locate_addr = next_addr;
}

void do_check(void)
{
  CHECKSUMPTR *ckpptr = &cklist;
  CHECKSUMPTR newck;
  i_32 ckstart, ckend;

  if (read_symbol(tempsymbol) != (char *) NULL && (ckstart=read_addr()) != -1
      && (ckend=read_addr()) != -1) {
    /* Got all three parameters */
    while (*ckpptr != (CHECKSUMPTR) NULL)
      ckpptr = &(*ckpptr)->ck_next;

    /* ckpptr points to the last link field */
    if ((*ckpptr=(CHECKSUMPTR)calloc(1,CHECKSUMSIZE)) == (CHECKSUMPTR) NULL)
      error("error calloc'ing %ld bytes for new checksum",
	    (long) CHECKSUMSIZE);
    newck = *ckpptr;
    DBG3("Address %ld: allocated %d bytes (do_check)", (long) newck,
	 CHECKSUMSIZE);
    (void) strncpy(newck->ck_symbol, tempsymbol, SYMBOL_LEN);
    newck->ck_symbol[SYMBOL_LEN] = '\0'; /* be sure symbol ends with NUL */
    newck->ck_start = ckstart;
    newck->ck_end = ckend;
  }
}

void do_comment(void)
{
  /* No processing necessary for comment */
}

void do_crsum(CRC_TYPE type)
{
  CRCPTR *crcpptr = &crclist;
  CRCPTR newcrc;
  SYMBOLPTR syms, syme;
  char esymb[SYMBOL_LEN+1];

  if (read_symbol(tempsymbol) != (char *) NULL &&
      read_symbol(esymb) != (char *) NULL) {
    if ((syms = findsymbol(tempsymbol)) == (SYMBOLPTR) NULL) {
      ERRMSG2("Symbol %s not defined.", tempsymbol);
    } else if (! (syms->s_fillcount & S_RESOLVED)) {
      ERRMSG2("Symbol %s not resolved.", tempsymbol);
    } else if ((syme = findsymbol(esymb)) == (SYMBOLPTR) NULL) {
      ERRMSG2("Symbol %s not defined.", esymb);
    } else if (! (syme->s_fillcount & S_RESOLVED)) {
      ERRMSG2("Symbol %s not resolved.", esymb);
    } else if (syms->s_value >= syme->s_value-4) {
      ERRMSG3("Start (%5lX) >= End (%5lX)", syms->s_value, syme->s_value);
    } else { /* start and end exist and are resolved and start < end! */
      while (*crcpptr != (CRCPTR) NULL)
	crcpptr = &(*crcpptr)->crc_next;

      /* crcpptr points to last link field */
      if ((*crcpptr = (CRCPTR) calloc(1,CRCSIZE)) == (CRCPTR) NULL)
	error("error calloc'ing %ld bytes for new crc", (long) CRCSIZE);
      newcrc = *crcpptr;
      DBG3("Address %ld: allocated %d bytes (do_crc)", (long)newcrc, CRCSIZE);
      newcrc->crc_start = syms->s_value;
      newcrc->crc_offset = syme->s_value-4;
      newcrc->crc_type = type;
    }
  }
}

void do_crc(CRC_TYPE type)
{
  CRCPTR *crcpptr = &crclist;
  CRCPTR newcrc;
  i_32 crcstart, crcoffset, crcreads, crcsectors;

  if ((crcstart=read_addr()) != -1 && (crcoffset=read_addr()) != -1
      && (crcreads=read_addr()) != -1 && (crcsectors=read_addr()) != -1) {
    /* Got all four parameters */
    while (*crcpptr != (CRCPTR) NULL)
      crcpptr = &(*crcpptr)->crc_next;

    /* crcpptr points to last link field */
    if ((*crcpptr = (CRCPTR) calloc(1,CRCSIZE)) == (CRCPTR) NULL)
      error("error calloc'ing %ld bytes for new crc", (long) CRCSIZE);
    newcrc = *crcpptr;
    DBG3("Address %ld: allocated %d bytes (do_crc)", (long)newcrc, CRCSIZE);
    newcrc->crc_start = crcstart;
    newcrc->crc_offset = crcoffset;
    newcrc->crc_reads = crcreads;
    newcrc->crc_sectors = crcsectors;
    newcrc->crc_type = type;
  }
}

void dbug(char *file)
{
  setname(&dbgname, file);
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
}


void do_dbug(void)
{
  char * cp;
  cp = read_filespec();
  if (cp == (char *) NULL) {
    debug = FALSE;		/* no file turns it off */
    return;
  }
  dbug(cp);
}

void do_define(void)
{
  SYMBOLPTR symb;
  i_32 sym_value;

  if (read_symbol(tempsymbol) != (char *) NULL) {
    if ((sym_value = read_addr()) != -1) {
      /* Valid address [if not valid, read_addr() calls errmsg()] */
      symb = getsymbol(tempsymbol);
      if (force() || (symb->s_fillcount & S_RESOLVED) == 0) {
	/* Not currently defined */
	symb->s_value = sym_value;
	symb->s_fillcount |= S_RESOLVED;
	symb->s_fillcount &= ~ ((b_16) S_RELOCATABLE);
	symb->s_module = &define_module;
      } else {
	/* Already defined -- error unless same definition */
	if (symb->s_value != sym_value
	    || (symb->s_fillcount & S_RELOCATABLE)) {
	  ERRMSG2("Symbol %s is already defined", symb->s_name);
	}
      }
    }
  } else {
    ERRMSG1("Missing symbol for DEfine");
  }
}

void do_end(void)
{
  i_16 option_char;

  cmd_mode = FALSE;
  suppress_resolve = FALSE;

  /* Check for NOresolve, REsolve flag */
  option_char = read_cmd();
  switch (option_char) {
  case NUll:
  case REsolve:
    /* case ENd: 	Include this line for strict sload compatibility */
    break;
  case NOresolve:
    suppress_resolve = TRUE;
    break;
  default:
    ERRMSG1("Unrecognized ENd option");
    break;
  }
}

void do_flag(void)
{
  i_16 flagname = read_cmd();

  if (flagname != NUll && !addflag(flagname)) {
    ERRMSG1("Too many flags");
  }
}

void do_force(void)
{
  forceflag = TRUE;
}

void do_help(void)
{
  /* Here I am -- possibly enhance this to allow a specific command? */
  LISTIT1(print_version);
  LISTIT2("  Maximum output module address is %.5lX", (long) MAX_NIBBLES);
  LISTIT1("Legal Commands are:");
  LISTIT1("  ABort");
  LISTIT1("  ANnotate <Comment>");
  LISTIT1("  BOundary <Hex addr> - Start next module on multiple of <addr>");
  LISTIT1("  CHeck    <Symbol name> <Hex addr> <Hex addr>");
  LISTIT1("  CKsum    <Start symbol> <End symbol>");
  LISTIT1("     (Kermit type CRC--CRC value stored in 4 nibs before End)");
  LISTIT1("  CRc      <Start> <Offset> <Reads/Halfsector> <Sectors>");
  LISTIT1("     (Lewis type CRC - 16 nibble reads)");
  LISTIT1("  DBug	<file name>");
  LISTIT1("  DEfine   <Symbol name> <Hex addr>");
  LISTIT1("  ENd      [<End Option>] - end commands and execute linking");
  LISTIT1("     Either of:");
  LISTIT1("       REsolveReferences (default) - resolve all references");
  LISTIT1("       NOresolveReferences - don't fill in references");
  LISTIT1("  ECrc     <Start> <End>");
  LISTIT1("     (Ernie type CRC)");
  LISTIT1("  FLag     <FlagName>");
  LISTIT1("  FOrce");
  LISTIT1("  HElp");
  LISTIT1("  ID       <Hex number>");
  LISTIT1("  IF [NOt] <FlagName> <Command...>");
  LISTIT1("  KCrc     <Start> <Offset> <Reads/Halfsector> <Sectors>");
  LISTIT1("     (KERMIT type CRC - 16 nibble reads)");
  LISTIT1("  LIst     <List Option list>");
  LISTIT1("     Any of:");
  LISTIT1("       ALl");
  LISTIT1("       COde");
  LISTIT1("       SYmbols");
  LISTIT1("       LXref");
  LISTIT1("       REferences");
  LISTIT1("       SHortenableReferences");
  LISTIT1("       SUmmary");
  LISTIT1("       UNresolvedRefs");
  LISTIT1("       XRef");
  LISTIT1("  LLu      <File spec>");
  LISTIT1("  LOcate   <Hex addr>");
  LISTIT1("  NExt - print out address at which next module would be loaded");
  LISTIT1("  OPtion   <Option list>");
  LISTIT1("     Any of:");
  LISTIT1("       COde only");
  LISTIT1("       SYmbols only");
  LISTIT1("       RElocatable (default) - code and symbols");
  LISTIT1("       NRefs (for simulator) - code and symbol names only");
  LISTIT1("  OUtput   <File spec> - specify object file");
  LISTIT1("  PAtch    <Hex Addr> <nibble>...");
  LISTIT1("  PUrge    <File spec>");
  LISTIT1("  RAnge    <SymbolName> <Hex Addr> <Hex Addr> - check symbol in range");
  LISTIT1("  RElocate <File spec> - incorporate code at NExt address");
  LISTIT1("  SEarch   <File spec> - read symbols only");
  LISTIT1("  SOftkeys <File spec>");
  LISTIT1("  STart    <Hex addr> - set first address for output module");
  LISTIT1("  SUppress <List Option list>");
  LISTIT1("  TItle    <Title text>");
  LISTIT1("  TStamp   <Symbol name> [<date string>]");
  LISTIT1("  TRansfer <File spec> - read commands from a file");
  LISTIT1("  UDefine  <Symbol name> - unresolve symbol");
  LISTIT1("  UFlag    <flagname> - unset flagname");
  LISTIT1("  UNresolved <Symbol name> - set fill value for unresolved references");
  LISTIT1("  URelocate <File spec> - duplicate symbols take precedence");
  LISTIT1("  USearch   <File spec> - duplicate symbols take precedence");
  LISTIT1("  ZEro     <Hex addr> <Hex addr> - NOT NEEDED!!!!!!");
  LISTIT1("  /Abort");
  LISTIT1("  /End");
  LISTIT1("  ??");
  LISTIT1("  ** <Comment>");
}

void do_id(void)
{
  /* Get hex value, assign it to IDvalue */
  IDvalue = read_addr();
  if (IDvalue < 0)
    IDvalue = 0;	/* read_addr() already produced an error msg */
}

void do_if(void)
{
  i_16 cmd;
  int complement = FALSE;
  int found = FALSE;

  while ((cmd=read_cmd()) == NOt) {
    complement = ! complement;
  }
  found = findflag(cmd);
  if (complement)
    found = !found;
  if (found)
    docmd(read_cmd());
}

b_16 read_listflags(void)
{
  b_16 tempflags = 0;
  i_16 flagtype;

  while ((flagtype=read_cmd()) != NUll) {
    switch(flagtype) {
    case SYmbols:
      tempflags |= LIST_SYMBOLS;
      break;
    case LXref:
      tempflags |= LIST_LONGXREF;
      break;
    case REferences:
      tempflags |= LIST_REFERENCES;
      break;
    case SHortenable:
      tempflags |= LIST_SHORTENABLE;
      break;
    case SUmmary:
      tempflags |= LIST_SUMMARY;
      break;
    case COde:
      tempflags |= LIST_CODE;
      break;
    case UNresolved:
      tempflags |= LIST_UNRESOLVED;
      break;
    case XRef:
      tempflags |= LIST_XREF;
      break;
    case ALl:
      tempflags |= LIST_ALL;
      break;
    default:
      ERRMSG1("Unrecognized listing type");
      break;
    }
  }
  return tempflags;
}

void do_list(void)
{
  listflags |= read_listflags();
}

void do_llu(void)
{
  char *cp;

  if (listing && (listfile != stdout)) {
    /* First close current list file */
    (void) fclose(listfile);
  }

  /* Get a file specifier */
  cp = read_filespec();
  if (cp != (char *) NULL)
    setname(&listname, cp);
  else {
    setname(&listname, "[stdout]");
    listfile = stdout;
  }

  if (strcmp(listname, "-") != 0) {
    if ((listfile=fopen(listname, TEXT_WRITE)) == (FILE *) NULL)
      error("unable to open %s as list file", listname);
  } else {
    listfile = stdout;
    setname(&listname, "[stdout]");
  }
  DBG2("List file name set to '%s'", listname);
}

void do_locate(void)
{
  locate_addr = read_addr();
  if (locate_addr < 0)
    locate_addr = next_addr; /* read_addr() already produced an error msg */
  locate_next = TRUE;
}

void do_next(void)
{
  LISTIT2("Next available location is %.5lX", (long) next_addr);
}

void do_option(void)
{
  if (! force() && code_option != OPTION_UNDEFINED) {
    ERRMSG1("Duplicate OPtion command");
    return;
  }

  switch(read_cmd()) {
  case COde:
    code_option = OPTION_CODE;
    break;
  case SYmbols:
    code_option = OPTION_SYMBOLS;
    break;
  case RElocate:
    code_option = OPTION_RELOCATABLE;
    break;
  case NRefs:
    code_option = OPTION_NOREFS;
    break;
  default:
    ERRMSG1("Unrecognized OPtion");
    break;
  }
}

void do_output(void)
{
  char *cp;

  /* Get a file specifier */
  cp = read_filespec();
  if (cp != (char *) NULL) {
    setname(&objname, cp);
  } else {
    ERRMSG1("Missing object file name");
  }
}

void do_patch(void)
{
  i_32 patch_addr;
  char oldcode[17];
  char c;
  int i = 0;

  patch_addr = read_addr();
  if (patch_addr >= 0) {
    for (i=0; i < 16; ++i) {
      oldcode[i] = tohex[getnib((i_32) (patch_addr + i))];
    }
    oldcode[16] = '\0';

  }
  LISTIT3("%.5lX - %s", (long) patch_addr, oldcode);

  skipwhite();
  while (c=read_char(), isxdigit(c)) {
    /* Put this hex digit at patch_addr, increment to next address */
    if (patch_addr < MAX_NIBBLES) {
      putnib(patch_addr++, (int) fromhex(c));
    } else {
      /* Address out of range! */
      ERRMSG2("PAtch address too large (%.5lX)", (long) patch_addr);
      break;	/* Break out of while() */
    }
  }
}

void do_purge(void)
{
  char *cp;

  cp = read_filespec();

  if (cp != (char *) NULL) {
    if (unlink(cp) == -1) {
      ERRMSG1("Unable to purge file");
    }
  } else {
    ERRMSG1("Missing purge file name");
  }
}

void do_range(void)
{
  i_32 low_value, high_value;
  SYMBOLPTR symb;
  
  if (read_symbol(tempsymbol) != (char *) NULL
      && (low_value=read_addr()) != -1
      && (high_value=read_addr()) != -1) {
    /* Got all three parameters */
    symb = findsymbol(tempsymbol);
    if (symb != (SYMBOLPTR) NULL) {
      /* Found the symbol -- check the range */
      if (symb->s_fillcount & S_RESOLVED) {
	if (symb->s_value<low_value || symb->s_value>high_value) {
	  ERRMSG6("Symbol %s at %s not in range %.5lX [%.5lX...%.5lX]",
		  symb->s_name,
		  addrfileaddr(symb->s_value, symb->s_module),
		  (long)(symb->s_value - symb->s_module->m_start),
		  (long) low_value, (long) high_value);
	}
      } else {
	/* Symbol found, but not resolved */
	ERRMSG2("Unable to check range of symbol %s (not resolved)",
		symb->s_name);
      }
    } else {
      /* Symbol not found at all */
      ERRMSG2("Unable to check range of symbol %s", tempsymbol);
    }
  }
}

#define SBLKS2READ	32		/* 8 K reads */

O_SYMBOLPTR getobject(int skip)
{
  static struct {
    char nam[4];
    O_SYMBOL s[SYMBS_PER_RECORD];
  } in[SBLKS2READ];
  static b_16 sblks_read, cur_sblk, cur_rec;
  O_SYMBOLPTR sym_rec;

  if (skip < 0) {		/* initialization call */
    sblks_read = cur_rec = 0;
    cur_sblk = 0;
    return (O_SYMBOLPTR) NULL;
  }
  cur_rec += skip;
  cur_sblk += cur_rec / SYMBS_PER_RECORD;
  cur_rec %= SYMBS_PER_RECORD;

  while (cur_sblk >= sblks_read) {
    cur_sblk -= sblks_read;
    if ((sblks_read = fread((char *) in, 256, SBLKS2READ, curfile)) == 0) {
      ERRMSG2("Problem reading symbol block in file (%s)", nameptr);
      return (O_SYMBOLPTR) NULL;
    }
    if (strncmp(in[0].nam, "Symb", 4) != 0) {
      ERRMSG2("Corrupt symbol table in file (%s)", nameptr);
      return (O_SYMBOLPTR) NULL;
    }
  }

  sym_rec = &in[cur_sblk].s[cur_rec];
  ++cur_rec;

  return sym_rec;
}

void read_symbs(i_16 symcount, int searchonly, int update)
{
  O_SYMBOLPTR sym;
  register SYMBOLPTR oldsym = (SYMBOLPTR) NULL;
  O_FILLREFPTR refptr;
  b_16 refcount=0;
  int sym_resolved, sym_relocatable;

  (void) getobject(-1);		/* reset record pointers */
  while (symcount-- > 0) {
    if ((sym = getobject(refcount)) == (O_SYMBOLPTR) NULL)
      break;
    refcount = get_u16(sym->os_fillcount);
    sym_resolved = (refcount & OS_RESOLVED) != 0;
    sym_relocatable = (refcount & OS_RELOCATABLE) != 0;
    refcount &= OS_REFERENCEMASK;
    /* Process the item here */
    if (searchonly == FALSE || sym_resolved) {
      /* Add this symbol if resolved, */
      /* or if not resolved and searchonly is FALSE */
      oldsym = getsymbol(sym->os_name);
      if (sym_resolved) {
	/* The new symbol is resolved -- check old symbol */
	if (oldsym->s_fillcount & S_RESOLVED) {
	  /* Old symbol is resolved -- check duplicate symbol */
	  if ((relocatable && (oldsym->s_fillcount & S_RELOCATABLE)) ||
	      (oldsym->s_value != get_32(sym->os_value))) {
	    /* Duplicate entry error */
	    ERRMSG4(
		    "Duplicate entry \"%s\" in modules %s and %s",
		    oldsym->s_name,
		    mybasename(oldsym->s_module->m_filespec),
		    mybasename(lastmodule->m_filespec));
	  }
	} 
	/* if unresolved or if update mode, set new values */
	if (! (oldsym->s_fillcount & S_RESOLVED) || update) {
	  /* Old symbol is not resolved -- update it */
	  oldsym->s_value = get_32(sym->os_value);
	  oldsym->s_fillcount |= S_RESOLVED;
	  if (sym_relocatable) {
	    oldsym->s_value += lastmodule->m_start;
	    oldsym->s_fillcount |= S_RELOCATABLE;
	  } else {
	    /* Not relocatable -- set up symbol to match */
	    oldsym->s_fillcount &= ~ ((b_16) S_RELOCATABLE);
	  }
	  oldsym->s_module = lastmodule;
	}
      }
      if (searchonly == FALSE) {
	/* Not searchonly -- add references, too */
	while (refcount-- > 0) {
	  /* Get another fill reference */
	  refptr = (O_FILLREFPTR) getobject(0);
	  if (refptr != (O_FILLREFPTR) NULL)
	    addfillref(oldsym, refptr);
	}
	refcount++;
      }
    }
  }
}

MODULEPTR newmodule(void)
{
  /* newmodule() adds another module reference to the module list */

  MODULEPTR *mpptr;

  mpptr = &modulelist;

  while (*mpptr != (MODULEPTR) NULL)
    mpptr = &(*mpptr)->m_next;

    /* mpptr points to last link field */
  if ((*mpptr = (MODULEPTR) calloc(1,MODULESIZE)) == (MODULEPTR) NULL)
    error("error calloc'ing %ld bytes for new module", (long) MODULESIZE);
  DBG3("Address %ld: allocated %d bytes (newmodule)",(long)*mpptr,MODULESIZE);
  lastmodule = *mpptr;
  lastmodule->m_error = TRUE;	/* Cleared if no errors found */
  return lastmodule;
}


void read_object(int searchonly, int update)
{
  char *cp;
  OBJHEAD buffer;
  b_16 symcount;
  b_32 nibbles;
  b_32 start, mstart;
  MODULEPTR this_module, curr_module;
  int has_code, module_relocatable;

  /* Get a file specifier */
  cp = read_filespec();
  if (cp == (char *) NULL) {
    ERRMSG1("Missing file specifier");
    return;
  }
  setname(&curname, cp);
  curfile = openfile(curname, OBJECT_ENV, OBJECT_PATH, BIN_READONLY, NULL);
  if (curfile == (FILE *) NULL) {
    /* unable to open this file */
    ERRMSG2S("Problem reading file %s", curname);
    return;
  }
  this_module = newmodule();
  this_module->m_filespec = nameptr;
  if (fread((char *) &buffer, sizeof(buffer), 1, curfile) == 1
      && strncmp(buffer.id, SAT_ID, sizeof(buffer.id)) == 0) {
    /* successfully opened Saturn file */
    symcount = get_u16(buffer.symbols);
    nibbles = get_u32(buffer.nibbles);
    start = get_u32(buffer.start);
    has_code = ((nibbles > 0) && searchonly == FALSE);

    module_relocatable = (get_u8(buffer.absolute) == 0);
    if (module_relocatable) {
      /* Non-absolute start address -- OK to relocate it */
      if (locate_next)
	mstart = locate_addr;
      else
	mstart = next_addr;
    } else {
      /* Absolute start address */
      mstart = start;
      if (locate_next) {
	/* BOundary or LOcate in effect */
	if (force() || (locate_addr == start))
	  mstart = locate_addr;
	else {
	  /* Can't locate there */
	  ERRMSG2S("Unable to locate as specified.  File=%s",
		  mybasename(this_module->m_filespec));
	  (void) fclose(curfile);
	  return;
	}
      }
    }
    /* Finish setting up module information */
    this_module->m_start = mstart;
    this_module->m_length = has_code ? nibbles : 0;
    (void) strncpy(this_module->m_title, buffer.title,sizeof(buffer.title));
    (void) strncpy(this_module->m_date, buffer.date, sizeof(buffer.date));

    locate_next = FALSE;
    if (has_code && (mstart < start_addr)) {
      /* Invalid start address */
      ERRMSG2S("Start address error.  File=%s",
	      mybasename(this_module->m_filespec));
      (void) fclose(curfile);
      return;
    }
    if (has_code && module_relocatable == FALSE) {
      /* Output module is not relocatable if any code module is not */
      relocatable = FALSE;
    }

    if (has_code) {
      /* Check for overlap with any existing module */
      curr_module = modulelist;
      while (curr_module != this_module) {
	if (check_union(curr_module->m_start, curr_module->m_length,
			this_module->m_start, this_module->m_length)) {
	  /* overlap between modules! */
	  ERRMSG3("Warning:  Modules %s and %s are not disjoint",
		  mybasename(curr_module->m_filespec),
		  mybasename(this_module->m_filespec));
	}
	curr_module = curr_module->m_next;
      }

      /* Check for MAX_NIBBLES overflow */
      if (mstart + nibbles - 1 >= MAX_NIBBLES) {
	/* Module is too big */
	ERRMSG2S("No room for code.  File=%s",
		mybasename(this_module->m_filespec));
	(void) fclose(curfile);
	return;
      }

      if ((mstart + nibbles) > next_addr)
	next_addr = mstart + nibbles;
      start_specified = TRUE;
    }

    if (searchonly == FALSE) {
      /* Ready to read the code records */
      if (!fseek(curfile, (long) 256, 0))
	readcode((i_32) mstart, (i_32) nibbles);
      else {
	ERRMSG2S("Unable to seek to start of code.  File=%s",
		mybasename(this_module->m_filespec));
	(void) fclose(curfile);
	return;
      }
    }
    if (!fseek(curfile, (long) (((nibbles+511)/512+1)*256), 0)) {
      read_symbs(symcount, searchonly, update);

      /* Clear the error indication for this module */
      /* Note that any errors encountered cause a return which leaves */
      /* the m_error flag set [ m_error is set in newmodule() ] */

      this_module->m_error = FALSE;
    } else {
      ERRMSG2S("Unable to seek to start of symbols.  File=%s",
	      mybasename(this_module->m_filespec));
    }
  } else {
    /* Error reading from file */
    ERRMSG2S("File is not a Saturn object code file.  File=%s",
	    mybasename(this_module->m_filespec));
  }
  (void) fclose(curfile);
}

void do_relocate(void)
{
  read_object(FALSE, force_update);
}

void do_search(void)
{
  read_object(TRUE, force_update);
}

void do_upreloc(void)
{
  read_object(FALSE, TRUE);
}

void do_upsearch(void)
{
  read_object(TRUE, TRUE);
}

void do_softkey(void)
{
  if (force() || *softkeyfile == '\0')
    (void) strncpy(softkeyfile, ptr, sizeof(softkeyfile));
  else {
    ERRMSG1("Duplicate SOftkeys command");
  }
}

void do_start(void)
{
  i_32 temp_start;

  if (force() || ! start_specified) {
    temp_start = read_addr();
    if (temp_start >= 0)
      start_addr = temp_start; /* read_addr() produces any error msgs */
    next_addr = start_addr;
    relocatable = FALSE;
    start_specified = TRUE;
  } else {
    ERRMSG1("Duplicate STart command");
  }
}

void do_suppress(void)
{
  listflags &= ~ read_listflags();
}

void do_title(void)
{
  if (force() || *title == '\0')
    (void) strncpy(title, ptr, sizeof(title));
  else {
    ERRMSG1("Duplicate TItle command");
  }
}

void do_transfer(void)
{
  char *cp;
    
  /* Get a file specifier */
  cp = read_filespec();
  if (cp != (char *) NULL && strcmp(cp, "-") != 0) {
    setname(&inname, cp);
    if ( (infile != stdin) && infile)
      fclose(infile);
    if ((infile=fopen(inname, TEXT_READONLY)) == (FILE *) NULL)
      error("Unable to open %s as input file", inname);
  }
  else {
    setname(&inname, "[stdin]");
    infile = stdin;
  }

  DBG2("Input file name set to '%s'", inname);
  /* Handle prompts correctly */
  if (need_prompt) {
    need_prompt = FALSE;
  }
  if (isatty(fileno(infile))) {
    need_prompt = TRUE;
    openprompt();
  }
  errno = 0;	/* reset errno value (isatty() sets if not a tty) */
}

void do_tstamp(void)
{
#ifndef __MSDOS__
  char * format;
#endif

  if (read_symbol(timestamp) == (char *) NULL) {
    ERRMSG1("Missing symbol for TStamp");
  }
#ifndef __MSDOS__
  if ((format = read_string()) != (char *) NULL) {
    tstamp = ctime(&clocktime);
    tstamp[strlen(tstamp)-1] = '\0';	/* remove nl */
  }
#endif
}

void do_udefine(void)
{
  SYMBOLPTR symb;

  if (read_symbol(tempsymbol) != (char *) NULL) {
    if ((symb=findsymbol(tempsymbol)) != (SYMBOLPTR) NULL)
      symb->s_fillcount &= ~ ((b_16) S_RESOLVED);
  } else {
    ERRMSG1("Missing symbol for UDefine");
  }
}

void do_uflag(void)
{
  i_16 flagname = read_cmd();

  if (flagname != NUll && dropflag(flagname)) {
    ERRMSG1("No flags");
  }
}

void do_unresolved(void)
{
  unresolved_specified = FALSE;
  if (read_symbol(unresolved_sym) == (char *) NULL) {
    ERRMSG1("Missing symbol for UNresolved");
  } else {
    unresolved_specified = TRUE;
  }
}

void do_zero(void)
{
  i_32 addr1, addr2;

  if ((addr1=read_addr()) < 0 || (addr2=read_addr()) < 0)
    return;

  while (addr1 <= addr2) {
    putnib(addr1++, (int) 0);
  }
}

void docmd(i_16 cmd)
{
  switch (cmd) {
  case ABort:
  case xABort:
    do_abort();
    break;
  case ANnotate:
    do_annotate();
    break;
  case BOundary:
    do_boundary();
    break;
  case CHeck:
    do_check();
    break;
  case CKerm:
    do_crsum(KCRC_SUM);
    break;
  case ECrc:
    do_crc(ERNI_CRC);
    break;
  case KCrc:
    do_crc(KERMIT_CRC);
    break;
  case CRc:
    do_crc(LEWIS_CRC);
    break;
  case DBug:
    do_dbug();
    break;
  case DEfine:
    do_define();
    break;
  case ENd:
  case xENd:
    do_end();
    break;
  case FLag:
    do_flag();
    break;
  case FOrce:
    do_force();
    break;
  case HElp:
  case xHElp:
    do_help();
    break;
  case ID:
    do_id();
    break;
  case IF:
    do_if();
    break;
  case LIst:
    do_list();
    break;
  case LLu:
    do_llu();
    break;
  case LOcate:
    do_locate();
    break;
  case NExt:
    do_next();
    break;
  case OPtion:
    do_option();
    break;
  case OUtput:
    do_output();
    break;
  case PAtch:
    do_patch();
    break;
  case PUrge:
    do_purge();
    break;
  case RAnge:
    do_range();
    break;
  case RElocate:
    do_relocate();
    break;
  case SEarch:
    do_search();
    break;
  case SOftkey:
    do_softkey();
    break;
  case STart:
    do_start();
    break;
  case SUppress:
    do_suppress();
    break;
  case TItle:
    do_title();
    break;
  case TRansfer:
    do_transfer();
    break;
  case TStamp:
    do_tstamp();
    break;
  case UDefine:
    do_udefine();
    break;
  case UFlag:
    do_uflag();
    break;
  case UNresolved:
    do_unresolved();
    break;
  case URelocate:
    do_upreloc();
    break;
  case USearch:
    do_upsearch();
    break;
  case ZEro:
    do_zero();
    break;
  case Comment:
  case NUll:
    do_comment();
    break;
  default:
    ERRMSG1("Unrecognized command");
    break;
  }
}

void initialize(void)
{
  char *current_date;
  char *cp;

  if (time(&clocktime) < 0)
    error("%s","unable to read current time");
  current_date = ctime(&clocktime);
  tstamp = strncpy(date, current_date, 26);
  date[24] = '\0';	/* kill the trailing NL */

  initsymbols();		/* initialize symbol headers */

  /* Set default timestamp symbol */
  (void) strncpy(timestamp, TIMESTAMP_SYM, SYMBOL_LEN+1);

  /* Set default unresolved symbol */
  (void) strncpy(unresolved_sym, UNRESOLVED_SYM, SYMBOL_LEN+1);

  setname(&inname, "-");
  DBG2("Input file name set to '%s'", inname);
  infile = stdin;
  setname(&listname, "-");
  DBG2("List file name set to '%s'", listname);
  listfile = stdout;
  setname(&msgname, "-");
  DBG2("Message file name set to '%s'", msgname);
  msgfile = stderr;

  /* Set up default prompt string */
  promptstring = DEFAULT_PROMPT;

  /* See if the environment variable for prompt is set */
  if ((cp=getenv(PROMPT_ENV)) != (char *) NULL) {
    if ((promptstring=calloc(1, (unsigned)(strlen(cp)+1))) == (char *) NULL)
      error("error calloc'ing memory for prompt string (%s)", cp);
    DBG3("Address %ld: allocated %d bytes (prompt)", (long) promptstring,
	 strlen(cp)+1);
    (void) strcpy(promptstring, cp);
  }

  /* Set up define_module values */
  define_module.m_filespec = "Defined";
  define_module.m_start = 0;
  define_module.m_length = 0;
  (void) strcpy(define_module.m_title, "");
  (void) strcpy(define_module.m_date, date);
  define_module.m_error = FALSE;
  define_module.m_next = (MODULEPTR) NULL;
}

void sloadget_options(int argc, char **argv)
{
  int i;
  i_16 flagname;

  while ((i=getopt(argc,argv,"aD:d:ef:Hhl:M:m:No:p:Puw:")) != EOF) {
    switch (i) {
    case 'a':
      compile_absolute = TRUE;
      break;
    case 'D':	/* Define a symbol */
      (void) definesymbol(optarg);
      break;
    case 'd':	/* Select a debug file, enable debug mode */
      dbug(optarg);
      break;
    case 'e':	/* Error messages to [stderr] */
      errors_to_msgfile = TRUE;
      break;
    case 'f':	/* Set flags */
      ptr = optarg;
      flagname = read_cmd();
      if (!addflag(flagname)) {
	ERRMSG1("Too many flags");
      }
      break;
    case 'H':	/* Write only binary code to object file */
      codeonly = TRUE;
      hexcode = FALSE;
      break;
    case 'h':	/* Write (ASCII) hex code to object file */
      codeonly = TRUE;
      hexcode = TRUE;
      break;
    case 'l':
      setname(&listname, optarg);
      DBG2("List file name set to '%s'", optarg);
      listing = TRUE;
      break;
    case 'M':	/* Set message destination (append) */
    case 'm':	/* Set message destination (overwrite) */
      setname(&msgname, optarg);
      DBG2("Message file name set to '%s'", optarg);
      msg_append = (i == 'M');
      break;
    case 'N':	/* Disable listing */
      listing = FALSE;
      break;
    case 'o':	/* Set object file name */
      setname(&objname, optarg);
      DBG2("Object file name set to '%s'", optarg);
      break;
    case 'p':	/* Set page length */
      if (sscanf(optarg, "%ld", &pagelength) != 1) {
	ERRMSG2("Non-numeric page length '%s'",optarg);
	pagelength = 60;
      }
      if (pagelength < 4) {
	ERRMSG2("Page length must be at least 4 (%ld)", pagelength);
	pagelength = 60;
      }
      break;
    case 'P':	/* Prompt for input */
      need_prompt = TRUE;
      break;
    case 'u':	/* Force update mode */
      force_update = TRUE;
      break;
    case 'w':	/* Page width */
      if (sscanf(optarg, "%ld", &pagewidth) != 1 || pagewidth < 30) {
	ERRMSG2("Invalid page width '%s' (minimum width is 30)",optarg);
	pagewidth = 80;
      }
      break;
    case '?':	/* bad option */
    default:
      ++errorcount;
      break;
    }
  }
  /* Check for input file name on command line */
  if (optind + 1 == argc) {
    /* input file specified on command line */
    setname(&inname, argv[optind]);
    DBG2("Input file name set to '%s'", argv[optind]);
  }
}

void open_files(void)
{
  if (strcmp(inname, "-") != 0) {
    if ((infile=fopen(inname, TEXT_READONLY)) == (FILE *) NULL)
      error("unable to open %s as input file", inname);
  } else {
    setname(&inname, "[stdin]");
    infile = stdin;
    DBG2("Input file name set to '%s'", inname);
  }

  if (strcmp(msgname, "-") != 0) {
    msgfile = fopen(msgname, msg_append ? TEXT_APPEND : TEXT_WRITE);
    if (msgfile == (FILE *) NULL)
      error("unable to open %s as message file", msgname);
  } else {
    msgfile = stderr;
    setname(&msgname, "[stderr]");
    DBG2("Message file name set to '%s'", msgname);
  }

  if (listing) {
    if (strcmp(listname, "-") != 0) {
      if ((listfile=fopen(listname, TEXT_WRITE)) == (FILE *) NULL)
	error("unable to open %s as list file", listname);
    } else {
      listfile = stdout;
      setname(&listname, "[stdout]");
      DBG2("List file name set to '%s'", listname);
    }
  }
  if (isatty(fileno(infile)))
    need_prompt = TRUE;

  errno = 0;
  if (need_prompt)
    openprompt();
}

#ifdef dll
sloadmain(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
#ifdef MEMSTATS
  char * StartBrk, * EndBrk, * sbrk();

  StartBrk = sbrk( 0L );
#endif
  progname = argv[0];

  initialize();
  sloadget_options(argc, argv);

  if (errorcount) {
    sloadusage();
    exit(-1);
  }

  open_files();	/* Files specified by initialize() and get_options() */

  while (cmd_mode) {
    if (need_prompt)
      prompt(promptstring);

    if ((ptr=sloadgetline(line,infile)) != (char *) NULL) {
      docmd(read_cmd());
    } else {
      /* Do ENd command with no options */
      ptr = "";
      do_end();
    }
  }
  if (abortf) {
    /* Quit immediately! */
    LISTIT2("Saturn Loader Aborted (%ld)", (long) abortcode);
    exit(abortcode);
  }

  if (! suppress_resolve)
    resolve_refs();

  compute_checksums(cklist);

  compute_crcs(crclist);

  if (objname != (char *) NULL)
    write_output();

  write_list();

  LISTIT2("%sEnd of Saturn Loader Execution", promptstring);

#ifdef TUNE
  (void) fputs((char*) symstats(), stderr);
#endif
#ifdef MEMSTATS
  EndBrk = sbrk( 0L );
  fprintf(stderr,"Start break value - %ld\n", StartBrk );
  fprintf(stderr,"End break value   - %ld\n", EndBrk );
  fprintf(stderr,"Bytes Allocated   - %ld\n", EndBrk - StartBrk );
#endif
  exit((int) errorcount);
  /*NOTREACHED*/
#ifdef dll
  return (0);
#endif
}
