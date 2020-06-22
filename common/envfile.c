
#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef	HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifndef	macintosh
# include <malloc.h>
#endif

#include "envfile.h"

#ifdef dll
#include "dllfile.h"
#endif

char	*nameptr;

void	fix_dirc( char *pathname )
{
#if	defined(__MSDOS__)
  while( ( pathname = strchr( pathname, '/' ) ) != NULL )
    *pathname++ = '\\';
#else
  while( ( pathname = strchr( pathname, '\\' ) ) != NULL )
    *pathname++ = '/';
#endif
}

void	setname( char **nameaddr, char *name )
{
  if( *nameaddr != NULL ) {
    FREE( *nameaddr );
    DEBUG_PRN( "Address %ld: free'd memory\n", *nameaddr );
  }
  if( ( *nameaddr = calloc( 1, strlen( name ) + 1 ) ) == NULL )
    error( "error getting memory for %s", name );
  DEBUG_PRN( "Address %ld: allocated %d bytes\n", *nameaddr, strlen( name ) + 1 );
  strcpy( *nameaddr, name );
  fix_dirc( *nameaddr );
}

FILE *openfile(char *filename, char *envname, char *defaultpath, char *mode, char **actpath) /* return actual successful path used */
{
  FILE *fp;
  char *cp, *path, *base, *filebase;
  char drive_letter = '\0', path_letter = '\0';
  int len, namelen;
  int dot_done = FALSE;	/* have checked current directory */
  int done_default = FALSE;	/* have set cp = defaultpath */
  int got_term = TRUE;	/* skipped a PATH_SEPARATOR at end of path */

  /*
   * If filename has >= 2 chars and second char is DRIVE, save drive_letter
   *   and skip drive (2 characters) --> filebase
   * If first character of filebase is DIRC or '.', use filename alone
   * If environment variable doesn't exist, check default path
   * A PATH_SEPARATOR at either end of a path means check "."
   * A null path means use "."
   * Two consecutive PATH_SEPARATORs means check "."
   * Return pointer to the first file found which can be opened
   */
  if (defaultpath == NULL)
    defaultpath = ".";

  namelen = strlen(filename) + 2;	/* space for DIRC, filename, '\0' */

  filebase = filename;
  if (strlen(filename) >= 2 && filename[1] == DRIVE) {
    drive_letter = *filename;
    filebase = &filename[2];
  }

  if ((cp=strchr(filebase,DIRC)) == (char *) NULL
      && (cp=strchr(filebase,'/')) == (char *) NULL)
    cp = filebase - 1;

  len = cp - filebase;
  if (len < 0 || (len > 0 && strncmp(filebase, "..", len) != 0)) {
    if (*envname == '\0' || (path=getenv(envname)) == (char *) NULL) {
      /* environment variable not specified; use default path */
      path = defaultpath;
      done_default = TRUE;
      got_term = TRUE;
    }
    /* path points to the path to try */
    len = 0;
    while (len >= 0) {
      /* Make default path letter match filename drive letter */
      path_letter = drive_letter;

      switch(*path) {
      case PATH_SEPARATOR:	/* path character */
	while (*path == PATH_SEPARATOR)
	  ++path;
	len = 0;
	break;
      case '\0':	/* terminating NULL */
	/* if terminator and have not done ".", len = 0 (else -1) */
	len = (got_term && dot_done == FALSE) ? 0 : -1;
	break;
      default:
	base = path;
	while (*path != '\0' && *path != PATH_SEPARATOR)
	  ++path;
	len = path - base;
	if (len >= 2 && base[1] == DRIVE) {
	  /* A drive letter is specified in the path */
	  path_letter = *base;
	  base += 2;
	  len -= 2;
	}
	got_term = FALSE;
	if (*path == PATH_SEPARATOR) {	/* skip to next path */
	  ++path;
	  got_term = TRUE;
	}
	if (len == 1 && *base == '.') {
	  /* path specified is "." */
	  len = 0;	/* convert to internal representation of "." */
	}
      }

      if (drive_letter != '\0' && drive_letter != path_letter) {
	/* Drive letters don't match - ignore this one */
	len = 0;
	continue;
      }

      if (len < 0) {
	if (*path == '\0' && done_default == FALSE) {
	  path = defaultpath;
	  done_default = TRUE;
	  got_term = FALSE;
	  len = 0;
	}
	continue;	/* next iteration of while (len >= 0) loop */
	/* note that len < 0 will terminate while () loop */
      }

      if (len == 0 && dot_done)
	continue;	/* next iteration of while (len >= 0) loop */

      /* Allocate room for length of path name (len) + length of
       * filename (namelen) + possible drive letter (2)
       * (namelen includes 2 extra chars - one for DIRC, one for '\0')
       */
      if ((nameptr=calloc(1,(unsigned)(len+namelen+2))) == (char *) NULL)
         error("error getting memory for %s", filename);
      DEBUG_PRN("Address %ld: allocated %d bytes", (long) nameptr, len+namelen);
      /* nameptr points to the name area (zeroed by calloc) */
      if (path_letter != '\0') {
	*nameptr = path_letter;
	nameptr[1] = DRIVE;
      }
      if (len == 0) {
	/* This is "." (current directory) */
	dot_done = TRUE;
      } else {
	/* Some directory other than "."; copy it, add DIRC after it */
	(void) strncat(nameptr, base, len); /* append to nameptr */
	len = strlen(nameptr);
	if (nameptr[len-1] != DIRC) {
	  /* Add DIRC only if not already there */
	  nameptr[len] = DIRC;
	}
      }
      (void) strcat(nameptr, filebase);

      fix_dirc(nameptr);

      if ((fp=fopen(nameptr,mode)) != (FILE *) NULL) {
	if (actpath != NULL)
	  *actpath = nameptr;
	return fp;
      }
      free(nameptr);
      DEBUG_PRN("Address %ld: free'd memory (openfile rel)", (long) nameptr);
    }
    if (actpath != NULL)
      *actpath = NULL;
    return (FILE *) NULL;
  }
  /* absolute path specified for name; ignore environment var, default path */
  if ((nameptr=calloc(1, (unsigned) namelen)) == (char *) NULL)
     error("error getting memory for %s", filename);
  DEBUG_PRN("Address %ld: allocated %d bytes", (long) nameptr, namelen);
  /* nameptr points to the name area */
  (void) strcpy(nameptr, filename);

  /* Change all occurances of '/' to DIRC */
  fix_dirc(nameptr);

  if ((fp=fopen(nameptr,mode)) != (FILE *) NULL) {
    if (actpath != NULL)
      *actpath = nameptr;
    return fp;
  }
  free(nameptr);
  DEBUG_PRN("Address %ld: free'd memory (openfile abs)", (long) nameptr);
  if (actpath != NULL)
    *actpath = NULL;
  return (FILE *) NULL;
}
