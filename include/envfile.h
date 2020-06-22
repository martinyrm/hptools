#ifndef	_ENVFILE_H_
#define	_ENVFILE_H_


#ifdef	__MSDOS__

#define DIRC '\\'
#define DRIVE ':'
#define BIN_READONLY "rb"
#define TEXT_READONLY "rt"
#define BIN_WRITE "wb"
#define TEXT_WRITE "wt"
#define BIN_READWRITE "w+b"
#define TEXT_READWRITE "w+t"
#define BIN_APPEND "ab"
#define TEXT_APPEND "at"
#define PATH_SEPARATOR ';'

#else	/*MSDOS*/

#define DIRC '/'
#define DRIVE '\0'
#define BIN_READONLY "r"
#define TEXT_READONLY "r"
#define BIN_WRITE "w"
#define TEXT_WRITE "w"
#define BIN_READWRITE "w+"
#define TEXT_READWRITE "w+"
#define BIN_APPEND "a"
#define TEXT_APPEND "a"
#define PATH_SEPARATOR ':'

#endif	/*MSDOS*/


extern int	debug;
extern FILE *dbgfile;
extern char *dbgname;

extern void error( char *, ... );

extern void DEBUG_PRN( char *, ... );
extern void	setname(char **, char *);


extern char	*nameptr;

extern void	fix_dirc( char *);
extern FILE *openfile(char *, char *, char *, char *, char **);

#endif
