
#ifdef	HAVE_CONFIG_H
#   include "config.h" 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef	HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "makerom.h"

#ifdef dll
#include "dllfile.h"
#endif

FILE	*infp, *outfp;
char
*in_name, *out_name = NULL,
  *progname,
  hex[] = "0123456789ABCDEF0123456789abcdef";
int	name_len = 0;

/* New name */
char
*config		=	NULL,
  *message	=	NULL,
  *hash		=	NULL,
  *hashf	=	NULL,
  *link		=	NULL,
  *linkf	=	NULL,
  *romname	=	NULL,
  *finish	=	NULL,
  *romhead	=	NULL,
  *romid	=	NULL;
int
finished	=	0,
  readcode	=	0,
  hashob	=	0,
  plugin	=	0,
  absolu	=	FALSE,  
  compile_ok	=	FALSE,
  original	=	FALSE;

struct {
  char	*name;
  void	(*function)(char *line, char *cmd1, char *cmd2, char *line_ori );
} parse_fncs[] = {
  {	"CONFIGURE",	parse_configure },
  {	"ROMPHEAD",		parse_head      },
  {	"ROMPART",		parse_head      },
  {	"MESSAGES",		parse_mesgs     },
  {	"NAME",			parse_name      },
  {	"HASH",			parse_hash		},
  {	"TABLE",		parse_table     },
  {	"RELOCATE",		parse_rel       },
  {	"SCAN",			parse_rel       },
  {	"FINISH",		parse_finish    },
  {	"END",			parse_end       },
  {	NULL,			null(void(*)(char *, char *, char *, char *)) }
};

struct exts_def {
  int				len, ofs, num, flag, tname, dup, reference, pos;
  char			*name1, *name2;
  struct exts_def	*sort;
}	ext_table[4096], *ext_sort = NULL;

int
current_num	=	0,
  index_tab	=	0,
  nb_items	=	0;

int	hashes[16];

extern	int		optind;
extern	char	*optarg, *version;

void  makeromusage( int flag )
{
  fprintf( stderr, "makerom %s\n", version );
  if( flag ) {
    fprintf( stderr, "Usage: %s [options] ... file.mn [file.m]\n", progname );
    fprintf( stderr, "options:\n" );
    fprintf( stderr, "-a\t\tabsolute user library (use ROMID)\n" );
    fprintf( stderr, "-c\t\tassemble generated files\n" );
    fprintf( stderr, "-s <loader file>\t\tAct like original Makerom in perl.(Will automatically compile the library)\n");
    fprintf( stderr, "-v\t\tprint version\n" );
    exit( 1 );
  } else
    exit( 0 );
}

int	chtoi( char ch )
{
  if( isxdigit( ch ) )
    return ( ( ch > '9' ) ? ( toupper( ch ) - '7' ) : ( ch - '0' ) );
  else
    return -1;
}

int	myatoi( char **ch )
{
  register int i = 0;
  
  while( isxdigit( **ch ) ) {
    i *= 16;
    i += ( ( **ch > '9' ) ? ( toupper( *(*ch)++ ) - '7') : ( *(*ch)++ - '0' ) );
  }
  return i;
}

char	*mystrcpy( char *str )
{
  char	*tmp;
  
  tmp = (char *)calloc( strlen( str ) + 1, sizeof( char ) );
  CHECK( tmp );
  return ( strcpy( tmp, str ) );
}

int	mystrlen( char *tmp )
{
  char ch, *cp = tmp;
  int len = 0;
  
  while( ( ch = *cp++ ) != '\0' ) {
    if( ch == '\\' ) {
      if( strspn( cp, hex ) != 1 )
	++cp;
      ++cp;
    }
    ++len;
  }
  return len;
}

#ifdef	macintosh

int	strcasecmp( char *s1, char *s2 )
{
  register int	i;
  char	*t1, *t2, *tp;
  
  t1 = (char *)malloc( strlen( s1 ) + 1 );
  CHECK( t1 );
  t2 = (char *)malloc( strlen( s2 ) + 1 );
  CHECK( t2 );
  tp = t1;
  while( *s1 )
    *tp++ = toupper( *s1++ );
  tp = t2;
  while( *s2 )
    *tp++ = toupper( *s2++ );
  i = strcmp( t1, t2 );
  free( t1 );
  free( t2 );
  return i;
}

#endif

int	mystrcmp( char *str1, char *str2 )
{
  /* stricmp == strcasecmp */
#if	defined(__MSDOS__)
  if( !stricmp( str1, str2 ) )
#else
    if( !strcasecmp( str1, str2 ) )
#endif
      return TRUE;
  if( toupper( *str1++ ) != toupper( *str2++ ) ||
      toupper( *str1++ ) != toupper( *str2++ ) )
    return FALSE;
  while( *str2 )
    if( toupper( *str1++ ) != toupper( *str2++ ) )
      return FALSE;
  return TRUE;
}

/* Compare two strings that may contain special characters */
int	mystrcmplen( char *str1, char *str2 )
{
  int char1, char2;
  
  while (*str1 && *str2) {
    if (*str1 == '\\') {
      char1 = chtoi(*(str1+1)) * 16 + chtoi(*(str1+2));
      str1+=3;
    }
    else {
      char1 = *str1;
      str1++;
    }
    if (*str2 == '\\') {
      char2 = chtoi(*(str2+1)) * 16 + chtoi(*(str2+2));
      str2+=3;
    }
    else {
      char2 = *str2;
      str2++;
    }
    if (char1 != char2)
      return (char1 - char2);
  }
  return 0;
}

char	*remove_ext( char *filename)
{
  char *buf, *ch;
  
  buf = (char *)calloc(strlen(filename) + 1, sizeof( char) );
  CHECK(buf);
  strcpy(buf, filename);
  ch = strrchr(buf, '.');
  if( ch )
    *ch = '\0';
  return(buf);
}

char	*conc_ext( char *filename, char *ext, int replace )
{
  char *buf, *ch;
  
  buf = (char *)calloc( strlen( filename ) + strlen( ext ) + 1, sizeof( char) );
  CHECK( buf );
  strcpy( buf, filename );
  ch = strrchr( buf, '.' );
  if( ch )
#if	defined(__MSDOS__)
    if( replace || stricmp( ch + 1, ext ) )
#else
      if( replace || strcmp( ch + 1, ext ) )
#endif
	*ch = '\0';
  strcat( buf, "." );
  strcat( buf, ext );
#if	defined(__MSDOS__)
  ch = buf;
  while( *ch )
    *ch++ = (char)toupper( *ch );
#endif
  return( buf );
}

void romprint(char *arg,int flag)
{
	if (!flag) {
	   if (strpbrk(arg," \t") == NULL)
	      fprintf(outfp,"REL %s.o\n",arg);
	   else
	      fprintf(outfp,"REL \"%s.o\"\n",arg);
	}
	else {
	   if (strpbrk(arg," \t") == NULL)
	      fprintf(outfp,"REL %s\n",arg);
	   else
	      fprintf(outfp,"REL \"%s\"\n",arg);
	}
}

char	*get_token( char **src, char *token_sep )
     /*
      * Just a little more convenient than strtok()
      * This function returns a pointer to the first token
      * in src, and makes src point to the "new string".
      */
{
  char	*tok;
  char	*tmp;
  
  tok = *src;
  if( !( src && *src && **src ) )
    return NULL;
  
  if (**src == '"') { /* argument starts with ", assume long name */
     tok = ++(*src);
     
     if ((tmp = strchr(*src,'"')) != (char *) NULL) { /* there's an ending " */
	*tmp = '\0';
	/* Remove trailing spaces */
	*src = tmp + 1;
     }
     else {
        /* No ending ", capture until end of line */
        *src = *src + strlen(*src);
        tmp = *src;
     }
	/* all remaining ending separator will be removed */
     while(strchr( token_sep, *(--tmp) ) )
	*tmp = '\0';
     return tok;
  }
	/* Make *src point after token */
  *src = strpbrk( *src, token_sep );
  if( *src ) {
     **src = '\0';
     ++(*src);
     while( **src && strchr( token_sep, **src ) )
       ++(*src);
  } else
    *src = "";
  return tok;
}

void	out_nibasc( FILE *fp, char *name )
{
  char	ch, *cp = name;
  int	value, done = 0;

  while( ( ch = *cp++ ) != '\0' ) {
    if( ch != '\\' ) {
      if( done++ == 0 )
	fprintf( fp, "\tNIBASC\t\\" );
      fprintf( fp, "%c", ch );
      done = done % 20;       /* max 20 characters in one line definition */
      if( done == 0 )
	fprintf( fp, "\\\n" );
    } else {
      if( done != 0 )
	fprintf( fp, "\\\n" );
      done = 0;
      if( strspn( cp, hex ) == 1 )
	value = ( strchr( hex, *cp++ ) - hex ) & 0x0F;
      else {
	value = 16 * ( ( strchr( hex, *cp++ ) - hex ) & 0x0F);
	value += ( strchr( hex, *cp++ ) - hex ) & 0x0F;
      }
      fprintf( fp, "\tCON(2)\t#%X\n", value );
    }
  }
  if( done != 0 )
    fprintf( fp, "\\\n" );
}

void	parse_mesgs( char *line , char *cmd1, char *cmd2, char *line_ori )
{
  if( message ) {
    fprintf( outfp, "** ERROR! DUPLICATE MESSAGE: %s\n", line );
    fprintf(stderr, "** Extra MESSAGE command\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  message = cmd1;
  fprintf( outfp, "**MESSAGES %s\n", message );
}

void	parse_name( char *line, char *cmd1, char *cmd2, char *line_ori )
{
  if( romname ) {
    fprintf( outfp, "** ERROR! DUPLICATE NAME: %s\n", line );
    fprintf(stderr, "** Extra NAME command\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  romname = line_ori;
  fprintf( outfp, "**NAME %s\n", romname );
}

void	parse_configure( char *line, char *cmd1, char *cmd2, char *line_ori )
{
  if( config ) {
    fprintf( outfp, "** ERROR! DUPLICATE CONFIGURE: %s\n", line );
    fprintf(stderr, "** Extra CONFIG command\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  config = cmd1;
  fprintf( outfp, "**CONFIGURE %s\n", config );
}


void	parse_rel( char *line, char *cmd1, char *cmd2 , char *line_ori )
{
  char buf[50];

  if(finished) {
    fprintf(outfp, "** Code after FINISH\n");
    fprintf(stderr, "** Code after FINISH\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  read_ext(cmd1);	/* Need to read .ext file */
  if (romid && !romhead) {
    if (readcode) {	/* complain if we missed */
      fprintf(outfp,"** Missing ROMPART header\n");
      fprintf(stderr,"** Missing ROMPART header\n");
    }
    else {
      sprintf(buf,"ROMHD%s",romid);
      romhead = mystrcpy(buf);
      romprint(romhead,FALSE);
    }
  }
  readcode = 1;
  if (mystrcmp("RELOCATE",line))
    romprint(cmd1,TRUE);
  else
    fprintf(outfp, "** %s\n",cmd1);
}

void	parse_head( char *line, char *cmd1, char *cmd2, char *line_ori  )
{
  if(romhead || readcode) {
    fprintf(outfp, "** Code before %s command\n",line);
    fprintf(stderr, "** Code before %s command\n",line);
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  romhead = remove_ext(cmd1);
  fprintf( outfp, "**%s %s\n", line, romhead);
  romprint(romhead,FALSE);
}

void	parse_hash( char *line, char *cmd1, char *cmd2, char *line_ori )
{
  if (hashob) {
    fprintf(outfp, "**ERROR! DUPLICATE HASH TABLE: %s\n",line);
    fprintf(stderr, "** Extra HASH command\n");
    return;
  }
  if (hash) {
    fprintf(outfp, "** HASH incompatible with TABLE\n");
    fprintf(stderr, "** HASH incompatible with TABLE\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  hash = cmd1;
  link = cmd2;
  hashob = 1;
}

void	parse_table( char *line, char *cmd1, char *cmd2, char *line_ori )
{
  if(hashob) {
    fprintf(outfp, "** TABLE incompatible with HASH\n");
    fprintf(stderr, "** TABLE incompatible with HASH\n");
    return;
  }
  if (hash) {
    fprintf(outfp, "** Extra TABLE command\n");
    fprintf(stderr, "** Extra TABLE command\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf(outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  hash = remove_ext(cmd1);
  if (finished) {
    fprintf(outfp, "** Code after FINISH\n");
    fprintf(stderr, "** Code after FINISH\n");
    return;
  }
  fprintf( outfp, "**TABLE %s\n", cmd1);
  if (cmd2 != NULL) {	/* If hash and link, include only link */
    link = remove_ext(cmd2);
    romprint(link,FALSE);
  }
  else {
    romprint(hash,FALSE);
  }	
}

void	parse_finish( char *line, char *cmd1, char *cmd2, char *line_ori )
{
  char	buf[50];
  
  if (finished) {
    fprintf(outfp, "** Extra FINISH command\n");
    fprintf(stderr, "** Extra FINISH command\n");
    return;
  }
  if (cmd1 == NULL) {
    fprintf( outfp, "** ERROR! %s expects an argument\n",line);
    fprintf(stderr, "** ERROR! %s expects an argument\n",line);
    return;
  }
  finish = remove_ext(cmd1);
  finished = 1;
  fprintf(outfp, "**FINISH %s\n", cmd1);
  if (romid && !hash) {
    sprintf(buf,"TABLE%s",romid);
    hash = mystrcpy(buf);
    romprint(hash,FALSE);
  }
  romprint(finish,FALSE);
}


void	parse_end( char *line, char *cmd1, char *cmd2, char *line_ori )
{
  char	buf[50];
  
  if ( romid && !hash) {
    sprintf(buf,"TABLE%s",romid);
    hash = mystrcpy(buf);
    romprint(hash,FALSE);
  }
  if ( romid && !finish) {
    sprintf(buf,"SYSEND%s",romid);
    finish = mystrcpy(buf);
    romprint(finish,FALSE);
  }
  fprintf( outfp, "END\n" );
}

void	parse_line( char *line )
{
  char *command, *line2, *line3, *cmd1, *cmd2;
  int i;
  
  if( *line == '*' )
    return;
  SKIPSPC( line );

  command = mystrcpy( line );
  command = strtok(command," ,\t\n");

  line2 = mystrcpy(line);
  get_token( &line2, " ,\t\n" );

  line3 = mystrcpy(line2);
  line3 = strtok(line3,"\n");

  cmd1 = get_token( &line2, " ,\t\n");
  cmd2 = get_token (&line2, " ,\t\n");

  if( command == NULL )
    return;

  for( i = 0; parse_fncs[i].name; i++ )
    if( mystrcmp( parse_fncs[i].name, command ) ) {
      parse_fncs[i].function(command , cmd1, cmd2, line3);
      return;
    }
  /* The command is not handled by makerom */
  fprintf(outfp,"%s\n",line);	
}

void	parse_file( void )
{
  char	buf[MAXBUFSIZE], *line;
  
  while( ( ( line = fgets( buf, MAXBUFSIZE, infp ) ) != NULL ))
    parse_line( line );
}


void    make_hash(FILE *fp)
{
  struct exts_def *dummy;
  int	i, ofs, pos;
  
  fprintf( fp, "\tCON(5)\t=DOHSTR\n" );
  fprintf( fp, "\tREL(5)\tTaBlEnD\n" );
  for( i = 0; i < 16; i++ )
    if( hashes[i] )
      fprintf( fp, "\tREL(5)\toBleN%x\n", i + 1 );
    else
      fprintf( fp, "\tCON(5)\t0\n" );
  fprintf( fp, "\tREL(5)\tnaMEend\n" );
  dummy = ext_sort;
  for( i = 0; i < 16; i++ )
    if( hashes[i] ) {
      fprintf( fp, "oBleN%x\n", i + 1 );
      ofs = 0;
      while( dummy && (dummy->len == ( i + 1 )) ) {
	if (!dummy->dup) {
	  fprintf( fp, "\tCON(2)\t#%X\n", i + 1 );
	  out_nibasc( fp, dummy->name2 );
	  fprintf( fp, "\tCON(3)\t%d\n", dummy->num );
	  dummy->ofs = ofs;
	  ofs += ( 5 + 2 * ( i + 1 ) );
	}
	dummy = dummy->sort;
      }
    }
  fprintf( fp, "naMEend\n" );

  for (pos = 0; pos < nb_items ; pos++) {
    if (!ext_table[pos].tname) {
      if (ext_table[pos].len) {
	fprintf( fp, "=~%s\tEQU\t#%s+4096*%d\n",
		 ext_table[pos].name1, romid, ext_table[pos].num );
	if(ext_table[pos].len != -999) {
	  if (!ext_table[pos].dup)
	    fprintf( fp, "\tCON(5)\t(*)-(oBleN%x)-%d\n",
		     ext_table[pos].len, ext_table[pos].ofs );
	  else
	    fprintf( fp, "\tCON(5)\t(*)-(oBleN%x)-%d\n",
		     ext_table[ext_table[pos].reference].len, ext_table[ext_table[pos].reference].ofs );
	}
	else {
	  fprintf( fp, "\tCON(5)\t0\n");
	}
      }
    }
  }

  for (pos = 0; pos < nb_items ; pos++) {
    if (!ext_table[pos].tname) {
      if (!ext_table[pos].len)
	fprintf( fp, "=~%s\tEQU\t#%s+4096*%d\n",
		 ext_table[pos].name1, romid, ext_table[pos].num );
    }
  }
  
  fprintf( fp, "TaBlEnD\n" );
}

void	make_link(FILE *fp)
{
  int		pos;
  
  fprintf( fp, "\tCON(5)\t=DOHSTR\n" );
  fprintf( fp, "\tREL(5)\tenDLink\n" );
  
  for (pos = 0; pos < nb_items ; pos++) {
    if (!ext_table[pos].tname && ext_table[pos].len) {
      if (linkf)
	fprintf( fp, "=~%s\tEQU\t#%s+4096*%d\n", ext_table[pos].name1, romid, ext_table[pos].num );
      fprintf( fp, "\tREL(5)\t=%s\n", ext_table[pos].name1);
    }
  }
  for (pos = 0; pos < nb_items ; pos++) {
    if (!ext_table[pos].tname && !ext_table[pos].len) {
      if (linkf)
	fprintf( fp, "=~%s\tEQU\t#%s+4096*%d\n", ext_table[pos].name1, romid, ext_table[pos].num );
      fprintf( fp, "\tREL(5)\t=%s\n", ext_table[pos].name1);
    }
  }

  fprintf( fp, "enDLink\n" );
}

void    parse_external(char *line)
{
	register int    i = 0, test = TRUE, tname = FALSE;
	char    *ch1, *ch2, *tmp;
	struct exts_def *current = NULL, *previous;
	int	pos = 0;
	
	tmp = mystrcpy(line);
	ch2 = strpbrk(tmp, " \n");
	if(*ch2++ == '\n') 
	{
		free(tmp);
		return;
	}
	ch1 = strtok(tmp, " \t\n");
	KILLNEWLINE(ch2)
		
	/* if the size of a label is more than 16 characters, then it uses the 
	* 16th entry in the hash table
	*/
    
	if( (*ch1 == 'H') || (*ch1 == 'h') ) 
	{
		i = - 999;
		ch1++;
	}
	else
		i = chtoi(*ch1++);
	
	if(i > 0 && i < 17)
		hashes[i - 1] = TRUE;
	else if( (i != 0) && (i != -999) ) {
		KILLNEWLINE(line)
			printf("Invalid External entry: %s\n", line);
		return;
	}
	
	if ( (i!=-999) && (i!=0) && (mystrlen(ch2) != i) )
		printf("** WARNING **\tEntry %s is possibly corrupted\n",ch1);
	
	
	/* Search if this entry (label) already exist (for tname) */
	pos = 0;
	while ((pos < nb_items) && !tname) 
	{
		if (!strcmp(ext_table[pos].name1,ch1))
			tname = TRUE;		
		else
			pos++;
	}
	ext_table[nb_items].tname = tname;
	ext_table[nb_items].len = i;
	ext_table[nb_items].ofs = 0;
	ext_table[nb_items].name1 = mystrcpy(ch1);
	ext_table[nb_items].sort = NULL;
	if (i != -999 )
		ext_table[nb_items].name2 = mystrcpy(ch2);
	else
		ext_table[nb_items].name2 = NULL;
	if (tname)
		ext_table[nb_items].num = ext_table[pos].num;
	else
		ext_table[nb_items].num = current_num;
	ext_table[nb_items].pos = nb_items;
	ext_table[nb_items].dup = FALSE;
	free(tmp);
	
	if (!tname)
		current_num++;
	nb_items++; /* One more item in the list */
	
	if ((i == 0) || (i == -999))
		return;
	
	/* We will add the new entry and sort the list (JYA) */
	/* Algorithm: ext_sort is a sorted list, we parse it until we find the right location for the new entry
	*/
	
	if (ext_sort != NULL) {
		current = ext_sort;
		/* current = current entry ; previous = entry before current */
		while ( ( (test = (ext_table[nb_items-1].len < current->len) ?
			-1 : (ext_table[nb_items-1].len > current->len) ?
			1 : mystrcmplen(ext_table[nb_items-1].name2, current->name2) )
			> 0) && (current->sort != NULL) ) {
			previous = current;
			current = current->sort;
		}
		if (test == 0) {/* If the name already exists in the list, put it away */
			ext_table[nb_items-1].dup = TRUE;
			ext_table[nb_items-1].reference = current->pos;
			return;
		}
		/* current is a pointer to the position we have to insert the new entry */
		if ( (current == ext_sort) && (test <= 0)) {
			/* We insert the new entry at the top of the list */
			ext_sort = &ext_table[nb_items-1];
			ext_sort->sort = current;
		}
		else
			if (current->sort || test < 0) {
				/* We insert the new entry somewhere in the list */
				previous->sort = &ext_table[nb_items-1];
				ext_table[nb_items-1].sort = current;
			}
			/* We insert the new entry at the bottom of the list */
			else
				current->sort = &ext_table[nb_items-1];
	}
	else
		ext_sort = &ext_table[nb_items-1];
}

void read_ext(char *file_name)
{
	char *tmp_name, buf[MAXBUFSIZE], buf2[50];
	FILE *tmpfp;
	int		i;
	
	tmp_name = conc_ext(file_name, "ext", TRUE );
	if((tmpfp = fopen(tmp_name, "r")) != NULL) {
		while((fgets(buf, MAXBUFSIZE, tmpfp)) != NULL)
			switch(buf[0])
			{
				case 'Y':
				case 'y':
				case 'X':
				case 'x':
					if (plugin && (plugin != buf[0])) fprintf(stderr,"*** Warning: ROMID/xROMID combo illegal\n");
					if (!plugin) plugin = toupper(buf[0]);
					strncpy(buf2, buf + 1, 3);
					buf2[3] = '\0';
					i = 3;
					while ( ( (buf2[i] == '\n') || (buf2[i] == '\0') ) && (i >= 0))
						buf2[i--] = '\0';
					if (!romid) { romid = mystrcpy(buf2); }
					else {
						if (strcmp(buf2,romid)) {
							fprintf(outfp, "** Wrong ROMID in %s: %s (should be %s)\n",tmp_name,buf2,romid);
							fprintf(stderr, "** Wrong ROMID in %s: %s (should be %s)\n",tmp_name,buf2,romid);
						}
					}
					break;
				default :
					parse_external(buf);
					break;
			}
			fclose(tmpfp);
			/* Kludge!!! stupid fopen() on some stations garble input string
			fprintf(outfp, "** Parsing %s\n", tmp_name); *current* */
			fprintf(outfp, "** Parsing %s\n", conc_ext(file_name, "ext", TRUE ) );
		} else {
			fprintf(outfp, "** Could not open %s\n", tmp_name);
	}
}

void    make_files(void)
{
  char	buf[MAXNAMELEN];
  FILE	*fp;

  if (romid) 
  { /* no romid means just do load */
    if (!hashob) 
    {
      hashf = hash;
      if (strcmp(hash,"-")) 
      { /* need to build tables */
	sprintf(buf,"%s.a",hash);
	if ( (fp = fopen(buf,"w")) == NULL) 
	{
	  perror(buf);
	  exit(-1);
	}
	fprintf(fp,"* ROMPART %s hashtable\n",romid);
	sprintf(buf,"HasH%s",romid);
	hash = mystrcpy(buf);
	if (!link)
	  fprintf(fp,"=%s\n",hash);
	else
	  fprintf(fp,"=c%s\n",hash);	/* We will assume they are covered if separate */
	make_hash(fp);
      }
      if (link) 
      {
	if (strcmp(hashf,"-")) 
	{
	  fprintf(fp,"\tEND\n");
	  fclose(fp);
	}
	linkf = link;
	sprintf(buf,"%s.a",link);
	if ((fp = fopen(buf,"w")) == NULL) 
	{
	  perror(buf);
	  exit(-1);
	}
	fprintf(fp,"* ROMPART %s link\n\n",romid);
	if (strcmp(hashf,"-")) 
	{
	  fprintf(fp,"=%s\n\tCON(5)\t=DOACPTR\n",hash);
	  fprintf(fp,"\tCON(5)\t=c%s\n\n",hash);
	  fprintf(fp,"\tCON(5)\t0\n");
	}
      }
      sprintf(buf,"LinK%s",romid);
      link = mystrcpy(buf);
      fprintf(fp,"=%s\n",link);
      make_link(fp);
      fprintf(fp,"\tEND\n");
      fclose(fp);
    }
    if (finish) 
    {
      sprintf(buf,"%s.a",finish);
      if ((fp = fopen(buf,"w")) == NULL) 
      {
	perror(buf);
	exit(-1);
      }
      fprintf(fp,"* ROMPART %s end\n\n",romid);
      if ( (plugin == 'Y') || absolu )
	fprintf(fp,"\tCON(4)\t0\n");
      fprintf(fp,"=SYSEND%s\n",romid);
      fprintf(fp,"\tEND\n");
      fclose(fp);
    }
    if (romhead) 
    {
      sprintf(buf,"%s.a",romhead);
      if ((fp = fopen(buf,"w")) == NULL) 
      {
	perror(buf);
	exit(-1);
      }
      fprintf(fp,"* ROMPART %s header\n\n",romid);
      fprintf(fp,"=ROMID%s\tEQU\t#%s\n",romid,romid);
      if ( (plugin == 'Y') || absolu )
	fprintf(fp,"\tCON(5)\t=DOLIB\n=LIB%s\n",romid);
      fprintf(fp,"\tREL(5)\t=SYSEND%s\n",romid);
      if (romname) 
      {
	name_len = mystrlen(romname);
	fprintf(fp, "\tCON(2)\t%d\n", name_len);
	out_nibasc(fp, romname);
	fprintf(fp, "\tCON(2)\t%d\n", name_len);
      }
      else 
      {
	fprintf(fp, "\tCON(2)\t0\t\tNo Name\n");
      }
      fprintf(fp, "=SYS%s\tCON(3)\t#%s\n", romid, romid);
      fprintf(fp, "\tREL(5)\t=HasH%s\n", romid);
      if(message)
	fprintf(fp, "\tREL(5)\t=%s\n", message);
      else
	fprintf(fp, "\tCON(5)\t0\t\tNo Messages\n");
      if (link)
	fprintf(fp,"\tREL(5)\t=%s\n",link);
      else
	fprintf(fp, "\tCON(5)\t0\t\tNo Link\n");
      if (config)
	fprintf(fp, "\tREL(5)\t=%s\n", config);
      else
	fprintf(fp, "\tCON(5)\t0\t\tNo Config\n");
      fprintf(fp, "\tEND\n");
      fclose(fp);
    }
  }
}

void    sort_xnames( void )
{
  int     xnames = 0, xindex, nullindex, pos, pos2;
  
  for (pos = 0; pos < nb_items ; pos++)
    if( !ext_table[pos].tname && ext_table[pos].len )
      xnames++;
  
  /* Now keep a running index of what the next xNAME index should be
   * and what the next nullname should be and fix the indexes.
   */
  xindex = 0;
  nullindex = xnames;
  for (pos = 0; pos < nb_items ; pos++)
    if( !ext_table[pos].tname ) 
	{
      if( ext_table[pos].len != 0 )
		ext_table[pos].num = xindex++;
      else
		ext_table[pos].num = nullindex++;
    }

  // ensure that all aliases have the same number than the original
  for (pos = 0; pos < nb_items ; pos++)
    if( ext_table[pos].tname )
	{
	  for (pos2 = 0; pos2 < nb_items ; pos2++)
		if( (!ext_table[pos2].tname) && (0==strcmp(ext_table[pos].name1,ext_table[pos2].name1)) )
			ext_table[pos].num = ext_table[pos2].num;
	}
}

void makeromget_options( int argc, char **argv )
{
  char	i;
  
  out_name = NULL;
  while( ( i = getopt( argc, argv, "acvs:" ) ) != (char)( -1 ) )
    switch( i ) {
    case 'a':
      absolu = TRUE;
      break;
            
    case 'c':
      compile_ok = TRUE;
      break;
    case 's':
      out_name = optarg;
      original = TRUE;
      compile_ok = TRUE;
      break;
    case 'v':
      makeromusage( FALSE );
      break;
    default:
      makeromusage( TRUE );
      break;
    }
}

#ifdef dll
int	makerommain( int argc, char **argv )
#else
int	main( int argc, char **argv )
#endif
{
  int		i;
  char	*std_sign = "-";
  char	tmp[255];
  
  progname = *argv;
  makeromget_options( argc, argv );
  
  if( ( argc - optind ) > 3 )
    makeromusage( TRUE );
  switch( argc - optind + 1 ) {
  case 3:
    if( !strcmp( argv[optind + 1], std_sign ) )
      out_name = std_sign;
    else
      out_name = argv[optind + 1];
  case 2:
    if( !strcmp( argv[optind], std_sign ) )
      in_name = std_sign;
    else
      in_name = argv[optind];
    if( out_name == NULL)
      out_name = conc_ext( argv[optind], "m", TRUE );
    break;
    /*		case 1:
		out_name = in_name = std_sign;
		break;
    */
  default:
    makeromusage( TRUE );
    break;
  }
  
  if( strcmp( in_name, std_sign ) )
    if( ( infp = fopen( in_name, "r" ) ) == NULL ) {
      perror( in_name );
      exit( 1 );
    }
  if( strcmp( out_name, std_sign ) )
    if( ( outfp = fopen( out_name, "w" ) ) == NULL ) {
      perror( out_name );
      exit( 1 );
    }
  for( i = 0; i < 16; i++ )
    hashes[i] = FALSE;
  
  /* here goes parsing of inname file */
  parse_file();
  /* sorting xNAMEs */
  sort_xnames();
  /* now we make tables and loader file */
  make_files();
  
  fclose( infp );
  fclose( outfp );
  
  if (compile_ok == TRUE) {
    sprintf(tmp,"sasm -E -a %s.l -o %s.o %s.a",romhead,romhead,romhead);
    if (system(tmp) != 0) {
      fprintf(stderr, "\nCouldn't assemble %s.a\n",romhead);
    }
    if (finish) {
      sprintf(tmp,"sasm -E -a %s.l -o %s.o %s.a",finish, finish, finish);
      if (system(tmp) != 0) {
	fprintf(stderr, "\nCouldn't assemble %s.a\n",finish);
      }
    }
    if (hashf && strcmp(hashf,"-")) {
      sprintf(tmp,"sasm -E -a %s.l -o %s.o %s.a",hashf, hashf, hashf);
      if (system(tmp) != 0) {
	fprintf(stderr, "\nCouldn't assemble %s.a\n",hashf);
      }
    }
    if (linkf) {
      sprintf(tmp,"sasm -E -a %s.l -o %s.o %s.a",linkf, linkf, linkf);
      if (system(tmp) != 0) {
	fprintf(stderr, "\nCouldn't assemble %s.a\n",linkf);
      }
    }
  }
  return 0;
}
