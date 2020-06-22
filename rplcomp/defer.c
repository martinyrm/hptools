
#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
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
#include <ctype.h>

#include "rplcomp.h"
#include "defer.h"

#ifdef dll
#include "dllfile.h"
#endif

void	drop_ary(int flag, struct LINE *line)
{
  struct LINE *next;

  next = line;
  while((line = next) != NULL) {
    next = line->next;
    if(flag)
      fputs(line->line, outfp);
    FREE(line->line);
    FREE(line);
  }
}

struct ARRY	*push_ary(void)
{
  struct ARRY	*tempo;

  if((tempo = (struct ARRY *)calloc(1, sizeof(struct ARRY))) == NULL)
    err_std("Cannot malloc ARRY BLK\n");
  return tempo;
}

void	add_obj(struct ARRY *arryp)
{
  struct ITEM	*tempo;

  if((tempo = (struct ITEM *)calloc(1, sizeof(struct ITEM))) == NULL)
    err_std("Cannot malloc ITEM BLK\n");
  if(arryp->itm_last == NULL)
    arryp->items = tempo;
  else
    arryp->itm_last->next = tempo;
  arryp->itm_last = tempo;
}
