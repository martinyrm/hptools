
#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>

#ifdef dll
#include "dllfile.h"
#endif

extern FILE *dbgfile;
extern int debug;

void	DEBUG_PRN( char *fmt_str, ... )
{
#ifdef	__DEBUG__
	va_list	arg_ptr;

	if( debug ) {
		va_start( arg_ptr, fmt_str );
		vfprintf( dbgfile, fmt_str, arg_ptr );
		va_end( arg_ptr );
	}
#endif
}
