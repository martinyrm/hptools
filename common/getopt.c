
#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef	HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#if	defined(__MSDOS__)
#include <io.h>
#endif

#include "getopt.h"

#ifdef dll
#include "dllfile.h"
#endif

int	optopt, opterr = TRUE, optind = 1;
char	*optarg;

static	int	sp = 1;

int getopt( int argc, char * const argv[], const char *opts )
{
  char *cp, errbuf[2];
  register char ch;

  if( sp == 1 ) {
    if( argc <= optind )
      return -1;
    if( ( errbuf[0] = argv[optind][0] ) == '\0' )
      return -1;
    if( !strchr( "/-", errbuf[0] ) )
      return -1;
    if( argv[optind][1] == '\0' )
      return -1;
    if( !strcmp( argv[optind], "--" ) ) {
      optind++;
      return -1;
    }
  }
  ch = optopt = argv[optind][sp];
  if( ch == ':' || ( cp = strchr( opts, ch ) ) == NULL ) {
    if( !opterr ) {
      errbuf[0] = ch;
      errbuf[1] = '\n';
      WRITE( ": illegal option -- " );
    }
    sp++;
    if( argv[optind][sp] == '\0' ) {
      optind++;
      sp = 1;
    }
    return '?';
  }
  if( *(++cp) == ':' ) {
    if( argv[optind][sp + 1] != '\0' ) {
      optarg = argv[optind++] + sp + 1;
      sp = 1;
      return ch;
    }
    if( argc > ++optind ) {
      optarg = argv[optind++];
      sp = 1;
      return ch;
    }
    if( opterr ) {
      errbuf[0] = ch;
      errbuf[1] = '\n';
      WRITE( ": option requires an argument -- " );
    }
    sp = 1;
    return '?';
  }
  if( argv[optind][++sp] == '\0' ) {
    sp = 1;
    optind++;
  }
  optarg = NULL;
  return ch;
}
