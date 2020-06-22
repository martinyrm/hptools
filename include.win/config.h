/* This file should be included only if you compile under Microsoft PC */

#ifndef	_CONFIG_H_
#define	_CONFIG_H_


#undef BIGENDIAN
#define LITTLEENDIAN


/* comment this if you don't have flat linear memory model */
#define	__FLAT_MEM__

/* uncomment this if you have MeSsyDOS segmented memory model */
/* #define	__FREE__
 */

/* comment this if you don't want debug reports inlined in code */
#define	__DEBUG__

/* uncomment this if you don't have getopt() call */
/* #define	__GETOPT__
 */


#ifndef	TRUE
# define	FALSE	0
# define	TRUE	(!FALSE)
#endif

#if	defined(__FREE__)
# define	FREE(x)	farfree(x)
#else
# define	FREE(x)	free(x)
#endif



/* RPLCOMP stuff here */
/* #if	defined(__RPLCOMP__) */

/* #endif */



/* SASM stuff here */
/* #if	defined(__SASM__) */

/* expression operator's stack depth */

#define LINE_LEN	256

#define LINE_LEN	256
/* length of a mnemonic */
#define MNEMONIC_LEN	12
#define OPCODE_LEN	80

/* length of a label */
#define SYMBOL_LEN	36

#define MAX_STACK	255

#define MAX_NIBBLES	0x100000
#define COMMENT_COLUMN	30
#define MAXCONDITIONALS	20
#define IMSTACKSIZE	20

#define	OP_STACK_MAX	20


#define NUMBER_OF_FLAGS	100
#define FLAG_SET	1
#define FLAG_CMDLINE	2
#define FLAG_CLEAR	(~ FLAG_SET)

#define LOOP_STACK_MAX	4

/*************************************
 * End of user configuration section *
 *************************************/

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
#undef _ALL_SOURCE
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#undef TIME_WITH_SYS_TIME

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if you have the getopt function.  */
#define HAVE_GETOPT 1

/* Define if you have the strcspn function.  */
#define HAVE_STRCSPN 1

/* Define if you have the <dos.h> header file.  */
/* #undef HAVE_DOS_H */

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the sys_errlist declared.  */
/* #undef SYS_ERRLIST_DECLARED */

/* Define to be the name of the operating system.  */
#define OS_TYPE "win32"

/* To insure MSDOS compatibility */
#define __MSDOS__

#if 0 /* need more info about other unices */
/* SunOS brain damage... */
#ifndef STDC_HEADERS
char *strstr();
char *strchr();
void *memcpy();
#endif /* STDC_HEADERS */

#ifndef HAVE_STRERROR
#  define mystrerror(x) (sys_errlist[x])
extern char *sys_errlist[];
#else /* no HAVE_STRERROR */
#  define mystrerror(x) strerror(x)
#endif /* HAVE_STRERROR */
#endif

#endif /* _CONFIG_H_ */
