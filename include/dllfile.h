#ifndef dllfile 
#define dllfile


#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

#define fprintf myfprintf
#define printf myprintf
#define fputs myfputs
#define exit mydllexit

extern int myfprintf(FILE *f, char *fmt, ...);
extern void myprintf(char *fmt, ...);
extern void mydllexit(int i);
extern int myfputs(char *s, FILE *f);


typedef void (__stdcall *call_back)(char *);

extern call_back stdoutcb;
extern call_back stderrcb;

#define DllExport    __declspec( dllexport )


DllExport void PASCAL initdll( call_back out, call_back err );
DllExport UINT PASCAL rplcomp( int argc, char **argv );
DllExport UINT PASCAL sasm   ( int argc, char **argv );
DllExport UINT PASCAL makerom( int argc, char **argv );
DllExport UINT PASCAL sload  ( int argc, char **argv );

extern int rplcompmain(int argc, char ** argv);
extern int sasmmain(int argc, char ** argv);
extern int makerommain(int argc, char ** argv);
extern int sloadmain(int argc, char ** argv);

#endif