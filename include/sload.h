#ifndef _SLOAD_H_
#define _SLOAD_H_

#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef	HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef __MSDOS__
#  include <io.h>
#endif

#ifdef	HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef __FREE__
# include <malloc.h>
#endif
#include <time.h>

#include <ctype.h>

#include <errno.h>

#include "saturn.h"

#include "envfile.h"
#include "debug.h"
#include "convert.h"

#ifdef	__MSDOS__

/* Default paths for various commands */
#define OBJECT_PATH ""
#define OUTPUT_PATH ""
#define LIST_PATH ""

#else	/*MSDOS*/

/* Default paths for various commands */
#define OBJECT_PATH ""
#define OUTPUT_PATH ""
#define LIST_PATH ""

#endif	/*MSDOS*/

#define OBJECT_ENV	"SLOAD_OBJECT"
#define OUTPUT_ENV	"SLOAD_OUTPUT"
#define LIST_ENV	"SLOAD_LIST"

#define DEFAULT_PROMPT	"/SLOAD:  "
#define PROMPT_ENV	"SLOAD_PROMPT"

#define TIMESTAMP_SYM	"@TIME@"
#define UNRESOLVED_SYM	""

#define SEPARATORS	" \t"
#define CSEPARATORS	" \t,"

/* Read size for CRC */
#define READSIZE	16

/* Equates for listflags bits */
#define LIST_SYMBOLS	1
#define LIST_LONGXREF	2
#define LIST_REFERENCES	4
#define LIST_SHORTENABLE	8
#define LIST_SUMMARY	16
#define LIST_CODE	32
#define LIST_UNRESOLVED	64
#define LIST_XREF	128
#define LIST_ALL	(LIST_SYMBOLS | LIST_LONGXREF | LIST_REFERENCES | LIST_SHORTENABLE | LIST_SUMMARY | LIST_CODE | LIST_UNRESOLVED | LIST_XREF)

#define OPTION_UNDEFINED	0
#define OPTION_CODE	1
#define OPTION_SYMBOLS	2
#define OPTION_REFS	4
#define OPTION_RELOCATABLE	(OPTION_CODE | OPTION_SYMBOLS | OPTION_REFS)
#define OPTION_NOREFS	(OPTION_CODE | OPTION_SYMBOLS)

#define LEWIS_CRC	0
#define KERMIT_CRC	1
#define ERNI_CRC	2
#define KCRC_SUM	3
#define CRC_TYPE	int

typedef struct module {
    char *m_filespec;
    i_32 m_start;
    i_32 m_length;
    char m_title[40];
    char m_date[26];
    int m_error;
    struct module *m_next;
} MODULE, *MODULEPTR;
#define MODULESIZE sizeof(struct module)

typedef struct fillref {
    i_32 f_address;
    i_32 f_bias;
    b_8 f_class;
    b_8 f_subclass;
    i_16 f_length;
    MODULEPTR f_module;		/* Pointer to infor for defining module */
    struct fillref *f_next;
} FILLREF, *FILLREFPTR;
#define FILLREFSIZE sizeof(struct fillref)

typedef struct symbol {
    struct symbol *s_left, *s_right;	/* to make splay tree */
    MODULEPTR s_module;		/* Pointer to info for defining module */
    FILLREFPTR s_fillref;
    FILLREFPTR *s_filltail;	/* do a quick reference queue */
    i_32 s_value;
    b_16 s_fillcount;
    char s_name[SYMBOL_LEN+1];
} SYMBOL, *SYMBOLPTR;
#define SYMBOLSIZE sizeof(struct symbol)

typedef struct crc {
    i_32 crc_start;
    i_32 crc_offset;
    i_32 crc_reads;
    i_32 crc_sectors;
    struct crc *crc_next;
    CRC_TYPE crc_type;
} CRC, *CRCPTR;
#define CRCSIZE sizeof(struct crc)

typedef struct checksum {
    char ck_symbol[SYMBOL_LEN+1];
    i_32 ck_start;
    i_32 ck_end;
    struct checksum *ck_next;
} CHECKSUM, *CHECKSUMPTR;
#define CHECKSUMSIZE sizeof(struct checksum)

/* Macros for output from sload (and error messages) */
extern void	listit(char *);
//  #define errmsg loaderrmsg								// wgg allow these messages to go out
extern void	errmsg(char *);

#define MSG1(x) { \
    if (fprintf(msgfile, x) < 0 || putc('\n', msgfile) == EOF) \
	error("error writing to message file (%s)", msgname); \
}
#define MSG2(x,y) { \
    if (fprintf(msgfile, x, y) < 0 || putc('\n', msgfile) == EOF) \
	error("error writing to message file (%s)", msgname); \
}
#define MSG3(x,y,z) { \
    if (fprintf(msgfile,x,y,z) < 0 || putc('\n', msgfile) == EOF) \
	error("error writing to message file (%s)", msgname); \
}
#define LISTIT1(z) listit(z)
#define LISTIT2(y,z) { \
    (void) sprintf(string, y, z); \
    listit(string); \
}
#define LISTIT3(x,y,z) { \
    (void) sprintf(string, x, y, z); \
    listit(string); \
}
#define LISTIT4(w,x,y,z) { \
    (void) sprintf(string, w, x, y, z); \
    listit(string); \
}
#define LISTIT5(v,w,x,y,z) { \
    (void) sprintf(string, v, w, x, y, z); \
    listit(string); \
}
#define LISTIT6(u,v,w,x,y,z) { \
    (void) sprintf(string, u, v, w, x, y, z); \
    listit(string); \
}
#define LISTIT7(t,u,v,w,x,y,z) { \
    (void) sprintf(string, t, u, v, w, x, y, z); \
    listit(string); \
}
#define ERRMSG1(z) errmsg(z)
#define ERRMSG2(y,z) { \
    (void) sprintf(string, y, z); \
    errmsg(string); \
}
#define ERRMSG2S(y,z) { \
    if (strpbrk(z," \t") == NULL) { (void) sprintf(string, y, z); } \
    else { (void) sprintf(string2,"\"%s\"",z); (void) sprintf(string, y, string2); } \
    errmsg(string); \
}

#define ERRMSG3(x,y,z) { \
    (void) sprintf(string, x, y, z); \
    errmsg(string); \
}
#define ERRMSG4(w,x,y,z) { \
    (void) sprintf(string, w, x, y, z); \
    errmsg(string); \
}
#define ERRMSG5(v,w,x,y,z) { \
    (void) sprintf(string, v, w, x, y, z); \
    errmsg(string); \
}
#define ERRMSG6(u,v,w,x,y,z) { \
    (void) sprintf(string, u, v, w, x, y, z); \
    errmsg(string); \
}
#define ERRMSG7(t,u,v,w,x,y,z) { \
    (void) sprintf(string, t, u, v, w, x, y, z); \
    errmsg(string); \
}

/* Symbols for fields of s_fillcount */
#define S_RESOLVED OS_RESOLVED
#define S_RELOCATABLE OS_RELOCATABLE
#define S_REFERENCEMASK OS_REFERENCEMASK
#define NUll (0)

#define ABort (('A'<<8)+'B')
#define xABort (('/'<<8)+'A')
#define ALl (('A'<<8)+'L')		/* Used by LIst and SUppress */
#define ANnotate (('A'<<8)+'N')
#define BOundary (('B'<<8)+'O')
#define CHeck (('C'<<8)+'H')
#define CKerm (('C'<<8)+'K')
#define COde (('C'<<8)+'O')		/* Used by LIst, SUppress, and OPtion */
#define CRc (('C'<<8)+'R')
#define Comment (('*'<<8)+'*')
#define DBug (('D'<<8)+'B')
#define DEfine (('D'<<8)+'E')
#define ECrc (('E'<<8)+'C')
#define ENd (('E'<<8)+'N')
#define xENd (('/'<<8)+'E')
#define FLag (('F'<<8)+'L')
#define FOrce (('F'<<8)+'O')
#define HElp (('H'<<8)+'E')
#define xHElp (('?'<<8)+'?')
#define ID (('I'<<8)+'D')
#define IF (('I'<<8)+'F')
#define KCrc (('K'<<8)+'C')
#define LIst (('L'<<8)+'I')
#define LLu (('L'<<8)+'L')
#define LOcate (('L'<<8)+'O')
#define LXref (('L'<<8)+'X')		/* Used by LIst and SUppress */
#define NExt (('N'<<8)+'E')
#define NOresolve (('N'<<8)+'O')	/* Used by ENd */
#define NOt (('N'<<8)+'O')
#define NRefs (('N'<<8)+'R')
#define OPtion (('O'<<8)+'P')
#define OUtput (('O'<<8)+'U')
#define PAtch (('P'<<8)+'A')
#define PUrge (('P'<<8)+'U')
#define RAnge (('R'<<8)+'A')
#define REferences (('R'<<8)+'E')	/* Used by LIst and SUppress */
#define RElocate (('R'<<8)+'E')		/* Also used by OPtion (relocatable) */
#define REsolve (('R'<<8)+'E')		/* Used by ENd */
#define SEarch (('S'<<8)+'E')
#define SHortenable (('S'<<8)+'H')	/* Used by LIst and SUppress */
#define SOftkey (('S'<<8)+'O')
#define STart (('S'<<8)+'T')
#define SUmmary (('S'<<8)+'U')		/* Used by LIst and SUppress */
#define SUppress (('S'<<8)+'U')
#define SYmbols (('S'<<8)+'Y')		/* Used by LIst, SUppress, and OPtion */
#define TItle (('T'<<8)+'I')
#define TRansfer (('T'<<8)+'R')
#define TStamp (('T'<<8)+'S')
#define UDefine (('U'<<8)+'D')
#define UFlag (('U'<<8)+'F')
#define UNresolved (('U'<<8)+'N')	/* Also used by LIst and SUppress */
#define URelocate (('U'<<8)+'R')
#define USearch (('U'<<8)+'S')
#define XRef (('X'<<8)+'R')		/* Used by LIst and SUppress */
#define ZEro (('Z'<<8)+'E')




/* Data declarations for the loader */
extern b_16 flag[NUMBER_OF_FLAGS];	/* Flag array */
extern char line[LINE_LEN];		/* Current input line */
extern char *ptr;			/* Global line pointer */
extern char *inname;		/* Input file name */
extern char *objname;		/* Object file name */
extern char *msgname;		/* Message file name */
extern char *listname;		/* Listing file name */
extern char *promptname;	/* Prompt file name */
extern char *curname;		/* Current object file name */
				/* File streams (correspond to *name above) */
extern FILE *infile, *objfile, *msgfile, *listfile, *promptfile, *curfile;
extern int cmd_mode;		/* Processing commands from input file */
extern int abortcode;		/* Code to return to caller */
extern int need_prompt;	/* Prompt for input from terminal */
extern char *promptstring;	/* Prompt string for input from terminal */
#define msg_append sloadmsg_append
extern int msg_append;		/* Append message to file */
extern char *progname;			/* Current program name */
extern int forceflag;		/* FOrce command specified */
extern i_32 next_addr;		/* Next address */
extern i_32 start_addr;		/* Start address */
extern int start_specified;	/* STart command specified */
extern char unresolved_sym[SYMBOL_LEN+1]; /* Unresolved symbol name */
extern i_32 unresolved_value;	/* Unresolved symbol value (filled by resolve)*/
extern int unresolved_specified;/* Unresolved symbol name provided by user */
extern char timestamp[SYMBOL_LEN+1];	/* TimeStamp symbol name */
extern char tempsymbol[SYMBOL_LEN+1];	/* Temporary use symbol name */
extern b_16 listflags;		/* Listing flags */
extern i_32 IDvalue;		/* ID value for object file */
extern int locate_next;	/* [LO or BO] specified for next module */
extern i_32 locate_addr;		/* Address for locate if locate_next is true */
extern int suppress_resolve;	/* Resolve references after ENd command */
extern i_32 errorcount;		/* Count of errors encountered */
extern MODULEPTR lastmodule;	/* pointer to the most recent module */
extern MODULEPTR modulelist;	/* pointer to module list */
extern MODULEPTR refmodule;		/* pointer to module (used by *xref) */
extern MODULEPTR symbmodule;		/* pointer to symbol (used by *xref) */
extern CRCPTR crclist;		/* pointer to CRC list */
extern CHECKSUMPTR cklist;	/* pointer to checksum list */
extern int firstletter;		/* First letter of last symbol printed */

extern MODULE define_module;		/* Module for Defined */

/* Global symbols used to resolve references (resolve_refs and func_resolve) */
extern i_32 unres_value;
extern i_32 nibs_saved;

extern int errors_to_msgfile;	/* Copy all error messages to msgfile */
#define pagewidth sloadpagewidth
#define pagelength sloadpagelength
extern long pagelength, lineofpage, page, pagewidth;
#define listing sloadlisting
extern int listing;
#define codeonly sloadcodeonly
#define hexcode sloadhexcode
extern int codeonly, hexcode;
extern int code_option;
extern time_t clocktime;			/* time reference */
extern char date[26];			/* Date in object file */
extern char *tstamp;			/* time stamp info */
extern char title[40];			/* Object file title */
extern char softkeyfile[20];		/* Soft keys filename */
extern int relocatable;		/* Object file is relocatable */
extern int force_update;	/* Duplicate entries resolve to last */

extern int compile_absolute; /* Flag not to fill with zero until the beginning */
extern char string[LINE_LEN];		/* Temporary use storage (LISTITx, ERRMSGx) */
extern char string2[LINE_LEN];		/* Temporary use storage (LISTITx, ERRMSGx) */

extern char version[];
extern char print_vers[];
extern char *print_version;

#define tohex sloadtohex
extern char tohex[];


extern int debug;		/* debug control flag (set by -d) */
extern FILE *dbgfile;			/* debug file stream */
extern char *dbgname;			/* debug file name */

extern void sloadusage(void);
#define doheader sloaddoheader
extern void doheader(void);
extern void listit(char *);
extern void newpage(void);
extern void errmsg(char *);
extern void prompt(char *);
extern void openprompt(void);
extern int check_union(i_32, i_32, i_32, i_32);
extern i_32 my_abs(i_32);
extern int onenibble(i_32, int);
#define fromhex sloadfromhex
extern int fromhex(char);
extern char *mybasename(char *);
extern char *fileaddr(i_32, MODULEPTR);
extern char *addrfileaddr(i_32, MODULEPTR);
extern void fillin_ref(i_32, i_32, int);
extern void func_resolve(SYMBOLPTR);
extern void resolve_refs(void);
extern void compute_checksums(CHECKSUMPTR);
extern void compute_crcs(CRCPTR);
extern void list_summary(void);
extern void func_xref(SYMBOLPTR);
extern void print_longsymb(SYMBOLPTR);
extern void print_longref(SYMBOLPTR, FILLREFPTR);
extern void func_longxref(SYMBOLPTR);
extern void func_symbrefs(SYMBOLPTR);
extern void func_unresolved(SYMBOLPTR);
extern void list_xref(void);
extern void list_longxref(void);
extern void list_references(void);
extern void list_symbols(void);
extern void list_unresolved(void);
extern void list_code(void);
extern void write_list(void);
extern int findflag(i_16);
extern int addflag(i_16);
extern int dropflag(i_16);
extern int force(void);
extern void do_abort(void);
extern void do_annotate(void);
extern void do_boundary(void);
extern void do_check(void);
extern void do_comment(void);
extern void do_crsum(CRC_TYPE);
extern void do_crc(CRC_TYPE);
extern void dbug(char *);
extern void do_dbug(void);
extern void do_define(void);
extern void do_end(void);
extern void do_flag(void);
extern void do_force(void);
extern void do_help(void);
extern void do_id(void);
extern void do_if(void);
extern b_16 read_listflags(void);
extern void do_list(void);
extern void do_llu(void);
extern void do_locate(void);
extern void do_next(void);
extern void do_option(void);
extern void do_output(void);
extern void do_patch(void);
extern void do_purge(void);
extern void do_range(void);
#define getobject sloadgetobject
extern O_SYMBOLPTR getobject(int);
extern void read_symbs(i_16, int, int);
extern MODULEPTR newmodule(void);
extern void read_object(int, int);
extern void do_relocate(void);
extern void do_search(void);
extern void do_upreloc(void);
extern void do_upsearch(void);
extern void do_softkey(void);
extern void do_start(void);
extern void do_suppress(void);
extern void do_title(void);
extern void do_transfer(void);
extern void do_udefine(void);
extern void do_uflag(void);
extern void do_unresolved(void);
extern void do_zero(void);
extern void docmd(i_16);

extern void initialize(void);

extern void sloadget_options(int, char **);

extern void open_files(void);

#endif
