#ifndef	_RPLCOMP_H_
#define	_RPLCOMP_H_

#include <stdio.h>

#include "config.h"

#define ENVLAM	"ADRLAM"
#define DIFPUTGET	-0x5                   /* wgg correct HP49 {{ }} generation */
#define	MAXCOMLEN	20
#define	MAXBUFSIZE	4096
#define	MAXTOKEN	256

#define MAXDEPTH	1024

#define MAXNAMESIZE	256

#define MAXINCLUDE	64

#define CHECK(a)        if(!(a)) { puts("Can't alloc memory"); exit(1); }
#define CHKEND(a)       (a != '\n' && a != '\0')
#define SKIPSPC(s)      for(;s&&(*s==' '||*s=='\t'); s++)
#define KILLNEWLINE(q)  if(*(q+strlen(q)-1)=='\n') *(q+strlen(q)-1)='\0'; else {};
#define KILLRETURN(q)   if(*(q+strlen(q)-1)=='\r') *(q+strlen(q)-1)='\0'; else {};
#define null(type)      (type) 0L

struct LINE {
	char    *line;
	struct LINE     *next;
};

struct ITEM {
	struct LINE     *item;
	struct LINE     *last;
	struct ITEM     *next;
};

struct ARRY {
	char    *type;
	struct LINE *dimens;
	struct LINE *dim_last;
	struct ITEM     *items;
	struct ITEM     *itm_last;
};

struct FILES {
	FILE    *fp;
	char    *name;
	int     line;
};

#define romid rplcompromid
extern int	debug, force_comment, commented, in_next_file, romid, exiterr;
extern unsigned short	ctrl_stack[MAXDEPTH];
extern unsigned int	ctrl_lines[MAXDEPTH];
extern int	fieldnumber, err_count, c_stack_level, label_cnt;
extern FILE	*outfp, *extfp , *dbgfile;
#define out_name rplcompout_name
extern char	*in_name, *out_name, *ext_name, *dbgname;
extern char	*progname;
extern char	*fields[MAXTOKEN], token_buf[MAXBUFSIZE], line_buf[MAXBUFSIZE];
extern char	str_buf[MAXBUFSIZE], name_buf[MAXNAMESIZE], *romid_num;
extern unsigned	char	romid_flag;
extern struct	HASHROOT	*hash_root;
extern struct	ARRY	*arryp;
extern struct	FILES	infiles[MAXINCLUDE], *infp;


extern void	rplcompusage(int);
extern void	*myexit(int);
#define mystrcpy rplcompmystrcpy
extern char	*mystrcpy(char *);
#define mystrcmp rplcompmystrcmp
extern int	mystrcmp(char *, char *);
#define conc_ext rplcompconc_ext
extern char	*conc_ext(char *, char *);
extern int	check_ctrl_stk(int);
extern void	add_ctrl_stk(int);
extern void	err_std(char *);
extern struct LINE	*make_line(void);
extern void	output(char *);
extern void	outputf(char *, ...);
extern void	out_ch(char);
extern void	stack_err(char *);
extern void	writeline(char *, ...);
extern void	end_file(void);
extern char	*nextqfield(void);
extern char	*nextfield(void);
extern void	dofields(void);
extern void	rplcompgetline(void);
extern void	prologue(char *);
extern void	doarry(int);
extern void	out_e_real(char *);
extern void	outreal(char *);
extern char	*real_exp(char *, int, int, long);
extern void	doreal(void);
extern void	docmp(void);
extern void	doereal(void);
extern void	doecmp(void);
extern int	isrealnum(char *);
extern void	dobint2(char *);
extern void	dohexnum(char *);
extern void	doromp(char *);
extern void	doflashp(char *);
extern void	doname(int, char *);
extern void	doinclob(void);
extern void	donameless(int);
extern void	doext(void);
extern void	dofext(void);
extern void	dolocal(void);
extern void	dodefine(void);
extern void	doromid(int);
extern void	dotitle(char *);
extern void	doconditional(int, char *);
extern void	dothen(char *);
extern void	doparen(void);
extern void	dosemi(char *);
extern int	istoken(char *);
extern void	docode(char *);
extern void	doendcode(void);
extern void	dorpl(void);
extern void	doassemble(char *);
extern int	get_str(char *, unsigned char, int *, char **);
extern void	dostring(char *, char *, unsigned char);
extern void	dochr(void);
extern void	dohxs(char *, char *);
extern void	dozint(char *, char *);
extern void	dotag(void);
extern void	doinclude(void);
extern void	doendbind(void);
extern void	dobind(void);
extern int	stuff_lambda(char *, char *, int);     /* wgg  */
extern int	parse_token(char *);
extern int	rpl2asmb(int count);
extern void	rplcompget_options(int, char **);
extern void	RPLCompError(int err, char *fmt_str, ...); 

#endif
