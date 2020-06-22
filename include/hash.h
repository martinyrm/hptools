#ifndef	_HASH_H_
#define	_HASH_H_

#ifdef	HAVE_CONFIG_H
#  include "config.h"
#endif

#define	HASH_F_0	0
#define	HASH_F_EXT	1
#define	HASH_F_LOC	2
#define	HASH_F_DEF	3
#define HASH_F_FEXT	4

struct HASH {
  struct HASH *prev;
  unsigned char	flag;
  int	lambda;
  char	hash[SYMBOL_LEN];
  char	*define;
};

struct HASHROOT {
  struct HASH	**hashptr;
  int	max;
  int	count;
};

void	hash_err(char *);
struct HASHROOT	*hashmake(int);
int	hashchck(char *, struct HASHROOT *);
struct HASH
*hashadd(struct HASHROOT *, char *, int),
  *hashfind(struct HASHROOT *, char *);

#endif
