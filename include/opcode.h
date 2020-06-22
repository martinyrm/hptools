#ifndef _OPCODE_H_
#define _OPCODE_H_


int	pushcond(void);
int	checkcond(void);
EXPRPTR	doexpr_fill(void);
void	zerrmsg(char *);
OPTABLEPTR	getopmem(int);
OPTABLEPTR	add_optable(OPTABLEPTR, OPTABLEPTR *);
void	readoptable(void);
char	*getterm(void);
char	*uppercase(char *);
int	getfs(void);
EXPRPTR	getdigitexpr(void);
void	reverse	(char *);
void	breverse(char *);
int	decode(char **);
int	getasc(char *, int);
int	getbin(char *, int ,int);
int	gethex(char *, int);
void	alloptable(void (*doit)(), OPTABLEPTR);
void	dofree(OPTABLEPTR);
void	cleanmacros(void);
OPTABLEPTR	findoptable(char *, OPTABLEPTR);
OPTABLEPTR	all_findoptable(char *);
#define fromhex opcodefromhex
int	fromhex(char);
void	setnibbles(char *, int, int);
void	setnibble(char *, int);
void	clearnibble(char *, int);
void	changenibble(char *, int);
void	setexpr(i_32);
void	setfield(int);
b_32	getlength(void);
struct loop	*newloop(void);
OPTABLEPTR	newmacro(char *);
#define getobject opcodegetobject
O_SYMBOLPTR	getobject(int *);
void	rdsymb(void);
void	hpcharmap(void);
void	putmacroline(void);
OPCODEPTR	getopcode(char *);
b_32	opcodelen(char *);


extern int	was_test;
/* external variables used in opcode.c */

extern OPTABLEPTR	optable_root;
extern OPTABLEPTR	macro_root;
extern OPTABLEPTR	entry;
extern OPTABLEPTR	macroentry;

extern OPTABLE	opcodes[];
#endif
