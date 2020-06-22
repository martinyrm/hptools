#include "dllfile.h"

call_back stdoutcb;
call_back stderrcb;

int myfprintf(FILE *f, char *fmt, ...)
{
	va_list ap;
	static char buf[10000];

	va_start(ap, fmt);

	if (f==stderr)
	{
		vsprintf(buf, fmt, ap);
		if (NULL!=stderrcb)
			stderrcb(&(buf[0]));
	}
	else if (f==stdout)
	{
		vsprintf(buf, fmt, ap);
		if (NULL!=stdoutcb)
			stdoutcb(&(buf[0]));
	}
	else
		vfprintf(f, fmt, ap);
	va_end(ap);

	return 50;
}

void myprintf(char *fmt, ...)
{
	va_list ap;
	static char buf[10000];
	va_start(ap, fmt);

	vsprintf(buf, fmt, ap);
	if (NULL!=stdoutcb)
		stdoutcb(&(buf[0]));
	va_end(ap);
}

int myfputs(char *s, FILE *f)
{
	if (f==stderr)
	{
		if (NULL!=stderrcb)
			stderrcb(&(s[0]));
	}
	else if (f==stdout)
	{
		if (NULL!=stdoutcb)
			stdoutcb(&(s[0]));
	}
	else
	{
#undef fputs
		fputs(s, f);
#define fputs myfputs
	}
	return EOF+1;
}

void mydllexit(int i)
{
	char *a;
	a = (char *)i;
	a[0] = 0;
}

void PASCAL initdll( call_back out, call_back err )
{
	  stdoutcb = out;
	  stderrcb = err;
}

UINT PASCAL rplcomp( int argc, char **argv )
{
	return rplcompmain(argc, argv);
}

UINT PASCAL sasm   ( int argc, char **argv )
{
	return sasmmain(argc, argv);
}

UINT PASCAL makerom( int argc, char **argv )
{
	return makerommain(argc, argv);
}

UINT PASCAL sload  ( int argc, char **argv )
{
	return sloadmain(argc, argv);
}
