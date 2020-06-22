#ifndef _WRITE_H_
#define _WRITE_H_

union {
	char		buffer[256];
	struct	SATURN	saturn;
} headerout;

/* Function prototypes */

extern	void
	error( char *, ... ),
	DEBUG_PRN( char *, ... );

extern	FILE	*openfile( char *, char *, char *, char *, char ** );

extern	void	set_8( unsigned char *, unsigned char );
extern	void	set_16( unsigned char *, unsigned int );
extern	void	set_32( unsigned char *, unsigned long );
extern	int	get_16( unsigned char * );
extern	long	get_32( unsigned char * );
extern	unsigned	int	get_u16( unsigned char * );
extern	unsigned	long	get_u32( unsigned char * );

extern	void	write_code( long, long );

extern	void	do_all_symbols( void (*)( ) );

void	write_header( void ); 					/* 0002 */
union SFC	*next_object( void ); 			/* 02CC */
void
	flush_objects( void ), 						/* 0334 */
	write_one_symbol( struct symbol * ),	/* 0386 */
	write_symbols( void ), 						/* 0562 */
	write_objfile( void ); 						/* 058A */

/* s_8A17 */

#endif
