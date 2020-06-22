#include "sasm.h"
#include "debug.h"
#include "symbols.h"
#ifdef dll
#include "dllfile.h"
#endif

SYMBOL s_terminal = {"", 0, 0, 0, NULL, 0, NULL, NULL, NULL};
SYMBOLPTR s_root, sys_root, gg, g, f;

char	*pushopenlabel(char *label)
{
  /* Create the string for the label and prepare the MASD stack */
  createopenlabel(label,index_c1);
  if (level_stack == MAX_STACK) {
    errmsg("Too much SKIP");
    return NULL;
  }
  masd_stack[level_stack].c1 = index_c1++;
  masd_stack[level_stack].c2 = index_c2++;
  masd_stack[level_stack].c3 = index_c3++;
  level_stack++;
  return (label);
}

char	*createopenlabel(char *label, int index)
{
  sprintf(label,"lBloB1%X", index);
  return (label);
}

char	*createskiplabel(char *label, int index)
{
  sprintf(label,"lBloB1%Xs", index);
  return (label);
}

char	*pushcloselabel(char *label)
{
  if (level_stack <= 0)
    errmsg("Mismatched }");
  else {
    createcloselabel(label,masd_stack[level_stack--].c2);
  }
  return (label);
}

char	*createcloselabel(char *label, int index)
{
  sprintf(label,"lBlcB2%X", index);
  return (label);
}

SYMBOLPTR newsymbol()
{
    SYMBOLPTR temp;

    temp = (SYMBOLPTR) calloc(1, SYMBOLSIZE);
    if (temp == (SYMBOLPTR) NULL)
	error("error getting memory for %s", "symbol");
    DBG3("Address %ld: allocated %d bytes", (long) temp, SYMBOLSIZE)
    temp->s_left = &s_terminal;
    temp->s_right = &s_terminal;
    return (temp);
}

LINEREFPTR newlineref()
{
    LINEREFPTR temp;

    temp = (LINEREFPTR) calloc(1, LINEREFSIZE);
    if (temp == (LINEREFPTR) NULL)
	error("error getting memory for %s", "line reference");
    DBG3("Address %ld: allocated %d bytes", (long) temp, LINEREFSIZE)
    return (temp);
}

FILLREFPTR newfillref(void)
{
    FILLREFPTR temp;

    temp = (FILLREFPTR) calloc(1, FILLREFSIZE);
    if (temp == (FILLREFPTR) NULL)
	error("error getting memory for %s", "fill reference");
    DBG3("Address %ld: allocated %d bytes", (long) temp, FILLREFSIZE)
    return (temp);
}

void allsymbols(void (*doit)(), SYMBOLPTR x)
{
    SYMBOLPTR x2;

    if (x != &s_terminal) {
	if (x->s_left != &s_terminal)
	    allsymbols(doit, x->s_left);
	x2 = x->s_right;
	(*doit)(x);
	if (x2 != &s_terminal)
	    allsymbols(doit, x2);
    }
}

void free_symb(SYMBOLPTR sym)
{
    LINEREFPTR lp;
    FILLREFPTR fp;

    /* Line references */
    lp = sym->s_lineref;
    while (lp != (LINEREFPTR) NULL) {
	free((char *) lp);
	DBG2("Address %ld: free'd memory", (long) lp)
	lp = lp->l_next;
    }

    /* Fill references */
    fp = sym->s_fillref;
    while (fp != (FILLREFPTR) NULL) {
	free((char *) fp);
	DBG2("Address %ld: free'd memory", (long) fp)
	fp = fp->f_next;
    }

    /* The symbol itself */
    free((char *) sym);
    DBG2("Address %ld: free'd memory", (long) sym)
}

void s_initialize(void)
{
    /* s_name is set to "" by newsymbol() */
    s_terminal.s_left = &s_terminal;
    s_terminal.s_right = &s_terminal;

    /* free all existing symbols first */
    if (s_root != (SYMBOLPTR) NULL)
	allsymbols(free_symb, s_root);
    s_root = newsymbol();
}

void sys_initialize(void)
{
    /* initialize user symbol table */

    /* s_name is set to "" by newsymbol() */
    s_terminal.s_left = &s_terminal;
    s_terminal.s_right = &s_terminal;
    sys_root = newsymbol();
}

SYMBOLPTR findsymbol(char *name)
{
    int c;
    char *cp;
    SYMBOLPTR x;

    if (*name == '=' || *name == ':')
	++name;
    if ((cp = strchr(name, ' ')) != (char *) NULL && (cp - name) < SYMBOL_LEN)
	*cp = '\0';

    (void) strncpy(s_terminal.s_name, name, SYMBOL_LEN);

    /* First check user symbol tree */
    x = s_root;
    while ((c=strncmp(name, x->s_name, SYMBOL_LEN)) != 0)
	x = (c<0) ? x->s_left : x->s_right;
    if (x != &s_terminal)
	return x;

    /* Not found - check system symbol tree */
    x = sys_root;
    while ((c=strncmp(name, x->s_name, SYMBOL_LEN)) != 0)
	x = (c<0) ? x->s_left : x->s_right;
    return ((x == &s_terminal) ? (SYMBOLPTR) NULL : x);
}

SYMBOLPTR s_rotate(char *v, SYMBOLPTR y)
{
    int c;
    SYMBOLPTR s, gs;

    s = (c=(strncmp(v, y->s_name, SYMBOL_LEN) < 0)) ? y->s_left : y->s_right;
    if (strncmp(v, s->s_name, SYMBOL_LEN) < 0) {
	gs = s->s_left; s->s_left = gs->s_right; gs->s_right = s;
    } else {
	gs = s->s_right; s->s_right = gs->s_left; gs->s_left = s;
    }
    if (c)
	y->s_left = gs;
    else
	y->s_right = gs;
    return (gs);
}

SYMBOLPTR s_split(SYMBOLPTR root, char *sym, SYMBOLPTR x)
{
    x->s_red = TRUE;
    x->s_left->s_red = FALSE;
    x->s_right->s_red = FALSE;
    if (f->s_red) {
	g->s_red = TRUE;
	if ((strncmp(sym, g->s_name, SYMBOL_LEN) < 0)
	 == (strncmp(sym, f->s_name, SYMBOL_LEN) < 0))
	    f = s_rotate(sym, g);
	x = s_rotate(sym, gg);
	x->s_red = FALSE;
    }
    root->s_right->s_red = FALSE;
    return (x);
}

SYMBOLPTR tree_addsymbol(SYMBOLPTR root, char *symbol, i_32 value, int flags)
{
    int c;
    char *cp;
    SYMBOLPTR x;

    if (*symbol == ':')
	++symbol;
    else if (*symbol == '=') {
	++symbol;
	flags |= S_EXTERNAL;
    }
    if ((cp = strchr(symbol, ' ')) != (char *) NULL && (cp-symbol) < SYMBOL_LEN)
	*cp = '\0';

    (void) strncpy(s_terminal.s_name, symbol, SYMBOL_LEN);
    gg = g = f = x = root;
    while ((c=strncmp(symbol, x->s_name, SYMBOL_LEN)) != 0) {
	gg = g; g = f; f = x; x = (c<0) ? x->s_left : x->s_right;
	if (x->s_left->s_red && x->s_right->s_red)
	    x = s_split(root, symbol, x);
    }
    if (x == &s_terminal) {
	x = newsymbol();
	(void) strncpy(x->s_name, symbol, SYMBOL_LEN);
	x->s_value = value;
	x->s_flags = (b_8) flags;
	if (strncmp(symbol, f->s_name, SYMBOL_LEN) < 0)
	    f->s_left = x;
	else
	    f->s_right = x;
	(void) s_split(root, symbol, x);
    }
    return (x);
}

SYMBOLPTR addsymbol(char *symbol, i_32 value, int flags)
{
  return tree_addsymbol(s_root, symbol, value, flags);
}

SYMBOLPTR sys_addsymbol(char *symbol, i_32 value, int flags)
{
    return tree_addsymbol(sys_root, symbol, value, flags);
}

void addlineref(SYMBOLPTR sym)
{
    LINEREFPTR *l;

    l = &sym->s_lineref;

    while (*l != (LINEREFPTR) NULL)
	l = &(*l)->l_next;
    *l = newlineref();
    (*l)->l_line = currentline;
}

void addfillref(EXPRPTR expr)
{
    SYMBOLPTR sym;
    FILLREFPTR *fp;
    static EXPR temp;
    int type = S_EXTERNAL;
    i_32 value = 0;

    if (!pass2 || (expr->e_extcount == 0 && expr->e_relcount == 0))
	return;
    if (expr->e_relcount != 0 && entry->op_class != 1) {
	temp = *expr;
	(void) strncpy(temp.e_name, prog_symbol, SYMBOL_LEN);
	/*temp.e_value -= codestart;*/
	expr = &temp;
	type = S_RESOLVED | S_ENTRY | S_RELOCATABLE;
	value = codestart;
    }
    if ((sym = findsymbol(expr->e_name)) == (SYMBOLPTR) NULL)
	sym = addsymbol(expr->e_name, value, type);
    fp = &sym->s_fillref;
    ++sym->s_fillcount;

    while (*fp != (FILLREFPTR) NULL)
	fp = &(*fp)->f_next;
    *fp = newfillref();
    (*fp)->f_address = pc + entry->op_offset;
    if (sym->s_flags & S_RESOLVED)
	(*fp)->f_bias = expr->e_value - sym->s_value;
    else
	(*fp)->f_bias = expr->e_value;
    (*fp)->f_class = entry->op_class;
    (*fp)->f_subclass = entry->op_subclass;
    (*fp)->f_length = entry->op_size;
}

/* Added conversion of offset labels to normal labels:
 *      "--" --> "lBm2%X"
 *      "-"  --> "lBm1%X"
 *      "+"  --> "lBp1%X"
 *      "++" --> "lBp2%X"
 */

void	xaddlabel(char *label, i_32 value, int flags, int redefine)
{
  char		*lab = label;
  SYMBOLPTR	x;
  
  if(*label == ':')
    lab++;
  else
    if (*label == '=') {
      lab++;
      flags |= S_ENTRY;
    }
  
  if (*lab == '\0') {
    errmsg("Invalid symbol");
    return;
  }

  if (pass1) {
    x = addsymbol(label, value, flags);
    addlineref(x);
  }
  if (pass2) {
    if ((x=findsymbol(label)) != (SYMBOLPTR) NULL) {
      if ((x->s_flags & S_SETPASS2) && redefine == FALSE)
	errmsg("Duplicate symbol");
      else {
	x->s_flags |= S_SETPASS2;
	if (x->s_value != value && redefine == FALSE)
	  errmsg("Symbol changed (use old value)");
      }
    } else {
      errmsg("Symbol was not defined in pass 1");
    }
  }
  if (redefine && x != (SYMBOLPTR) NULL)
    x->s_value = value;
}

void addlabel(char *label, i_32 value, int flags)
{
    xaddlabel(label, value, flags, FALSE);
}
