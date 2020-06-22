#ifndef _INPUT_H_
#define _INPUT_H_


/* local declarations */
extern int column, lastcolumn;
extern char *lastptr;

char read_char(void);
void unread_char(void);
#define getfield inputgetfield
int getfield(char *, int, char *);
#define skipwhite inputskipwhite
void skipwhite(void);
void skipsep(void);
char *sloadgetline(char *, FILE *);
i_32 read_addr(void);
i_16 read_cmd(void);
char *read_filespec(void);
char *read_symbol(char *);
char *read_string(void);
#define copyline inputcopyline
void copyline(char *, char *, int);

#endif
