#ifdef dll
#include "dllfile.h"
#endif
#include "sload.h"
#include "debug.h"

#include "symb.h"
#include "sptree.h"


static SYMBOLPTR symtree;
int lookups=0, lkcmps=0, adds=0;

b_16 symbols;
b_16 fillrefs;


void initsymbols(void)
{

    /* Initialize symbol counter */
    symbols = 0;
    fillrefs = 0;

    /* Initialize symbol tree */
    symtree = NULL;
}

char * symstats(void)
{
    static char stats[50];

    (void) sprintf(stats,"f(%d %4.2f) i(%d)\n",lookups,
		lookups ? (float)lkcmps / lookups : 0.0, adds);
    return stats;
}

void allsymbols(void (*function)())
{
    (void) spscan(symtree, function);
}

SYMBOLPTR newsymbol(char *name)
{
    register SYMBOLPTR ptr;

    DBG2("Adding symbol '%s' to symbol table", name);
    if ((ptr=(SYMBOLPTR) malloc(SYMBOLSIZE)) == (SYMBOLPTR) NULL)
	error("error malloc'ing %ld bytes for new symbol", (long) SYMBOLSIZE);
    DBG3("Address %ld: allocated %d bytes (newsymbol)", (long) ptr, SYMBOLSIZE);
    (void) strncpy(ptr->s_name, name, SYMBOL_LEN);
    ptr->s_name[SYMBOL_LEN] = '\0';
    ptr->s_value = 0;
    ptr->s_fillcount = S_RELOCATABLE;
    ptr->s_module = lastmodule;
    ptr->s_fillref = (FILLREFPTR) NULL;
    ptr->s_filltail = &ptr->s_fillref;	/* set tail of reference queue */

    ++symbols;
    return ptr;
}

FILLREFPTR newfillref(void)
{
    register FILLREFPTR ptr;

    DBG1("Adding fill reference to symbol table");
    if ((ptr=(FILLREFPTR) malloc(FILLREFSIZE)) == (FILLREFPTR) NULL)
	error("error malloc'ing %ld bytes for new fill reference",
							(long) FILLREFSIZE);
    DBG3("Address %ld: allocated %d bytes (newfillref)", (long)ptr,FILLREFSIZE);
    ptr->f_next = (FILLREFPTR) NULL;
    ++fillrefs;
    return ptr;
}

SYMBOLPTR findsymbol(char *symbname)
{
    register char * cp;

	/* Change the first blank into a NUL ('\0') */
    symbname[SYMBOL_LEN] = '\0';
    if ((cp=strchr(symbname, ' ')) != (char *) NULL) {
	*cp = '\0';
    }

    return splook(symbname, &symtree, 0);
}

SYMBOLPTR getsymbol(char *symbname)
{
    register SYMBOLPTR symbptr;
    register char * cp;

    if (! isascii(*symbname) || ! isgraph(*symbname)) {
	/* Illegal character (not printable) */
	errmsg("Illegal first character in symbol");
    }

	/* Change the first blank into a NUL ('\0') */
    symbname[SYMBOL_LEN] = '\0';
    if ((cp=strchr(symbname, ' ')) != (char *) NULL) {
	*cp = '\0';
    }

    symbptr = splook(symbname, &symtree, 1);

    /* symbptr points to the requested symbol record */
    return symbptr;
}

void definesymbol(char *symbol)
{
    char *cp;
    char symbname[SYMBOL_LEN+1];
    long value = 1;	/* default value is one */
    int len;
    SYMBOLPTR symbptr;

    if (*symbol == '=')
	++symbol;

    if ((cp=strrchr(symbol, '=')) != (char *) NULL)
	len = cp - symbol;
    else
	len = strlen(symbol);

    if (len > SYMBOL_LEN)
	len = SYMBOL_LEN;

    (void) memcpy(symbname, symbol, len);
    symbname[len] = '\0';	/* terminate the name with a null */
    /* Figure out value, if any given */
    if (cp !=(char *) NULL && cp[1] != '\0' && sscanf(cp+1,"%ld",&value) != 1) {
	(void) fprintf(stderr, "Non-numeric expression for -D (%s)\n", cp + 1);
	++errorcount;
	return;
    }
    symbptr = getsymbol(symbname);
    symbptr->s_value = value;
    symbptr->s_fillcount = S_RESOLVED;
    symbptr->s_module = &define_module;
}

void addfillref(SYMBOLPTR sym, O_FILLREFPTR reference)
{
    register FILLREFPTR tp;

    *sym->s_filltail = tp = newfillref();
    sym->s_filltail = &tp->f_next;
    ++sym->s_fillcount;	/* Adjust fill count to reflect new reference */
    tp->f_address = get_32(reference->of_address) + lastmodule->m_start;
    tp->f_bias = get_32(reference->of_bias);
    tp->f_class = get_u8(reference->of_class);
    tp->f_subclass = get_u8(reference->of_subclass);
    tp->f_length = get_16(reference->of_length);
    tp->f_module = lastmodule;
}
