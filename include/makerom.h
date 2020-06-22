#ifndef	_MAKEROM_H_
#define	_MAKEROM_H_

#define	MAXCOMLEN	20
#define	MAXBUFSIZE	80
#define MAXNAMELEN	256

#define	CHECK(a)	if(!(a)) { puts("Can't alloc memory"); exit(1); }
#define	SKIPSPC(s)	for(;s&&(*s==' '||*s=='\t'); s++)
#define	KILLNEWLINE(q)	if(*(q+strlen(q)-1)=='\n') *(q+strlen(q)-1)='\0'; else {};
#define	KILLRETURN(q)	if(*(q+strlen(q)-1)=='\r') *(q+strlen(q)-1)='\0'; else {};
#define	null(type)	(type) 0L

#define mystrcmp makerommystrcmp
int
chtoi(char),
  myatoi(char **),
  mystrcmp(char *, char *),
  mystrcmplen(char *, char *);
#define mystrcpy makerommystrcpy
#define conc_ext makeromconc_ext
char
*mystrcpy(char *),
  *conc_ext(char *, char *, int),
  *get_token(char **, char *);
void
makeromusage(int),
  parse_configure(char *, char *, char *, char *),
  parse_mesgs(char *, char * , char *, char *),
  parse_name(char *, char *, char *, char *),
  parse_rel(char *, char *, char *, char *),
  parse_head(char *, char *, char *, char *),
  parse_hash(char *, char * , char *, char *),
  parse_table(char *, char *, char *, char *),
  parse_finish(char *, char *, char *, char *),
  parse_end(char *, char *, char *, char *),
  parse_line(char *),
  make_hash(FILE *),
  make_link(FILE *),
  make_end(void),
  make_files(void),
  parse_external(char *),
  read_ext(char *);

extern	int	getopt(int, char * const [], const char *);

#endif
