#ifdef	DEBUG

extern int debug;
extern FILE *dbgfile;
extern char *dbgname;
extern void error();

#define DBG1(x) { \
    if (debug && (fprintf(dbgfile, x) < 0 || putc('\n', dbgfile) == EOF)) \
	error("error writing to %s", dbgname); \
}
#define DBG2(x,y) { \
    if (debug && (fprintf(dbgfile, x, y) < 0 || putc('\n', dbgfile) == EOF)) \
	error("error writing to %s", dbgname); \
}
#define DBG3(x,y,z) { \
    if (debug && (fprintf(dbgfile,x,y,z) < 0 || putc('\n', dbgfile) == EOF)) \
	error("error writing to %s", dbgname); \
}

#else	/*DEBUG*/

#define DBG1(x)		; /* Null statement */
#define DBG2(x,y)	; /* Null statement */
#define DBG3(x,y,z)	; /* Null statement */

#endif	/*DEBUG*/
