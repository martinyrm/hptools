#ifndef _EXPR_H_
#define _EXPR_H_

#define MAXSTACK 20
#define START '\n'
#define opstring &operator[1]

#define LESSEQUAL 0x89
#define MOREEQUAL 0x8A
#define DIFFERENT 0x8B

EXPRPTR	doexpr(void);
int	absexpr(EXPRPTR);
char	seechar(void);
char	nextchar(void);
int	getop(char);
int	pushop(char);
int	seeop(void);
int	popop(void);
int	pushexpr(EXPRPTR);
EXPRPTR	popexpr(void);
#define getsymbol exprgetsymbol
int	getsymbol(void);
int	nextexpr(char);
int	doop(int);

#endif
