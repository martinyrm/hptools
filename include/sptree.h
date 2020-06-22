#ifndef _SPTREE_H_
#define _SPTREE_H_


#define STRCMP(a,b) ((Sct= *(a)-*(b)) ? Sct : (Sct = strcmp((a), (b)) ) )
#ifdef TUNE
#define Tune(x) x
#else
#define Tune(x)
#endif

extern SYMBOL *splook(char *, SYMBOLPTR *, int);
extern void spscan(SYMBOLPTR, void (*function)());


#endif
