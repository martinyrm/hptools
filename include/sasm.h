#ifndef _SASM_H_
#define _SASM_H_

#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef	HAVE_UNISTD_H
#  include <unistd.h>
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
#include "debug.h"
#include "envfile.h"

#include "convert.h"

extern char	version[];


#ifdef	__MSDOS__

/* Default paths for various commands */
#define CHARMAP_PATH ";"
#define INCLUDE_PATH ";"
#define RDSYMB_PATH ";"

#else	/*MSDOS*/

/* Default paths for various commands */
#define CHARMAP_PATH ":"
#define INCLUDE_PATH ":"
#define RDSYMB_PATH ":"

#endif	/*MSDOS*/

#define FF		'\014'
#define SEPARATORS	" \t"
#define OPC_NAME	"sasm.opc"
#define OPCODETAG	"Opcodes-4:%d\n"

/* Environment variable names for various commands */
#define OPC_ENV		"SASM_LIB"
#define CHARMAP_ENV	"SASM_CHARMAP"
#define INCLUDE_ENV	"SASM_INCLUDE"
#define RDSYMB_ENV	"SASM_RDSYMB"

/* Equates for listflags bits */
#define LIST_CODE	1
#define LIST_MACRO	2
#define LIST_INCLUDE	4
#define LIST_PSEUDO	8
#define LIST_ALL	(LIST_CODE | LIST_MACRO | LIST_INCLUDE | LIST_PSEUDO)
#define LIST_WOULD	16
#define LIST_FORCE	32
#define LIST_SUPPRESS	64

typedef struct macro {
    char *m_line;	/* Macro line contents */
    struct macro *m_next; /* Pointer to next macro line (NULL = end of macro) */
} MACRO, *MACROPTR;
#define MACROSIZE sizeof(struct macro)


/* op_operand */
#define OPD_NONE 0
#define OPD_FS 1
#define OPD_D_OR_FS 2
#define OPD_DIGIT_EXPR2 3
#define OPD_FS2 4
#define OPD_DIGIT_EXPR 5
#define OPD_EXPRESSION 6
#define OPD_RELATIVE_JUMP 7
#define OPD_ABSOLUTE_JUMP 8
#define OPD_NIBHEX 9
#define OPD_245_HEX 10
#define OPD_LCHEX 11
#define OPD_ASC 12
#define OPD_STRING 13
#define OPD_SYMBOL 14
#define OPD_FLAG 15
#define OPD_F_AND_DIGIT 16
#define OPD_PF_AND_DIGIT 17
#define OPD_PARTIAL_FS 18
#define OPD_STR 19		/* Used for the instructions LCSTR and LASTR */
#define OPD_LENASC 20		/* Used for the instructions ASC(m) and ASCM(m) */
#define OPD_LENHEX 21		/* Used for the instructions HEX(m) and HEXM(m) */
#define OPD_BIN 22

/* op_flags */
#define A_SPEC 1
#define MACRO_PASS2 1
#define B_SPEC 2
#define TEST 4
#define GOYES 8
#define PSEUDO_OP 16
#define MACRO 32
/* CPU level (0=1LF2, 1=1LK7, 2=1LR2 [Lewis], 3=????) */
#define LEVELBITS (128+64)
#define LEVEL0 0
#define LEVEL1 64
#define LEVEL2 128
#define LEVEL3 (128+64)

#define COMBOS 0x100
#define MASD	0x200

/* op_class for PSEUDO_OP */
#define PS_ABS		0
#define PS_REL		1
#define PS_BSS		2
#define PS_EQU		3
#define PS_RDSYMB	4
#define PS_END		5
#define PS_EQUALS	6
#define PS_UNDEF	7

#define PS_IF		10
#define PS_IFCOND	11
#define PS_IFSTRCOND	12
#define PS_ABASE	13
#define PS_ALLOC	14

/* C_* are a bit map (1 for <, 2 for =, 4 for >, 8 for any test) */
#define C_LT		1
#define C_EQ		2
#define C_GT		4
#define C_NE		(C_LT | C_GT)
#define C_GE		(C_GT | C_EQ)
#define C_LE		(C_LT | C_EQ)

#define PS_IFDEF	15
#define PS_IFNDEF	16
#define PS_IFOPC	17
#define PS_IFNOPC	18
#define PS_IFPASS1	19
#define PS_IFPASS2	20
#define PS_IFCARRYSET	21
#define PS_IFCARRYCLR	22
#define PS_IFANYCARRY	23
#define PS_IFREACHED	24

#define PS_ELSE		38
#define PS_ENDIF	39

#define PS_TITLE	40
#define PS_STITLE	41
#define PS_EJECT	42
#define PS_LIST		43
#define PS_LISTM	44
#define PS_UNLIST	45
#define PS_SETLIST	46
#define PS_CLRLIST	47
#define PS_LISTALL	48

#define PS_MACRO	50
#define PS_ENDM		51
#define PS_EXITM	52
#define PS_INCLUDE	53
#define PS_CHARMAP	54

#define PS_SETFLAG	60
#define PS_CLRFLAG	61
#define PS_SETCARRY	62
#define PS_CLRCARRY	63
#define PS_NOTREACHED	64
#define PS_JUMP		65

#define PS_MESSAGE	70


#define PS_FOR		71
#define PS_NEXT		72
#define PS_EXITF	73

#define PS_UP		74
#define PS_EXIT		75
#define PS_UPC		76
#define PS_EXITC	77
#define PS_SKIP		78
#define PS_SKELSE	79
#define PS_SKIPCLOSE	80
#define PS_INCLOB	81

/* The op_carrystate field is a bit field showing the current carry state */
/* The low nibble is the OR field, the high nibble is the AND field  e.g. */
/*   (previous_state | value) & (value >> 4) & 0xF gives the new state.	  */
/* In each nibble, the least significant bit is the Carry Clear bit, and  */
/*  the next bit is the Carry Set bit.					  */
#define OP_NC	1
#define OP_C	2
#define OP_NOCHGCARRY	((OP_C | OP_NC) << 4)
#define OP_NOSETCARRY	(OP_NC << 4)
#define OP_NOCLRCARRY	(OP_C << 4)
#define OP_CLRCARRY	((OP_NC << 4) | OP_NC)
#define OP_SETCARRY	((OP_C << 4) | OP_C)
#define OP_CHGCARRY	(((OP_C | OP_NC) << 4) | OP_C | OP_NC)
#define OP_NOTREACHED	0

typedef struct optable {
    char op_mnem[MNEMONIC_LEN+1];
    i_8 op_operand;
    b_16 op_flags;
    i_16 op_length;		/* base length, non-special fields */
    char op_string[10+1];	/* longest opcodes are 10 nibbles long */
    char alt1_op_string[10+1];	/* alternative opcode for A field select */
    char alt2_op_string[10+1];	/* alternative opcode for B field select */
    i_8 op_fs_nib;		/* location of FS nibble */
    i_8 op_offset;
    i_8 op_size;
/* op_bias: for STRING, set high bit of last nibble if <>0 */
    i_32 op_bias;
/* op_class: for PSEUDO_OP, this is PS_type */
    b_8 op_class;
    b_8 op_subclass;
    b_8 op_Afield;
    b_8 op_Bfield;
    b_8 op_carrystate;
    b_8 op_spare[3];
    MACROPTR op_macroptr;
    struct optable *op_left, *op_right; /* child pointers */
} OPTABLE, *OPTABLEPTR;

#define OPTABLESIZE sizeof(struct optable)

typedef struct {
  union TYPE {
    char value[OPCODE_LEN+1];
    char *valuebyte;
  } valuefield;
  b_32 length;
  int variable;
} OPCODE, *OPCODEPTR;

typedef struct lineref {
    b_32 l_line;
    struct lineref *l_next;
} LINEREF, *LINEREFPTR;
#define LINEREFSIZE sizeof(struct lineref)

typedef struct fillref {
    i_32 f_address;
    i_32 f_bias;
    b_8 f_class;
    b_8 f_subclass;
    i_16 f_length;
    struct fillref *f_next;
} FILLREF, *FILLREFPTR;
#define FILLREFSIZE sizeof(struct fillref)

#define S_EXTERNAL 1
#define S_RESOLVED 2
#define S_RDSYMB 4
#define S_ENTRY 8		/* Entry point (=) */
#define S_RELOCATABLE 16
#define S_SETPASS2 32		/* Already defined during pass 2 */

typedef struct symbol {
    char s_name[SYMBOL_LEN+1];
    i_32 s_value;
    i_8 s_red;			/* link is "red" (red-black tree info) */
    b_8 s_flags;
    LINEREFPTR s_lineref;
    b_16 s_fillcount;
    FILLREFPTR s_fillref;
    struct symbol *s_left, *s_right;
} SYMBOL, *SYMBOLPTR;
#define SYMBOLSIZE sizeof(struct symbol)

typedef struct expression {
    i_16 e_extcount;
    i_16 e_relcount;
    char e_name[SYMBOL_LEN+2];
    i_32 e_value;
} EXPR, *EXPRPTR;
#define EXPRLEN sizeof(struct expression)

#define DO_STACK	3
#define END_INCLUDE	2
#define START_INCLUDE	1
#define NO_IM		0
#define START_MACRO	-1
#define END_MACRO	-2

typedef struct imacro {
    int im_type;
    i_32 im_line;
    char im_buffer[LINE_LEN+1];
    long im_addr;
    char *im_filename;
    int im_flagptr;
    int im_listflags;
} IMACRO, *IMACROPTR;

#define FS_P 0
#define FS_WP 1
#define FS_XS 2
#define FS_X 3
#define FS_S 4
#define FS_M 5
#define FS_B 6
#define FS_W 7
#define FS_A 15
#define FS_BAD -1

/* Support for the previous GNU Tools */

struct loop {
  char	*lp_filename;
  i_32	lp_fpos;
  char	lp_label[MNEMONIC_LEN + 1];
  i_32	lp_value;
  i_32	lp_end;
  i_32	lp_step;
};

/* Type Definition */
union SFC { 
  O_SYMBOL s;
  O_FILLREF f;
  char c[SYMRECLEN];
};

typedef struct {
  int c1;
  int c2;
  int c3;
} MSTACK;

/* Function Declaration */

/* sasm.c */

void	putlist(char *);
void	testfunc(FILE *, b_32);
#define doheader sasmdoheader
void	doheader(void);
void	sasmusage(void);
#define errmsg sasmerrmsg
void	errmsg(char *);
void	warningmsg(char *);
void	sasmputnib(int);
void	flushnibs(void);
void	writeheader(void);
void	putobject(union SFC *);
#define flushobject sasmflushobject
void	flushobject(void);
void	dosymbol(SYMBOLPTR);
void	writesymbols(void);
void	writeopcode(b_32, char *);
int	iscomment(void);
int	isfield(void);
#define getfield sasmgetfield
int	getfield(char *, int, char *);
int	isfield2(void);
int	getfield2(char *, int, char *);
#define skipwhite sasmskipwhite
void	skipwhite(void);
char	*sasmgetline(char *, FILE *);
#define copyline sasmcopyline
void	copyline(char *, char *, int);
void	putsymbolref(SYMBOLPTR);
void	listsymbols(void);
void	liststatistics(void);
#define definesymbol sasmdefinesymbol
void	definesymbol(char *);
void	sasmget_options(int, char **);
void	copy_sys(SYMBOLPTR);
void	dopptr(void);
void	get_macroline();
void	imget();
char	*makename(char *, char *, char *);
void	setup_formats(void);


/* external variables shared */

extern int	optind;

/* symbols.c */
extern SYMBOLPTR	s_root, sys_root, gg, g, f;


/* opcode.c */
extern char	symname[LINE_LEN+1];
extern int	f_ptr;
extern OPTABLEPTR	entry;
extern int	carry;

/* envfile.c */
extern char	*nameptr;

/* sasm.c */
extern int e_ptr, o_ptr;

extern int	column, listflags, listthisline, listaddress, stdinread;
extern i_32	listcount;
extern int	reading, begin_pseudos_ok, rel_expressions_ok, quote_ok;
#define pagelength sasmpagelength
extern int	pagelength, lineofpage;
extern char	title[], subtitle[], label[], line[], prog_symbol[];
extern char	*ptr, *linestart;
extern unsigned char	charset[];
extern i_32	pc, codestart, abase;
extern int	have_start_addr;
extern int	code_level;
extern int	absolute, pass1, pass2, pass1_listing;
extern unsigned char	flag[];
extern int	addingmacro;
extern int	imtype, impending;
extern int	assembling;
extern FILE	*include;

extern int	combos, masd, sasm_only, no_bin;

extern FILE	*opfile;
extern char	*opname;

extern char	*inname, *dbgname, *nameptr;
extern i_32	loops[LOOP_STACK_MAX][3];
extern FILE	*dbgfile, *infile, *symfile;
extern int	insideloop;

extern i_32	currentline;

extern int	label_index_m2, label_index_m1, label_index_p1, label_index_p2;

extern int	index_c1, index_c2, index_c3, level_stack;
extern MSTACK	masd_stack[MAX_STACK];

extern int	expr_resolved, expr_unknown;
extern int	blanks_in_expr;

extern int	force_global, absolute;


/* opcode.c */
extern int	was_test;

extern	int	getopt(int, char * const [], const char *);

#endif
