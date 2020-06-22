#ifndef _SYMB_H_
#define _SYMB_H_

extern int lookups, lkcmps, adds;

extern b_16 symbols;
extern b_16 fillrefs;

extern void initsymbols(void);
extern char * symstats(void);
#define allsymbols symballsymbols
extern void allsymbols(void (*function)());
#define newsymbol symbnewsymbol
extern SYMBOLPTR newsymbol(char *);
#define newfillref symbnewfillref
extern FILLREFPTR newfillref(void);
#define findsymbol symbfindsymbol
extern SYMBOLPTR findsymbol(char *);
#define getsymbol symbgetsymbol
extern SYMBOLPTR getsymbol(char *);
#define definesymbol symbdefinesymbol
extern void definesymbol(char *);
#define addfillref symbaddfillref
extern void addfillref(SYMBOLPTR, O_FILLREFPTR);

#endif
