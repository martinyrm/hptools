#ifndef _SYMBOLS_H_
#define _SYMBOLS_H_

#define newsymbol symbolsnewsymbol
SYMBOLPTR	newsymbol(void);
LINEREFPTR	newlineref(void);
#define newfillref symbolsnewfillref
FILLREFPTR	newfillref(void);

void	s_initialize(void);
void	sys_initialize(void);
#define allsymbols symbolsallsymbols
void	allsymbols(void (*doit)(), SYMBOLPTR);
void	free_symb(SYMBOLPTR);
#define findsymbol symbolsfindsymbol
SYMBOLPTR	findsymbol(char *);
SYMBOLPTR	s_rotate(char *, SYMBOLPTR);
SYMBOLPTR	s_split(SYMBOLPTR, char *, SYMBOLPTR);
SYMBOLPTR	tree_addsymbol(SYMBOLPTR, char *, i_32, int);
SYMBOLPTR	addsymbol(char *, i_32, int);
SYMBOLPTR	sys_addsymbol(char *, i_32, int);
void	addlineref(SYMBOLPTR);
#define addfillref symbolsaddfillref
void	addfillref(EXPRPTR);
void	xaddlabel(char *, i_32, int, int);
void	addlabel(char *, i_32, int);
char	*createopenlabel(char *, int);
char	*createcloselabel(char *, int);
char	*createskiplabel(char *, int);
char	*pushopenlabel(char *);
char	*pushcloselabel(char *);
char	*pushskiplabel(char *);

#endif
