
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

#include "hash.h"

#ifdef dll
#include "dllfile.h"
#endif

void	hash_err( char *errstr )
{
	fprintf( stderr, "hash: %s", errstr );
	exit( 1 );
}

struct HASHROOT	*hashmake( int num )
{
	struct HASHROOT	*dummy;

	if( ( dummy = (struct HASHROOT *)calloc( 1, sizeof( struct HASHROOT ) ) ) == NULL )
		hash_err( "No memory for hashmake\n" );
	if( ( dummy->hashptr = (struct HASH **)calloc( num, sizeof( struct HASH ) ) ) == NULL )
		hash_err( "No memory for hashmake\n" );
	dummy->max = num;
	dummy->count = 0;
	return dummy;
}

int	hashchck( char *hash, struct HASHROOT *hash_root )
{
	register unsigned int	sum = 0;

	while( *hash != '\0' )
		sum += 2 * sum + *hash++;
	return sum % hash_root->max;
}

struct HASH	*hashadd( struct HASHROOT *hash_root, char *hash, int size )
{
	struct HASH	*hashptr, **dummy;

	if( ( hashptr = (struct HASH *)calloc( 1, sizeof( struct HASH ) ) ) == NULL )
		hash_err( "No memory for hashadd\n" );
	if( ( hashptr->define = (char *)calloc( 1, size ) ) == NULL )
		hash_err( "No memory for hashadd\n" );
	strncpy( hashptr->hash, hash, SYMBOL_LEN );
	hashptr->lambda = 0;                          /* wgg not a lambda variable to start */
	dummy = &hash_root->hashptr[hashchck( hash, hash_root )];
	hashptr->prev = *dummy;
	*dummy = hashptr;
	++hash_root->count;
	return hashptr;
}

struct HASH	*hashfind( struct HASHROOT *hash_root, char *hash )
{
	struct HASH	*hashptr;

	hashptr = hash_root->hashptr[hashchck( hash, hash_root )];
	while( hashptr != NULL && strncmp( hash, hashptr->hash, SYMBOL_LEN ) )
		hashptr = hashptr->prev;
	return hashptr;
}

/*
hashdel(struct HASHROOT *hash_root, char *hash)




*/
