#include "sasm.h"

#include "opcode.h"
#include "expr.h"
#include "symbols.h"

#ifdef dll
#include "dllfile.h"
#endif

OPCODE	this_opcode;
int	expr_resolved, expr_unknown;
int	carry;

int	offset;
b_8	class_id;
b_16	flags;

OPTABLEPTR	optable_root = NULL;
OPTABLEPTR	macro_root = NULL;
OPTABLEPTR	entry;
OPTABLEPTR	macroentry;

static OPCODE	null_opcode = { { "" } , 0, FALSE};

FILE *symfile;
char symname[LINE_LEN+1];
MACROPTR *lastmacro = NULL;	/* Pointer to pointer to last macro link */

static EXPR abs_expr = {1, 0, "*00000", 0};
char tohex[] = "0123456789ABCDEFabcdef";

int	was_test = FALSE;


/* conditionals should probably be done through calloc() */
char f_stack[MAXCONDITIONALS][SYMBOL_LEN+1];
char f_elsedone[MAXCONDITIONALS];
int f_ptr = -1;

struct loop	*fnentry = NULL;

void faddlabel(char *label, i_32 value, int flags)
{
  if (!pass2 && assembling && !addingmacro) {
    xaddlabel(label, value, flags, FALSE);
  }
}


int pushcond(void)
{
  if (++f_ptr >= MAXCONDITIONALS) {
    errmsg("Conditional assembly stack overflow");
    --f_ptr;
    return FALSE;
  }
  (void) strncpy(f_stack[f_ptr], label, SYMBOL_LEN);
  f_elsedone[f_ptr] = FALSE;	/* Haven't done ELSE yet */
  return TRUE;
}

int checkcond(void)
{
  return (f_ptr != -1 && strncmp(f_stack[f_ptr], label, SYMBOL_LEN) == 0);
}

static struct fs {
  char string[3];
  int value;
} fs_table[] = {
  { "P", FS_P },
  { "WP", FS_WP },
  { "XS", FS_XS },
  { "X", FS_X },
  { "S", FS_S },
  { "M", FS_M },
  {  "B", FS_B },
  { "W", FS_W },
  { "A", FS_A },
  { "", FS_BAD }
};

#define NUM_FS (sizeof(fs_table) / sizeof(struct fs)) - 1

EXPRPTR doexpr_fill(void)
{
  EXPRPTR exprp;
	
  if ((exprp=doexpr()) != (EXPRPTR) NULL) {
    addfillref(exprp);
  }
  return exprp;
}

void zerrmsg(char *s)
{
  *this_opcode.valuefield.value = '\0';
  this_opcode.length = 0;
  errmsg(s);
}

OPTABLEPTR getopmem(int num_entries)
{
  OPTABLEPTR opt;
	
  opt = (OPTABLEPTR) calloc((unsigned) num_entries, OPTABLESIZE);
  DBG3("Address %ld: allocated %d bytes",(long) opt,num_entries*OPTABLESIZE)
    return opt;
}


OPTABLEPTR add_optable(OPTABLEPTR tbl_pointer, OPTABLEPTR *lastptr)
{
  int done = FALSE;
  int compare;
	
  while (!done) {
    if (*lastptr != (OPTABLEPTR) NULL) {
      compare = strcmp( tbl_pointer->op_mnem,(*lastptr)->op_mnem);
      if (compare) {
	lastptr = compare < 0 ? &(*lastptr)->op_left
	  : &(*lastptr)->op_right;
      } else
	done = TRUE;
    } else {	/* add it here */
      *lastptr = tbl_pointer;
      tbl_pointer->op_left = NULL;
      tbl_pointer->op_right = NULL;
      done = TRUE;
    }
  }
  return (*lastptr);
}


char *getterm(void)
{
  switch (*ptr) {
  case '"':
    ++ptr;
    return "\"";
  case '\'':
    ++ptr;
    return "'";
  case '<':
    ++ptr;
    return ">";
  default:
    return SEPARATORS;
  }
}

char *uppercase(char *str)
{
  char *sp = str - 1;
  char c;
	
  while ((c = *++sp)) {
    if (islower(c))
      *sp = toupper(c);
  }
  return (str);
}

int getfs2(void)
{
  /* Get Field Select, return FS_value. */
  char fs[LINE_LEN+1];
  int i;
	
  if (!getfield2(fs, LINE_LEN, " \t"))
    return (FS_BAD);
  (void) uppercase(fs);
  for (i=0; i < NUM_FS; i++)
    if (strcmp(fs_table[i].string, fs) == 0)
      break;
  return (fs_table[i].value);
}
int getfs(void)
{
  /* Get Field Select, return FS_value. */
  char fs[LINE_LEN+1];
  int i;
	
  if (!getfield(fs, LINE_LEN, " \t,"))
    return (FS_BAD);
  (void) uppercase(fs);
  for (i=0; i < NUM_FS; i++)
    if (strcmp(fs_table[i].string, fs) == 0)
      break;
  return (fs_table[i].value);
}

EXPRPTR getdigitexpr(void)
{
  EXPRPTR exp;
	
  if ((exp = doexpr_fill()) != (EXPRPTR) NULL) {
    if (exp->e_relcount != 0) {
      errmsg("Relocatable offset not allowed here");
      return (EXPRPTR) NULL;
    }
    if (expr_resolved && (exp->e_value >15 || exp->e_value < 0))
      errmsg("Expression out of range");
  }
  return exp;
}

EXPRPTR getdigitexpr2(void)
{
  EXPRPTR exp;
	
  if ((exp = doexpr_fill()) != (EXPRPTR) NULL) {
    if (exp->e_relcount != 0) {
      errmsg("Relocatable offset not allowed here");
      return (EXPRPTR) NULL;
    }
  }
  return exp;
}

void reverse(char *sp)
{
  char save, *tmp;
	
  tmp = sp + strlen(sp) - 1;
  while (sp < tmp) {
    save = *sp;
    *sp++ = *tmp;
    *tmp-- = save;
  }
}

void breverse(char *sp)
{
  char save1, save2, *tmp;
	
  tmp = sp + strlen(sp) - 1;
  while (sp < tmp) {
    save1 = *sp;
    save2 = *(sp+1);
    *sp++ = *(tmp-1);
    *sp++ = *tmp;
    *tmp-- = save2;
    *tmp-- = save1;
  }
}

int decode(char **cptr)
{
  int i, charvalue = 0;
	
  if (**cptr == '\0')
    return -1;
  if (**cptr == '\\') {
    ++*cptr;
    if (**cptr == '\0')
      return -1;
    switch (**cptr) {
    case 'a':
      charvalue = 7;
      break;
    case 'b':
      charvalue = 8;
      break;
    case 't':
      charvalue = 9;
      break;
    case 'n':
      charvalue = 10;
      break;
    case 'v':
      charvalue = 11;
      break;
    case 'f':
      charvalue = 12;
      break;
    case 'r':
      charvalue = 13;
      break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      for (i=0; i<3 && isdigit(**cptr); ++i) {
	charvalue = (charvalue << 3) + *(*cptr)++ - '0';
      }
      --*cptr;	/* undo last increment */
      break;
    case 'x':
      /* HEX character */
      ++*cptr;
      for (i=0; i<2 && isxdigit(**cptr); ++i) {
	charvalue <<= 4;
	if (isdigit(**cptr))
	  charvalue += **cptr - '0';
	else
	  charvalue += toupper(**cptr) - 'A' + 10;
	++*cptr;
      }
      --*cptr;	/* undo last increment */
      if (i == 0)		/* if (i == 0), not \xnn sequence */
	charvalue = **cptr;
      break;
    default:
      charvalue = **cptr;
    }
    /* now (**cptr) is the last character used */
  } else
    charvalue = **cptr;
  ++*cptr;
  return (unsigned char) charvalue;
}

int getasc(char *dest, int len)
{
  char buffer[LINE_LEN +1], *bp, termchr[2];
  int value;
  int valid = FALSE;
	
  termchr[1] = '\0';
  if ((*termchr = *ptr) != '\0') {
    ++ptr;
    if (getfield(buffer, LINE_LEN, termchr) && *ptr == *termchr) {
      ++ptr;	/* skip terminating character */
      bp = buffer;
      while ((value = decode(&bp)) != -1 && len-- > 1) {
	value = charset[value];
	*dest++ = tohex[value % 16];
	--len;
	*dest++ = tohex[(value >> 4) & 0xF];
	valid = TRUE;
      }
    }
  }
  *dest = '\0';
  return (valid && *bp == '\0');
}

int getbin(char *dest, int len, int straight )
{
  int	valid = FALSE, bits = 0, value = 0;
  char	*bp = NULL, buffer[OPCODE_LEN + 1];
	
  if( getfield( buffer, OPCODE_LEN, SEPARATORS) ) {
    bp = buffer;
    if( *bp == '%' )
      bp++;
    while( ( *bp == '0' || *bp == '1' ) && len-- > 0 ) {
      /* Or with #8 #4 #2 or #1 if '1' */
      if( *bp == '1' ) {
	if( straight )
	  value |= 1 << ( 3 - bits );
	else
	  value |= 1 << bits;
      }
      if( ++bits == 4 ) {
	*dest++ = tohex[value];
	value = bits = 0;
      }
      bp++;
      valid = TRUE;
    }
  }
  /* Output cached bits */
  if( bits != 0 )
    *dest++ = tohex[value];
  *dest = '\0';
  if( valid && *bp == '\0' )
    return TRUE;
  else
    return FALSE;
}


int gethex(char *dest, int len)
{
  char buffer[OPCODE_LEN+1], *bp;
  int valid = FALSE;
	
  if (getfield(buffer, OPCODE_LEN, SEPARATORS)) {
    bp = buffer;
    if (*bp == '#')
      ++bp;
    while (isxdigit(*bp) && len-- > 0) {
      *dest++ = toupper(*bp);
      ++bp;
      valid = TRUE;
    }
  }
  *dest = '\0';
  return (valid && *bp == '\0');
}

void alloptable(void (*doit)(), OPTABLEPTR root)
{
  if (root != (OPTABLEPTR) NULL) {
    if (root->op_left != (OPTABLEPTR) NULL)
      alloptable(doit, root->op_left);
    (*doit)(root);
    if (root->op_right != (OPTABLEPTR) NULL)
      alloptable(doit, root->op_right);
  }
}

void dofree(OPTABLEPTR optr)
{
  MACROPTR mp, mpold;
	
  mpold = optr->op_macroptr;
  while (mpold != (MACROPTR) NULL) {
    mp = mpold->m_next;
    free((char *) mpold);
    DBG2("Address %ld: free'd memory", (long) mpold)
      mpold = mp;
  }
  free((char *) optr);
  DBG2("Address %ld: free'd memory", (long) optr)
    }

void cleanmacros(void)
{
  /* free all macro table entries */
  alloptable(dofree, macro_root);
  macro_root = NULL;
}


OPTABLEPTR findoptable(char *mnem_in, OPTABLEPTR node)
{
  int compare;
	
  while (node != (OPTABLEPTR) NULL) {
    if ((compare=strcmp(mnem_in, node->op_mnem)) == 0)
      break;
    node = (compare < 0) ? node->op_left : node->op_right;
  }
  return (node);
}

OPTABLEPTR all_findoptable(char *mnem_in)
{
  OPTABLEPTR result = NULL;
	
  if ((result = findoptable(mnem_in, optable_root)) == (OPTABLEPTR) NULL
      && (result = findoptable(mnem_in, macro_root)) == (OPTABLEPTR) NULL ) {
		
    /* Not found with case unchanged - try forcing upper case */
    mnem_in = uppercase(mnem_in);
    if ((result = findoptable(mnem_in, optable_root)) == (OPTABLEPTR) NULL)
      result = findoptable(mnem_in, macro_root);
  }
  return result;
}

int fromhex(char c)
{
  return (isdigit(c) ? c - '0' : c - 'A' + 10);
}

void setnibbles(char *cptr, int value, int nibbles)
{
  while (nibbles-- > 0) {
    *cptr++ = tohex[value & 0x0F];
    value >>= 4;
  }
}

void setnibble(char *cptr, int value)
{
  *cptr = tohex[(fromhex(*cptr) | value) & 0xF];
}

void changenibble(char *cptr, int value)
{
  *cptr = tohex[(fromhex(*cptr) ^ value) & 0xF];
}

void clearnibble(char *cptr, int value)
{
  *cptr = tohex[(fromhex(*cptr) & value) & 0xF];
}

void setexpr(i_32 value)
{
  int i = entry->op_size;
  char *cptr = &this_opcode.valuefield.value[offset];
	
  while (i-- > 0) {
    *cptr++ = tohex[value & 0xF];
    value >>= 4;
  }
  *cptr = '\0';
}

void setfield(int field)
{
  if ((entry->op_flags & (A_SPEC | B_SPEC)) == 0) {
    setnibble(this_opcode.valuefield.value + entry->op_fs_nib, field);
    return;
  } 
  if (entry->op_flags & A_SPEC && entry->op_Afield == field) {
    /* process A field */
    (void) strcpy(this_opcode.valuefield.value, entry->alt1_op_string);
    return;
  }
	
  if (entry->op_flags & B_SPEC && entry->op_Bfield == field) {
    /* process B field */
    (void) strcpy(this_opcode.valuefield.value, entry->alt2_op_string);
    return;
  }
	
  /* op_flags has A_SPEC or B_SPEC set, but FS does not match */
  setnibble(this_opcode.valuefield.value + entry->op_fs_nib, field);
  return;
}

b_32 getlength(void)
{
  b_32 len_expr, len_value;
	
  len_value = strlen(this_opcode.valuefield.value);
	
  if (entry->op_operand > OPD_FS2) {
    len_expr = offset + entry->op_size;
    while (len_expr > len_value)
      this_opcode.valuefield.value[len_value++] = '0';
    this_opcode.valuefield.value[len_value] = '\0';
  }
  return (len_value);
}

struct loop *newloop( void )
{
  struct loop *dummy;
	
  if( ( dummy = (struct loop *)calloc( 1, sizeof( struct loop ) ) ) == NULL )
    error( "error getting memory for for-next loop",NULL );
  return dummy;
}


OPTABLEPTR newmacro(char *name)
{
  OPTABLEPTR macro_ptr;
	
  if ((macro_ptr=getopmem(1)) == (OPTABLEPTR) NULL)
    error("%s", "error getting memory for macro entry");
  (void) strncpy(macro_ptr->op_mnem, name, MNEMONIC_LEN);
  macro_ptr->op_operand = OPD_NONE;
  macro_ptr->op_flags = PSEUDO_OP | MACRO;
  macro_ptr->op_length = -1;
  macro_ptr->op_string[0] = '\0';
  macro_ptr->alt1_op_string[0] = '\0';
  macro_ptr->alt2_op_string[0] = '\0';
  macro_ptr->op_fs_nib = 0;
  macro_ptr->op_offset = 0;
  macro_ptr->op_size = 0;
  macro_ptr->op_class = 0;
  macro_ptr->op_subclass = 0;
  macro_ptr->op_Afield = 0;
  macro_ptr->op_Bfield = 0;
  macro_ptr->op_macroptr = NULL;
  lastmacro = &macro_ptr->op_macroptr;
  return (add_optable(macro_ptr, &macro_root));
}

O_SYMBOLPTR getobject(int *in_index)
{
  char string[LINE_LEN];
  static union {
    char c[256];
    struct {
      char nam[4];
      O_SYMBOL s[SYMBS_PER_RECORD];
    } sym;
  } in;
  O_SYMBOLPTR sym_rec;
	
  while (*in_index >= SYMBS_PER_RECORD) {
    if (fread(in.c, 256, 1, symfile) != 1) {
      (void) sprintf(string, "Error reading from RDSYMB file (%s)",
		     nameptr);
      errmsg(string);
      return (O_SYMBOLPTR) NULL;
    }
    *in_index -= SYMBS_PER_RECORD;
  }
	
  sym_rec = &in.sym.s[*in_index];
  *in_index += 1 + (get_u16(sym_rec->os_fillcount) & OS_REFERENCEMASK);
  return sym_rec;
}

void rdsymb(void)
{
  char *term;
  char string[LINE_LEN];
  OBJHEAD buffer;
  O_SYMBOLPTR sym;
  b_16 symcount;
  int in_index = SYMBS_PER_RECORD;
  int mod_abs, symres;
	
  term = getterm();
  if (getfield(symname, LINE_LEN, term)) {
    if ((symfile=openfile(symname,RDSYMB_ENV,RDSYMB_PATH,BIN_READONLY,NULL))
	!= (FILE *) NULL) {
      if (fread((char *) &buffer, sizeof(buffer), 1, symfile) == 1
	  && strncmp(buffer.id, SAT_ID, sizeof(buffer.id)) == 0
	  && ! fseek(symfile,
		     (long)(((get_u32(buffer.nibbles)+511)/512+1)*256), 0) ) {
	symcount = get_u16(buffer.symbols);
	mod_abs = get_u8(buffer.absolute);
	while (symcount-- > 0) {
	  if ((sym = getobject(&in_index)) == (O_SYMBOLPTR) NULL) {
	    printf("end (%d)\n",symcount);
	    break;
	  }
	  symres = (get_u16(sym->os_fillcount) & (OS_RESOLVED | OS_RELOCATABLE));
	  if ((OS_RESOLVED || mod_abs) && (symres & OS_RESOLVED)) {
	    addlabel(sym->os_name, get_32(sym->os_value),
		     S_RDSYMB | S_RESOLVED);
	  }
	}
      } else {
	(void) sprintf(string,"Error reading RDSYMB file (%s)",nameptr);
	errmsg(string);
      }
      (void) fclose(symfile);
      free(nameptr);
      DBG2("Address %ld: free'd memory", (long) nameptr)
	} else {
	  (void) sprintf(string, "Unable to open RDSYMB file (%s)", symname);
	  errmsg(string);
	}
  } else
    errmsg("Invalid file specifier");
}

void inclob(void)
{
  char	*cp;
  char	string[LINE_LEN];
  static char	*file;
  size_t	size;

  cp = getterm();
  if (getfield(symname, LINE_LEN, cp)) {
    if ((symfile=openfile(symname,"","",
			  BIN_READONLY,NULL)) != (FILE *) NULL) {
      if (fseek(symfile,0L,SEEK_END) != 0) {
	error("error setting location in include file %s",
	      nameptr);
      }
      size = ftell(symfile);
      if (size < 0) {
	error("Can't guess the size of %s",
		       nameptr);
      }
      if ((file = malloc(size)) == NULL) {
	error("Can't allocate memory for %s",nameptr);
      }
      fseek(symfile,0L,SEEK_SET);
      if (fread(file,size,1L,symfile) != 1) {
	(void) sprintf(string, "Can't read %s", nameptr);
	errmsg(string);
      }
      fclose(symfile);
      this_opcode.variable = TRUE;
      this_opcode.valuefield.valuebyte = file;
      this_opcode.length = 2 * size;
      free(nameptr);
    }
    else {
      (void) sprintf(string, "Unable to open file (%s)", symname);
      errmsg(string);
    }
  }
  else
    errmsg("Invalid file specifier");
}

void hpcharmap(void)
{
  char *cp;
  char string[LINE_LEN];
  int byte1, byte2;
	
  cp = getterm();
  if (getfield(symname, LINE_LEN, cp)) {
    if ((symfile=openfile(symname,CHARMAP_ENV,CHARMAP_PATH,
			  TEXT_READONLY,NULL)) != (FILE *) NULL) {
      while ((cp=fgets(string, LINE_LEN, symfile)) != (char *) NULL) {
				/* Interpret the characters here */
	byte1 = decode(&cp);
	byte2 = decode(&cp);
	if (byte1 != -1 && byte2 != -1)
	  charset[byte1] = byte2;
      }
      if (ferror(symfile) != 0) {
	(void) sprintf(string, "Error reading CHARMAP file (%s)",
		       nameptr);
	errmsg(string);
      }
      (void) fclose(symfile);
      free(nameptr);
      DBG2("Address %ld: free'd memory", (long) nameptr)
	} else {
	  (void) sprintf(string, "Unable to open CHARMAP file (%s)", symname);
	  errmsg(string);
	}
  } else
    errmsg("Invalid file specifier");
}

void putmacroline(void)
{
  char *cp;
	
  if (pass1 && (macroentry->op_flags & MACRO)) {
    /* Add the line to the macro chain */
    cp = malloc((unsigned) (MACROSIZE + strlen(linestart)+1));
    DBG3("Address %ld: allocated %d bytes",(long) cp,
	 MACROSIZE + strlen(linestart)+1);
    if (cp == (char *) NULL)
      error("error getting memory for macro line '%s'", linestart);
    *lastmacro = (MACROPTR) cp;
    (*lastmacro)->m_line = &cp[MACROSIZE];
    (*lastmacro)->m_next = NULL;
    (void) strcpy((*lastmacro)->m_line, linestart);
    lastmacro = &(*lastmacro)->m_next;
  }
}

OPCODEPTR getopcode(char *mnem_in)
{
  char *saveptr = ptr;
  char *cp;
  int savecol = column;
  int i, field, size_instr;
  EXPRPTR exprptr;
  SYMBOLPTR symbptr;
  b_8 cstate;
  i_32 reloffset, maxoffset;
  /* Special variables for instruction reg=reg+- x field */
  char	buf[OPCODE_LEN + 1];
  char	set1[OPCODE_LEN + 1];
  int		sign;
	
  if ((entry = all_findoptable(mnem_in)) != (OPTABLEPTR) NULL) {
    offset = entry->op_offset;
    flags = entry->op_flags;
    if (flags & PSEUDO_OP)
      class_id = entry->op_class;
    else
      class_id = 0;
  } else {
    if (assembling && !addingmacro) {
      errmsg("Illegal mnemonic");
      return (&null_opcode);
    }
  }
  this_opcode.variable = FALSE;

  if (!assembling) {
    if (entry == (OPTABLEPTR) NULL || (flags & PSEUDO_OP) == 0
	|| (class_id != PS_ELSE
	    && class_id != PS_ENDIF
	    && ! (class_id==PS_ENDM && imtype==START_MACRO))) {
      *label = '\0';
      listthisline = FALSE;
      listflags |= LIST_SUPPRESS;
      return(&null_opcode);
    }
  }
  if (addingmacro) {
    if (class_id == PS_FOR) {
      errmsg("Can't nest FOR inside MACRO");
      listaddress = FALSE;
    }
    else {
      listaddress = FALSE;
      if (entry != (OPTABLEPTR) NULL && (flags &PSEUDO_OP) && class_id == PS_ENDM
	  && (*label == '\0'
	      || strncmp(macroentry->op_mnem,label,MNEMONIC_LEN) == 0)) {
				/* This is the ENDM corresponding the this MACRO - finish it up */
	addingmacro = FALSE;
      } else {
				/* Didn't find ENDM yet -- add the line to the macro */
				/* Note that if this is ENDM, but the label is wrong, it will */
				/* generate an error when the macro is expanded.  It cannot   */
				/* generate an error here because this might be the ENDM of a */
				/* MACRO/ENDM pair (nested macro definition). */
	putmacroline();
      }
    }
    *label = '\0';	/* don't add this label to symbol table */
    return (&null_opcode);
  }
	
  if ((flags & LEVELBITS) > code_level) {
    errmsg("Instruction not allowed with this CPU");
    return(&null_opcode);
  }
	
  if ( (flags & COMBOS) && !combos) {
    errmsg("COMBOS not allowed in SASM compatibility mode");
    return (&null_opcode);
  }
	
  if ( (flags & MASD) && !masd) {
    errmsg("MASD instruction set not allowed");
    return (&null_opcode);
  }
	
  if (flags & MACRO) {
    if (pass2 && (flags & MACRO_PASS2) == 0) {
      errmsg("Illegal mnemonic");
      return (&null_opcode);
    }
    impending = START_MACRO;
    /* If MACRO lines will be listed, show invoking line, too! */
    if (listflags & LIST_MACRO)
      listthisline = TRUE;
    return (&null_opcode);
  }
  (void) strcpy(this_opcode.valuefield.value, entry->op_string);
  this_opcode.length = 0;
    
  if (flags & PSEUDO_OP) {
    listaddress = FALSE;
    switch (class_id) {
    case PS_ABS:
    case PS_REL:
      listaddress = TRUE;
      if (begin_pseudos_ok) {
	if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	  if (absexpr(exprptr)) {
	    if (exprptr->e_value < MAX_NIBBLES) {
	      if (! have_start_addr) /* command line overrides */
		pc = codestart = exprptr->e_value;
	    } else
	      zerrmsg("Value too big");
	  }
	  else
	    zerrmsg("Expression must be absolute");
	} else
	  zerrmsg("Invalid expression");
	absolute = (class_id == PS_ABS);
	cp = label;
				/* Redefine "*START" symbol */
	symbptr = addsymbol("*START", codestart, S_RESOLVED|S_SETPASS2);
	symbptr->s_value = codestart;
	symbptr->s_flags = S_RESOLVED | S_SETPASS2;
	if (absolute == FALSE)
	  symbptr->s_flags |= S_RELOCATABLE;
	if (*cp) {
	  if (*cp == '=')
	    ++cp;
	  (void) strncpy(prog_symbol, cp, SYMBOL_LEN+1);
	  i = S_RESOLVED | S_ENTRY;
					
	  if (absolute == FALSE)
	    i |= S_RELOCATABLE;	/* relocatable */
					
	  symbptr = addsymbol(cp, pc, i);
	  if (pass1)
	    addlineref(symbptr);
	  if (pass2) {
	    if (symbptr->s_flags & S_SETPASS2)
	      errmsg("Duplicate symbol");
	    else {
	      symbptr->s_flags |= S_SETPASS2;
	      if (symbptr->s_value != pc) {
		errmsg("PC changed (use old value)");
		pc = symbptr->s_value;
	      }
	    }
	  }
	}
      } else
	errmsg("ABS/REL must be at beginning");
      *label = '\0';
      begin_pseudos_ok = FALSE;
      break;
    case PS_BSS:
      listaddress = TRUE;
      if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	if (absexpr(exprptr)) {
	  if (exprptr->e_value >= 0) {
	    if ((b_32) (pc + exprptr->e_value) < MAX_NIBBLES)
	      this_opcode.length = exprptr->e_value;
	    else
	      zerrmsg("Number of nibbles too big for file");
	  }
	  else
	    zerrmsg("Number of nibbles must be non-negative");
	} else
	  zerrmsg("Expression must be absolute");
      } else
	zerrmsg("Invalid expression");
      break;
    case PS_END:
      listaddress = TRUE;
      if (imtype == NO_IM)
	reading = FALSE;
      else
	impending = END_INCLUDE;
      break;
    case PS_IF:
    case PS_IFCOND:
      if (pushcond()) {
	listthisline = (listflags & LIST_PSEUDO) != 0;
				/* check for flag value */
	if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	  if (absexpr(exprptr)) {
	    i_32 exp = exprptr->e_value;
	    b_8 results = (exp==0) ? C_EQ : ((exp<0) ? C_LT : C_GT);
	    if (class_id == PS_IF) {
	      if (exp >= 0 && exp < NUMBER_OF_FLAGS)
		assembling = (flag[exp] & FLAG_SET) != 0;
	      else
		zerrmsg("Flag value out of range");
	    } else {
	      /* PS_IFCOND */
	      assembling = (results & entry->op_subclass) != 0;
	    }
	  } else
	    zerrmsg("IF expression must be absolute");
	} else
	  zerrmsg("Invalid flag expression");
      }
      *label='\0';
      break;
    case PS_IFSTRCOND:
      if (pushcond()) {
	char sep = *ptr++;	/* First character is separator */
	char *s1 = ptr;
	char *s2 = strchr(ptr, sep);
	char save1, save2;
	int results;
				
	listthisline = (listflags & LIST_PSEUDO) != 0;
				
	if (s2!=(char *)NULL && (cp=strchr(&s2[1],sep))!=(char *)NULL) {
	  save1 = *s2;
	  *s2++ = '\0';
	  save2 = *cp;
	  *cp = '\0';
	  results = strcmp(s1,s2);
	  *cp = save2;
	  *--s2 = save1;
	  results = (results==0) ? C_EQ : ((results<0) ? C_LT : C_GT);
	  assembling = (results & entry->op_subclass) != 0;
	} else
	  zerrmsg("Missing separator character");
      }
      *label='\0';
      break;
    case PS_IFDEF:
    case PS_IFNDEF:
      if (pushcond()) {
	listthisline = (listflags & LIST_PSEUDO) != 0;
				/* check if symbol is defined */
	if (getfield(symname, LINE_LEN, SEPARATORS)) {
	  if ((symbptr = findsymbol(symname)) != (SYMBOLPTR) NULL
	      && (pass1
		  || (pass2 && symbptr->s_flags & S_SETPASS2))) {
	    assembling = (class_id == PS_IFDEF);
	  } else {
	    assembling = (class_id == PS_IFNDEF);
	  }
	  if (pass2 && symbptr != (SYMBOLPTR) NULL)
	    addlineref(symbptr);
	} else
	  zerrmsg("Invalid symbol name");
      }
      *label='\0';
      break;
    case PS_IFOPC:
    case PS_IFNOPC:
      if (pushcond()) {
	listthisline = (listflags & LIST_PSEUDO) != 0;
				/* check if operand is defined */
	if (getfield(symname, LINE_LEN, SEPARATORS)) {
	  OPTABLEPTR opt;
	  int fl;
					
	  if ((opt=all_findoptable(symname)) != (OPTABLEPTR) NULL) {
	    fl = opt->op_flags;
	    if (pass2 && (fl & MACRO) && ! (fl & MACRO_PASS2))
	      opt = NULL;
	  }
	  if (opt != (OPTABLEPTR) NULL) {
	    assembling = (class_id == PS_IFOPC);
	  } else {
	    assembling = (class_id == PS_IFNOPC);
	  }
	} else
	  zerrmsg("Invalid symbol name");
      }
      *label='\0';
      break;
    case PS_IFPASS1:
    case PS_IFPASS2:
      if (pushcond()) {
	listthisline = (listflags & LIST_PSEUDO) != 0;
				/* check which pass it is now */
	assembling = (  (pass1 && class_id == PS_IFPASS1)
			|| (pass2 && class_id == PS_IFPASS2) );
      }
      *label='\0';
      break;
    case PS_IFCARRYSET:
    case PS_IFCARRYCLR:
    case PS_IFANYCARRY:
    case PS_IFREACHED:
      if (pushcond()) {
	listthisline = (listflags & LIST_PSEUDO) != 0;
				/* check carry state (assemble-time possibilities only!) */
	switch (class_id) {
	case PS_IFCARRYSET:
	  assembling = (carry == OP_C);
	  break;
	case PS_IFCARRYCLR:
	  assembling = (carry == OP_NC);
	  break;
	case PS_IFANYCARRY:
	  assembling = (carry == (OP_C | OP_NC));
	  break;
	case PS_IFREACHED:
	  assembling = (carry != OP_NOTREACHED);
	  break;
	}
      }
      *label='\0';
      break;
    case PS_ELSE:
      if (checkcond()) {
	if (! f_elsedone[f_ptr]) {
	  assembling = !assembling;
	  f_elsedone[f_ptr] = TRUE;
	  listthisline = (listflags & LIST_PSEUDO) != 0;
	} else {
	  errmsg("Duplicate ELSE statement");
	}
      } else {
				/* Not matched - error message if and only if assembling  */
				/* If not assembling, could be in the non-executed part   */
				/* of the outer IF/ENDIF, and this could be a nested IF/  */
				/* ENDIF which is being ignored currently.                */
	if (assembling)
	  errmsg("ELSE without matching IF");
      }
      *label='\0';
      break;
    case PS_ENDIF:
      if (checkcond()) {
	--f_ptr;	/* drop the flag */
	assembling = TRUE;
	listthisline = (listflags & LIST_PSEUDO) != 0;
      } else {
				/* Not matched - error message if and only if assembling  */
				/* This situation is more fully described in PS_ELSE */
	if (assembling)
	  errmsg("ENDIF without matching IF");
      }
      *label='\0';
      break;
    case PS_MESSAGE:
      (void) fputs(ptr, stderr);
      (void) fputc('\n', stderr);
      *label = '\0';
      break;
    case PS_RDSYMB:
      rdsymb();
      *label = '\0';
      break;
    case PS_EQU:
      /* sym  EQU  value */
      if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	if (exprptr->e_extcount == 0) {
	  /* addlabel() complains if the symbol is already defined */
	  addlabel(label, exprptr->e_value, S_RESOLVED
		   | (exprptr->e_relcount!=0 ? S_RELOCATABLE : 0));
	} else {
	  zerrmsg("Expression can not be external");
	  addlabel(label, (i_32) 0, S_RESOLVED);
	}
      } else {
	zerrmsg("Invalid expression");
	addlabel(label, (i_32) 0, S_RESOLVED);
      }
      *label = '\0';	/* don't add it in main() */
      break;
    case PS_EQUALS:
      /* sym  =  value */
      if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	if (exprptr->e_extcount == 0) {
	  /* xaddlabel() allows a new value for the symbol	*/
	  /* if the fourth parameter is non-zero		*/
	  xaddlabel(label, exprptr->e_value, S_RESOLVED
		    | (exprptr->e_relcount != 0 ? S_RELOCATABLE : 0),
		    TRUE);
	} else {
	  zerrmsg("Expression can not be external");
	  xaddlabel(label, (i_32) 0, S_RESOLVED, TRUE);
	}
      } else {
	zerrmsg("Invalid expression");
	xaddlabel(label, (i_32) 0, S_RESOLVED, TRUE);
      }
      *label = '\0';	/* don't add it in main() */
      break;
    case PS_ABASE:
      if((exprptr = doexpr()) != NULL) {
	if(exprptr->e_value >= 0x100000L)
	  zerrmsg("Too big base value");
	else
	  abase = exprptr->e_value;
      } else
	zerrmsg("Invalid expression");
      break;
			
    case PS_ALLOC:
      if((exprptr = doexpr()) != NULL) {
	if(exprptr->e_extcount == 0) {
	  addlabel(label, abase, ((exprptr->e_relcount != 0) ? S_RELOCATABLE : 0) | S_RESOLVED);
	  abase += exprptr->e_value;
	  if(abase >= 0x100000L) {
	    zerrmsg("Allocation wrapped around zero");
	    abase -= 0x100000L;
	  }
	} else {
	  zerrmsg("Expression can not be external");
	  addlabel(label, 0L, S_RESOLVED);
	}
      } else {
	zerrmsg("Invalid expression");
	addlabel(label, 0L, S_RESOLVED);
      }
      label[0] = '\0';
      break;
			
    case PS_TITLE:
      if (*title == '\0')
	(void) strncpy(title, ptr, 40);
      if (strncmp(title, ptr, 40) != 0)
	errmsg("Only one TITLE statement allowed per file");
      *label = '\0';
      break;
    case PS_STITLE:
      (void) strncpy(subtitle, ptr, 40);
      /* fall into EJECT code here */
    case PS_EJECT:
      /* ignore if on last line of a page or if listing is not enabled */
      if (listthisline) {
	if (lineofpage < pagelength)
	  lineofpage = pagelength - ((listflags & LIST_PSEUDO) != 0);
				/* don't list this line if LIST_PSEUDO not active */
	listthisline = (listflags & LIST_PSEUDO) != 0;
      }
      *label = '\0';	/* ignore any label on STITLE or EJECT */
      break;
    case PS_LIST:
    case PS_LISTM:
    case PS_UNLIST:
      switch (class_id) {
      case PS_LIST:
	listflags |= LIST_CODE;
	break;
      case PS_LISTM:
	listflags |= LIST_MACRO | LIST_INCLUDE;
	break;
      case PS_UNLIST:
	listflags &= ~ (LIST_CODE | LIST_INCLUDE | LIST_MACRO);
	break;
      }
      listthisline = (listflags & LIST_PSEUDO) != 0;
      symbptr = addsymbol("*LISTFLAGS", (i_32) 0, S_RESOLVED);
      symbptr->s_value = (i_32) (listflags & LIST_ALL);
      *label = '\0';	/* ignore any label on LIST */
      break;
    case PS_LISTALL:
				/* Turn on listing of ALL lines for N lines */
				/* The LIST_FORCE flag is turned on in sasm.c after printing */
				/* this line (when listcount > 0 after processing this line) */
      listcount = 0;
      if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	if (absexpr(exprptr))
	  listcount = exprptr->e_value;
	else
	  zerrmsg("Expression not absolute");
      } else
	zerrmsg("Invalid expression");
      *label = '\0';	/* ignore any label on LISTALL */
      break;
    case PS_SETLIST:
    case PS_CLRLIST:
      listthisline = (listflags & LIST_PSEUDO) != 0;
				/* valid types are "MACRO", "INCLUDE", "CODE", "PSEUDO", "ALL" */
				/* another valid type is "NOLIST", which suppresses listing of */
				/* this line */
				/* Types are separated by " \t," */
      i = 0;
      while (getfield(symname, LINE_LEN, " \t,")) {
	(void) uppercase(symname);
	switch (*symname) {
	case 'M':
	  i |= LIST_MACRO;
	  break;
	case 'I':
	  i |= LIST_INCLUDE;
	  break;
	case 'C':
	  i |= LIST_CODE;
	  break;
	case 'P':
	  i |= LIST_PSEUDO;
	  break;
	case 'A':
	  i |= LIST_ALL;
	  break;
	case 'N':
	  listthisline = FALSE;
	  break;
	default:
	  errmsg(
		 "List type not CODE,MACRO,INCLUDE,PSEUDO,ALL, or NOLIST"
		 );
	}
	if (*ptr != '\0')
	  ++ptr;	/* Skip the separator character */
	skipwhite();
      }
      if (class_id == PS_SETLIST)
	listflags |= i;
      else
	listflags &= ~ i;
      symbptr = addsymbol("*LISTFLAGS", (i_32) 0, S_RESOLVED);
      symbptr->s_value = (i_32) (listflags & LIST_ALL);
      *label = '\0';	/* ignore any label on CLRLIST, SETLIST */
      break;
    case PS_MACRO:
      cp = label;
      if (*cp == '\0') {
	errmsg("Unlabeled MACRO statement (*MACRO used)");
	cp = "*MACRO";
      }
				/* Check if a macro or opcode by this name already exists */
      if ((macroentry=findoptable(cp,optable_root)) != (OPTABLEPTR)NULL) {
	errmsg("Can't redefine an existing opcode");
      }
      if (pass1 && macroentry == (OPTABLEPTR) NULL) {
	if (findoptable(cp, macro_root) != (OPTABLEPTR) NULL) {
	  errmsg("Can't redefine an existing macro");
	}
	macroentry = newmacro(cp);	/* Add it to the macro table */
      }
      if (pass2 && macroentry == (OPTABLEPTR) NULL) {
	if ((macroentry=findoptable(cp,macro_root))==(OPTABLEPTR)NULL) {
	  error("macro '%s' was not defined in pass 1", cp);
	}
	if (macroentry->op_flags & MACRO_PASS2) {
	  errmsg("Can't redefine an existing macro");
	}
	macroentry->op_flags |= MACRO_PASS2;	/* got here in pass 2 */
      }
      addingmacro = TRUE;
      listthisline = (listflags & LIST_CODE) != 0;
      *label = '\0';
      return (&null_opcode);
    case PS_EXITM:
    case PS_ENDM:
				/* This code is reached for EXITM and if ENDM is specified when */
				/* not in a macro definition */
				
				/* NOTE: if no macro is currently active, an error message */
				/* will be reported while processing 'impending'           */
				
      if (class_id == PS_EXITM)
	impending = END_MACRO;
      else
	errmsg("ENDM and EXITM not permitted outside of a macro");
				
      *label = '\0';	/* ignore any label on EXITM */
      return (&null_opcode);
    case PS_FOR:
      if( stdinread ) {
	errmsg( "Can't LOOP while reading stdin" );
	return &null_opcode;
      }
      cp = label;
      if( *cp == '\0' ) {
	errmsg( "Unlabeled FOR statement (*FOR used)" );
	cp = "*FOR";
      }
      for( i = ( LOOP_STACK_MAX - 1 ); i > 0 ; i-- ) {
	loops[i][0] = loops[i - 1][0];
	loops[i][1] = loops[i - 1][1];
	loops[i][2] = loops[i - 1][2];
      }
				/* add push stack */
				
				/* only one FOR-NEXT loop allowed for now,
				 * put spanning in following 'if'
				 */
      if( pass1 || pass2 ) {
	fnentry = newloop();
	fnentry->lp_filename = inname;
	fnentry->lp_fpos = ftell( infile );
	strncpy( fnentry->lp_label, cp, MNEMONIC_LEN );
	fnentry->lp_value = loops[0][0] = 0L;
	fnentry->lp_end = loops[0][1] = -1L;
	fnentry->lp_step = loops[0][2] = 1L;
	if( isfield() && ( exprptr = doexpr() ) != NULL ) {
	  fnentry->lp_value = loops[0][0] = exprptr->e_value;
	  skipwhite();
	  if( isfield() && ( exprptr = doexpr() ) != NULL ) {
	    fnentry->lp_end = loops[0][1] = exprptr->e_value;
	    skipwhite();
	    if( isfield() && ( exprptr = doexpr() ) != NULL ) {
	      fnentry->lp_step = loops[0][2] = exprptr->e_value;
	    }
	  }
	}
      }
      insideloop = TRUE;
      listthisline = (listflags & LIST_PSEUDO) ? TRUE : FALSE;
      label[0] = '\0';
      return &null_opcode;
				
    case PS_NEXT:
      if( fnentry == NULL ) {
	errmsg( "NEXT without FOR" );
	return &null_opcode;
      }
      fnentry->lp_value += fnentry->lp_step;
      loops[0][0] = fnentry->lp_value;
      if( fnentry->lp_value <= fnentry->lp_end ) {
	/* check that lp_filename belongs to inname !! */
	fseek( infile, fnentry->lp_fpos, SEEK_SET );
	listthisline = FALSE;
      } else {
	for( i = 0; i < ( LOOP_STACK_MAX - 1 ); i++ ) {
	  loops[i][0] = loops[i + 1][0];
	  loops[i][1] = loops[i + 1][1];
	  loops[i][2] = loops[i + 1][2];
	}
	/* add pop stack */
	free( fnentry );
	fnentry = NULL;
	insideloop = FALSE;
	listthisline = (listflags & LIST_PSEUDO) ? TRUE : FALSE;
      }
      label[0] = '\0';
      return &null_opcode;
				
    case PS_EXITF:
      if( fnentry == NULL ) {
	errmsg( "EXITF without FOR" );
	return &null_opcode;
      }
      fnentry->lp_value = 0L;
      fnentry->lp_end = -1L;
      fnentry->lp_step = 1L;
      label[0] = '\0';
      return &null_opcode;
				
    case PS_INCLOB:
      inclob();
      break;
				
    case PS_INCLUDE:
      cp = getterm();
      if (getfield(symname, LINE_LEN, cp)) {
	impending = START_INCLUDE;
      } else
	errmsg("Invalid file specifier");
      *label = '\0';	/* ignore any label on INCLUDE */
      break;
    case PS_CHARMAP:
      hpcharmap();
      *label = '\0';	/* ignore any label on CHARMAP */
      break;
    case PS_SETFLAG:
    case PS_CLRFLAG:
				/* check for flag value */
      if ((exprptr = doexpr()) != (EXPRPTR) NULL) {
	if (absexpr(exprptr)) {
	  i = exprptr->e_value;
	  if (i >= 0 && i < NUMBER_OF_FLAGS) {
	    if (class_id == PS_SETFLAG)
	      flag[i] |= FLAG_SET;
	    else
	      flag[i] &= FLAG_CLEAR;
	  } else
	    zerrmsg("Flag value out of range");
	} else
	  zerrmsg("Flag expression must be absolute");
      } else
	zerrmsg("Invalid flag expression");
      *label = '\0';	/* ignore any label on CLRFLAG, SETFLAG */
      break;
    case PS_SETCARRY:
    case PS_CLRCARRY:
				/* add carry set/clear flag */
      carry |= (class_id == PS_SETCARRY) ? OP_C : OP_NC;
      *label = '\0';
      break;
    case PS_NOTREACHED:
				/* clear carry set/clear flag */
      carry = 0;
      *label = '\0';
      break;
    case PS_JUMP:
				/* Figure out the shortest guaranteed jump by examining 'carry' */
				/* to see if a particular carry state is guaranteed */
				
				/* First check if a label is present.  If so, set carry state */
      if (*label) {
	DBG1("Setting carry bits (labelled opcode)");
	carry = OP_C | OP_NC;
      }
				/* If carry state is unknown, use entry->op_string as opcode */
				/* If carry state is CARRYCLR, use entry->alt1_op_string */
				/* If carry state is CARRYSET, use entry->alt2_op_string */
      switch (carry) {
      case OP_C | OP_NC:
	cp = entry->op_string;
	break;
      case OP_NC:
	cp = entry->alt1_op_string;
	break;
      case OP_C:
	cp = entry->alt2_op_string;
	break;
      case 0:
	/* This JUMP is never reached - generate no code for it */
	DBG1("No code needed for PS_JUMP");
	return(&null_opcode);
      }
      DBG2("Selected %s as new opcode for PS_JUMP", cp);
				/* now find the correct opcode for this instruction */
      if ((entry=all_findoptable(cp)) == (OPTABLEPTR) NULL)
	error("corrupt opcode table entry (%s not in table)", cp);
				/* entry now points to the new opcode - set new value */
      (void) strcpy(this_opcode.valuefield.value, entry->op_string);
      offset = entry->op_offset;
      flags = entry->op_flags;
      class_id = entry->op_class;
      listaddress = TRUE;
      break;
    case PS_UP:
    case PS_EXIT:
      if (entry->op_bias && was_test) /* If the instruction depends on a test */
	cp = entry->alt2_op_string;
      else
	cp = entry->alt1_op_string;
					
      if ( (ptr = (char *)calloc(1,LINE_LEN)) == NULL)
	error( "error getting memory for UP/EXIT",NULL );
      if (level_stack >= entry->op_offset) {
	if (class_id == PS_UP )
	  createopenlabel(ptr, masd_stack[level_stack - entry->op_offset].c1);
	if (class_id == PS_EXIT)
	  createcloselabel(ptr, masd_stack[level_stack - entry->op_offset].c2);
	if ((entry=all_findoptable(cp)) == (OPTABLEPTR) NULL)
	  error("corrupt opcode table entry (%s not in table)", cp);
	/* entry now points to the new opcode - set new value */
	(void) strcpy(this_opcode.valuefield.value, entry->op_string);
	offset = entry->op_offset;
	flags = entry->op_flags;
	class_id = entry->op_class;
	listaddress = TRUE;
      }
      else
	errmsg("Insufficient level of structure");
      break;
    case PS_SKIPCLOSE:
      if (level_stack <= 0)
	errmsg("Mismatched }");
      else {
	level_stack--;
	createcloselabel(label,masd_stack[level_stack].c2);
	/* For all } we create two labels (One for the SKELSE, one for END */
	faddlabel(label, pc, S_RESOLVED
		  | (force_global ? S_ENTRY : 0)
		  | (absolute ? 0 : S_RELOCATABLE));
	createskiplabel(label,masd_stack[level_stack].c3);
	faddlabel(label, pc, S_RESOLVED
		  | (force_global ? S_ENTRY : 0)
		  | (absolute ? 0 : S_RELOCATABLE));
      }
      break;					
    case PS_SKIP:
      /* compile a GOTO or a GOYES ? */
      cp = NULL;
      switch (entry->op_offset) {
      case 1:
	if (was_test)
	  cp = entry->alt2_op_string;
	else
	  cp = entry->alt1_op_string;
	break;
      case 2:
	/* Must have a test before */
	if (was_test)
	  cp = entry->alt1_op_string;
	else
	  errmsg("GOYES/RTNYES without test instruction");
	break;
      case 3:
	/* Can be a label only */
	if (was_test)
	  cp = entry->alt1_op_string;
	break;
      default:
	cp = entry->alt1_op_string;
	break;
      }
      if (cp) {
	if ((entry=all_findoptable(cp)) == (OPTABLEPTR) NULL)
	  error("corrupt opcode table entry (%s not in table)", cp);
	/* entry now points to the new opcode - set new value */
	(void) strcpy(this_opcode.valuefield.value, entry->op_string);
	offset = entry->op_offset;
	flags = entry->op_flags;
	class_id = entry->op_class;
	listaddress = TRUE;
					
	/* Replace with a jump to the next } */
					
	if ( (ptr = (char *)calloc(1,LINE_LEN)) == NULL)
	  error( "error getting memory for SKIP",NULL );
	createskiplabel(ptr,index_c3);
	/* We add a label in the entry (for the { ) */
	pushopenlabel(buf);
	faddlabel(buf, pc + entry->op_length, S_RESOLVED
		  | (force_global ? S_ENTRY : 0)
		  | (absolute ? 0 : S_RELOCATABLE));
      }
      else {
	pushopenlabel(buf);
	faddlabel(buf, pc, S_RESOLVED
		  | (force_global ? S_ENTRY : 0)
		  | (absolute ? 0 : S_RELOCATABLE));
      }
      break;
					
    case PS_SKELSE:
      if (level_stack <= 0)
	errmsg("Mismatched }");
      else {
	if(entry->op_length) {
	  cp = entry->alt1_op_string;
	  if ((entry=all_findoptable(cp)) == (OPTABLEPTR) NULL)
	    error("corrupt opcode table entry (%s not in table)", cp);
	  /* entry now points to the new opcode - set new value */
	  (void) strcpy(this_opcode.valuefield.value, entry->op_string);
	  offset = entry->op_offset;
	  flags = entry->op_flags;
	  class_id = entry->op_class;
	  listaddress = TRUE;
	  
	  /* Replace with a jump to the next } */
	  
	  if ( (ptr = (char *)calloc(1,LINE_LEN)) == NULL)
	    error( "error getting memory for SKELSE",NULL );
	  createskiplabel(ptr, index_c3);
	}
	/* We add a label in the entry for the skip */
	
	createskiplabel(buf,masd_stack[level_stack - 1].c3);
	masd_stack[level_stack - 1].c3 = index_c3++;
	
	faddlabel(buf, pc + entry->op_length, S_RESOLVED
		  | (force_global ? S_ENTRY : 0)
		  | (absolute ? 0 : S_RELOCATABLE));
	
	createopenlabel(buf,index_c1);
	masd_stack[level_stack - 1].c1 = index_c1++;
	
	faddlabel(buf, pc + entry->op_length, S_RESOLVED
		  | (force_global ? S_ENTRY : 0)
		  | (absolute ? 0 : S_RELOCATABLE));
      }
      break;
					
    default:
      errmsg("Unknown PSEUDO-OP");
      break;
    }
  }
	
  switch (entry->op_operand) {
  case OPD_NONE:
    break;
  case OPD_FS2:
    /* For reg=reg +x field, where x can be > 16, handle the case where x is < 0 */
    if ((exprptr = getdigitexpr2()) != (EXPRPTR) NULL) {
			
      if ((field = getfs2()) == FS_BAD ) {
	errmsg("Illegal field select");
	break;
      }
      if (expr_resolved && !expr_unknown) {
	exprptr->e_value++; /* The expression evaluator has substracted 1, add it back */
				
	if (exprptr->e_value == 0)
	  return (&null_opcode);
				
	sign = entry->op_class;
	if (exprptr->e_value < 0) {
	  exprptr->e_value = (- exprptr->e_value);
	  sign = !sign;
	}
				
	if ((exprptr->e_value != 1) && (field==FS_P || field==FS_XS || field==FS_S || field==FS_WP))
	  warningmsg("Opcode buggy if overflow");
				
	if (( ( exprptr->e_value / 16 + 1 ) * strlen(this_opcode.valuefield.value) ) < OPCODE_LEN ) {
	  /* We add reg+16.field instruction set */
	  /* We set the nibble for the field */
	  setfield(field);
	  strcpy(set1,this_opcode.valuefield.value);
	  /* We apply the sign of the expression to the mnemonic */
	  if (sign)
	    changenibble(set1 + 4,8);
	  this_opcode.length = 0;
	  this_opcode.valuefield.value[0] = '\0';
	  /* we set the value for the register */
	  setnibble(entry->op_string + 5, 0xF);
	  for (i=0; i < (exprptr->e_value - 1) / 16; i++) {
	    strcat(this_opcode.valuefield.value,set1);
	  }
	  if ((exprptr->e_value % 16) == 1) { /* do we add one ? */
	    /* We add the modulo of the size */
	    switch(field) {
	    case FS_A:
	      strcpy(set1,entry->alt2_op_string);
	      if (sign) {
		changenibble(set1,2);
		changenibble(set1 + 1,8);
	      }
	      strcat(this_opcode.valuefield.value,set1);
	      break;
	    default:
	      strcpy(set1,entry->alt1_op_string);
	      if (sign) {
		changenibble(set1,1);
		changenibble(set1 + 2, 8);
	      }
	      setnibble(set1 + 1 , field);
	      strcat(this_opcode.valuefield.value,set1); /* operation if we add 1 */
	      break;
	    }
	  }
	  else {
	    strcpy(set1,entry->op_string);
	    setnibble(set1 + 3, field);
	    if (sign)
	      changenibble(set1 + 4, 8);
	    set1[5] = tohex[((exprptr->e_value % 16) - 1) & 0xF];
	    strcat(this_opcode.valuefield.value,set1);
	  }
	  this_opcode.length = strlen(this_opcode.valuefield.value);
					
	}
	else
	  errmsg("Expression out of range");
      }
      else {
	warningmsg("If external reference is <= 0 or > 16, compilation result is undefined");
	setfield(field);
      }
    } else
      errmsg("Invalid expression");
		
    break;
		
  case OPD_FS:
  case OPD_PARTIAL_FS:
    /* op_fs_nib is location of FS nibble in opcode */
    if ((field = getfs()) != FS_BAD && *ptr != ',') {
      if (entry->op_operand == OPD_PARTIAL_FS
	  && (field==FS_P||field==FS_XS||field==FS_S||field==FS_WP)) {
	warningmsg("Opcode buggy if overflow");
      }
      setfield(field);
    } else
      errmsg("Illegal field select");
    break;
  case OPD_D_OR_FS:
    /* op_fs_nib is location of FS/offset is digit nibble in opcode */
    /* entry->op_size is for digit only */
    /* entry->op_length is unused */
    if ((field = getfs()) != FS_BAD && *ptr != ',')
      setfield(field);
    else {
      /* need to process digit expression here */
      ptr = saveptr;	/* restart expression at start */
      column = savecol;	/* restore column number w/ptr */
      if ((exprptr = getdigitexpr()) != (EXPRPTR) NULL) {
	setnibble(&this_opcode.valuefield.value[offset - 1], 8);
	if (expr_resolved) {
	  setnibble(&this_opcode.valuefield.value[offset],
		    (int) (exprptr->e_value & 0xF));
	}
      } else
	errmsg("Invalid field select/digit");
    }
    break;
  case OPD_F_AND_DIGIT:
  case OPD_PF_AND_DIGIT:	/* No single nibble fields allowed */
    /* op_fs_nib is location of FS nibble in opcode */
    if ((field = getfs()) != FS_BAD) {
      if (entry->op_operand == OPD_PF_AND_DIGIT
	  && (field==FS_P||field==FS_XS||field==FS_S||field==FS_WP)) {
	warningmsg("Opcode buggy if overflow");
				
				/* Note that the field IS set, although an error is reported */
      }
      setfield(field);
      if (*ptr == ',')
	++ptr;
      skipwhite();
      if ((exprptr = getdigitexpr()) != (EXPRPTR) NULL) {
	if (expr_resolved) {
	  setnibble(this_opcode.valuefield.value + offset,
		    (int) (exprptr->e_value & 0xF));
	}
      } else
	errmsg("Invalid expression");
    } else
      errmsg("Illegal field select");
    break;
		
  case OPD_DIGIT_EXPR2:
    /* For instruction Dx+ n, where n can be > 16 */
    if ((exprptr = getdigitexpr2()) != (EXPRPTR) NULL) {
      if (expr_resolved && !expr_unknown) {
	if (exprptr->e_value == -1)
	  return (&null_opcode);
				
	if (exprptr->e_value < -1) {
	  exprptr->e_value = (- exprptr->e_value) - 2;
	  (void) strcpy(this_opcode.valuefield.value, entry->alt1_op_string);
	}
	size_instr = strlen(this_opcode.valuefield.value);
	strcpy(buf,this_opcode.valuefield.value);
	if (
	    ( ( exprptr->e_value / 16 + 1 ) * size_instr ) < OPCODE_LEN
	    ) {
	  this_opcode.length = size_instr * (exprptr->e_value / 16 + 1);
	  for (i=0; i < exprptr->e_value / 16; i++)
	    strcat(this_opcode.valuefield.value,buf);
	  for (i=0; i < exprptr->e_value / 16; i++)
	    setnibble(this_opcode.valuefield.value + (size_instr * i) + offset, 0xF);
	  setnibble(this_opcode.valuefield.value + (size_instr * i) + offset,
		    exprptr->e_value & 0xF);
	}
	else
	  errmsg("Expression out of range");
      }
      else {
	warningmsg("If external reference is <= 0 or > 16, compilation result is undefined");
      }
    } else
      errmsg("Invalid expression");
    break;
		
  case OPD_DIGIT_EXPR:
    if ((exprptr = getdigitexpr()) != (EXPRPTR) NULL) {
      if (expr_resolved) {
	setnibble(this_opcode.valuefield.value + offset,
		  (int) (exprptr->e_value & 0xF));
      }
      else
	warningmsg("If external reference is <= 0 or > 16, compilation result is undefined");
    } else
      errmsg("Invalid expression");
    break;
  case OPD_EXPRESSION:
    if ((exprptr = doexpr_fill()) != (EXPRPTR) NULL && expr_resolved)
      setexpr(exprptr->e_value);
    break;
  case OPD_RELATIVE_JUMP:
    exprptr = NULL;
    if (rel_expressions_ok)
      exprptr = doexpr();
    else {
      /* relative jumps do NOT allow expressions! */
      expr_resolved = TRUE;
      e_ptr = o_ptr = -1;
      if (getsymbol()) {
	if (*ptr != ')')
	  exprptr = popexpr();
	else
	  errmsg("Expression not allowed as target of branch");
      } else {
	expr_resolved = FALSE;
      }
    }
    if (exprptr != (EXPRPTR) NULL) {
      if (exprptr->e_extcount == 0
	  && exprptr->e_relcount == 0 && !absolute) {
				/* Add reference to "*00000" symbol */
	abs_expr.e_value = exprptr->e_value;
	exprptr = &abs_expr;
      }
      if (expr_resolved) {
	reloffset = exprptr->e_value - pc - offset;
	if (entry->op_subclass == 1)
	  reloffset -= entry->op_size;
	maxoffset = (((i_32) 1) << (entry->op_size * 4 - 1)) - 1;
	if (reloffset+1 < (-maxoffset) || reloffset > maxoffset)
	  errmsg("Branch out of range");
	setexpr(reloffset);
      }
      addfillref(exprptr);
    }
    break;
  case OPD_ABSOLUTE_JUMP:
    /* allow an expression for destination address */
    if ((exprptr = doexpr_fill()) != (EXPRPTR) NULL && expr_resolved)
      setexpr(exprptr->e_value);
    break;
  case OPD_NIBHEX:
    if (!gethex(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset))
      zerrmsg("Invalid HEX constant");
    break;
  case OPD_BIN:
    if (!getbin(this_opcode.valuefield.value + offset, OPCODE_LEN - offset,
		(entry->op_bias == 0) ? TRUE : FALSE ))
      zerrmsg("Invalid BIN constant");
    break;
  case OPD_LCHEX:
    if (gethex(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset)) {
      reverse(&this_opcode.valuefield.value[offset]);
      if ((i=strlen(&this_opcode.valuefield.value[offset])) <= 16)
	setnibble(&this_opcode.valuefield.value[offset-1], i - 1);
      else
	zerrmsg("Invalid HEX constant (too large)");
    } else
      zerrmsg("Invalid HEX constant");
    break;
  case OPD_245_HEX:
    if (gethex(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset)) {
      i = strlen(&this_opcode.valuefield.value[offset]);
      if (i == 2 || i == 4 || i == 5) {
	this_opcode.length = offset + i;
	reverse(&this_opcode.valuefield.value[offset]);
	if (i != 2) {
	  /* if i == 4, add 1 to the nibble preceding 'offset' */
	  /* if i == 5, add 2 to the nibble preceding 'offset' */
	  cp = &this_opcode.valuefield.value[offset-1];
	  *cp = tohex[fromhex(*cp) + i - 3];
	}
      } else
	zerrmsg("Invalid HEX constant (not 2, 4, or 5 digits)");
    } else
      zerrmsg("Invalid HEX constant");
    break;
  case OPD_LENHEX:
    if (gethex(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset)
	&& strchr(SEPARATORS, *ptr) != (char *) NULL) {
      i = strlen(this_opcode.valuefield.value + offset) + entry->op_bias;
      if ((long)i >= (1L << 4 * offset))
	zerrmsg("Invalid HEX constant (too large)");
      else
	setnibbles(this_opcode.valuefield.value, i, offset);
    } else
      zerrmsg("Invalid HEX constant");
    break;
  case OPD_LENASC:
    if (getasc(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset)
	&& strchr(SEPARATORS, *ptr) != (char *) NULL) {
      i = strlen(this_opcode.valuefield.value + offset) / 2 + entry->op_bias;
      if ((long)i >= (1L << 4 * offset))
	zerrmsg("Invalid ASC constant (too large)");
      else
	setnibbles(this_opcode.valuefield.value, i, offset);
    } else
      zerrmsg("Invalid ASC constant");
    break;
  case OPD_STR:
  case OPD_ASC:
    if (getasc(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset)
	&& strchr(SEPARATORS, *ptr) != (char *) NULL) {
      if (entry->op_operand == OPD_ASC)
	breverse(&this_opcode.valuefield.value[offset]);
      if ((i=strlen(&this_opcode.valuefield.value[offset])) <= 16)
	setnibble(&this_opcode.valuefield.value[offset-1], i - 1);
      else
	zerrmsg("Invalid ASC constant (too large)");
    } else
      zerrmsg("Invalid ASC constant");
    break;
  case OPD_STRING:
    if (getasc(&this_opcode.valuefield.value[offset], OPCODE_LEN - offset)
	&& strchr(SEPARATORS, *ptr) != (char *) NULL) {
      i = strlen(this_opcode.valuefield.value);
      if (entry->op_bias == -1) {
	if (i >= OPCODE_LEN - 3)
	  zerrmsg("Invalid CSTR constant (too long)");
	else
	  strcpy(this_opcode.valuefield.value + i, "00");
      }
      else
	if (entry->op_bias == 1)	/* STRING opcode */
	  setnibble(&this_opcode.valuefield.value[i-1], 8);
    } else
      zerrmsg("Invalid ASC constant");
    break;
  case OPD_SYMBOL:
    exprptr = NULL;
    e_ptr = o_ptr = -1;
    if (getsymbol()) {
      if (*ptr != ')')
	exprptr = popexpr();
      else
	errmsg("Expression not allowed for INC(x), LINK, and SLINK");
    }
    if (exprptr != (EXPRPTR) NULL) {
      if (exprptr->e_extcount == 0
	  && exprptr->e_relcount == 0 && !absolute) {
				/* Add reference to "*00000" symbol */
	abs_expr.e_value = exprptr->e_value;
	exprptr = &abs_expr;
      }
      addfillref(exprptr);
    }
    break;
  default:
    error("operand type (%d) is not valid", entry->op_operand);
  }
	
  if (!this_opcode.length) {
    this_opcode.length = getlength();
  }
  if (this_opcode.length) {
    i = was_test | (flags & GOYES);
    was_test = flags & TEST;
    if (i == TEST)
      errmsg("Test instruction without GOYES/RTNYES");
    if (i == GOYES)
      errmsg("GOYES/RTNYES without test instruction");
    if (*label && (flags & GOYES)) {
      errmsg("Label not allowed on GOYES/RTNYES statement");
      *label = '\0';
    }
  }
  if (*label) {
    DBG1("Setting carry bits (labelled opcode)");
    carry = OP_C | OP_NC;
  }
  cstate = entry->op_carrystate;
  DBG3("Setting carry state (AND with %1X, OR with %1X)",
       (cstate >> 4) & 0xF,
       cstate & 0xF);
  carry = (carry | cstate) & (cstate >> 4) & 0xF;
  return (&this_opcode);
}

b_32 opcodelen(char *mnemonic)
{
  b_32 mylength = 0;
  OPCODEPTR opc;
  b_8 carrystate;
	
  if ((entry = all_findoptable(mnemonic)) != (OPTABLEPTR) NULL) {
    if (!assembling || (mylength=entry->op_length) == -1 
	|| addingmacro || entry->op_flags & PSEUDO_OP) {
      /* Need to figure out opcode to decide length */
      opc = getopcode(mnemonic);
      mylength = opc->length;
   } else {
      carrystate = entry->op_carrystate;
      DBG3("Setting carry state (AND with %1X, OR with %1X)",
	   (carrystate >> 4) & 0xF,
	   carrystate & 0xF);
      carry = (carry | carrystate) & (carrystate >> 4) & 0xF;
      /* Is the previous instruction was a test : to support the UP/EXIT MASD command */
      was_test = entry->op_flags & TEST;
    }
  } else
    if (addingmacro)
      putmacroline();
  return (mylength);
}
