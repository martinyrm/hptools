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
#include <ctype.h>

#include "hash.h"
#include "rplcomp.h"
#include "defer.h"
#include "masd.h"
#include "envfile.h"
#include "getopt.h"

#ifdef dll
#include "dllfile.h"

/* #define exit myexit		     wgg - I commented this out to allow for the exit in dllfile.h
void	*exit(int errorcode)
{
  if (exiterr)
    exit(errorcode);
  else
    exit(0);
} */

#endif

unsigned short	ctrl_stack[MAXDEPTH];
unsigned int	ctrl_lines[MAXDEPTH];
int	fieldnumber, err_count, c_stack_level, label_cnt;
int
debug	=	FALSE,
  force_comment	=	FALSE,
  commented	=	FALSE,
  in_next_file	=	FALSE,
  romid	=	FALSE,
  exiterr =	TRUE;

FILE	*outfp, *extfp , *dbgfile;
char	*in_name, *out_name = NULL, *ext_name = NULL, *dbgname = NULL;
char	*progname;
char	*fields[MAXTOKEN], token_buf[MAXBUFSIZE], line_buf[MAXBUFSIZE];
char	str_buf[MAXBUFSIZE], name_buf[MAXNAMESIZE], *romid_num;
unsigned	char	romid_flag;
struct	HASHROOT	*hash_root;
struct	ARRY	*arryp;
struct	FILES	infiles[MAXINCLUDE], *infp;

extern	int	optind;
extern	char	*optarg, *version;

struct {
  char	names[6];
  char	values[6];
} chr_aliases = {	"ntr\"\\", "\n\t\r\"\\"	};

struct {
  char	hex[33];
  char	dec[11];
} str_table = {	"0123456789ABCDEF0123456789abcdef",	"0123456789"	};

struct {
  char	*token;
  int	flag;
  void	(*function)(char *);
} parse_ftab1[] = {
  {	";",	TRUE,	dosemi	},
  {	"(;)",	TRUE,	dosemi	},
  {	"[;]",	TRUE,	dosemi	},
  {	"THEN",	TRUE,	dothen	},
  {	"(THEN)",	TRUE,	dothen	},
  {	"[THEN]",	TRUE,	dothen	},
  {	"CODE",	TRUE,	docode	},
  {	"CODEM",	TRUE,	docodem	},
  {	"TITLE",	FALSE,	dotitle	},
  {	"xTITLE",	FALSE,	dotitle	},
  {	"STITLE",	FALSE,	dotitle	},
  {	"EJECT",	FALSE,	dotitle	},
  {	"ASSEMBLE",	FALSE,	doassemble	},
  {	"ASSEMBLEM",    FALSE, doassemblem },
  {	"SETFLAG",  FALSE,	dotitle	},
  {	"CLRFLAG",  FALSE,	dotitle	},
  {	NULL,	FALSE,	null(void(*)(char *))	}
};

struct {
  char	*token;
  int	flag;
  void	(*function)(void);
} parse_ftab2[] = {
  {	"ENDCODE",	TRUE,	doendcode	},
  {	"TAG",	TRUE,	dotag	},
  {	"CHR",	TRUE,	dochr	},
  {	"%",	TRUE,	doreal,	},
  {	"%%",	TRUE,	doereal	},
  {	"C%",	TRUE,	docmp	},
  {	"C%%",	TRUE,	doecmp	},
  {	"RPL",	FALSE,	dorpl	},
  {	"(",	FALSE,	doparen	},
  {	"EXTERNAL",	FALSE,	doext	},
  {	"FEXTERNAL",	FALSE,	dofext	},
  {	"LOCAL",	FALSE,	dolocal	},
  {	"DEFINE",	FALSE,	dodefine	},
  {	"INCLUDE",	FALSE,	doinclude	}, 
  {	"{{",		TRUE,	dobind	},
  {	"}}",		TRUE,	doendbind	},
  {	"INCLOB",	FALSE,	doinclob },
  {	NULL,	FALSE,	null(void(*)(void))	}
};

struct {
  char	*token;
  int	flag;
  void	(*function)(int);
  int	parameter;
} parse_ftab3[] = {
  {	"ROMID",	FALSE,	doromid,	0	},
  {	"xROMID",	FALSE,	doromid,	1	},
  {	"LOCALNAME",	FALSE,	donameless,	FALSE	},
  {	"LOCALLABEL",	FALSE,	donameless,	FALSE	},
  {	"NAMELESS",	FALSE,	donameless,	TRUE	},
  {	"LABEL",	FALSE,	donameless,	TRUE	},
  {	"ARRY",	TRUE,	doarry,	0	},
  {	"LNKARRY",	TRUE,	doarry,	1	},
  {	"LNKOTHER",	TRUE,	doarry,	2	},
  {	NULL,	FALSE,	null(void(*)(int)),	0	}
};

struct {
  char	*token;
  int	flag;
  void	(*function)(int, char *);
  int	par1;
  char	*par2;
} parse_ftab4[] = {
  {	"NAME",		TRUE,	doname,	1,	"NAME"},
  {	"NULLNAME",	FALSE,	doname,	0,	"NULLNAME"},
  {	"xNAME",	TRUE,	doname,	2,	"xNAME"	},
  {	"hNAME",	TRUE,	doname,	3,	"hNAME"	},
  {	"sNAME",	TRUE,	doname,	4,	"sNAME"	},
  {	"tNAME",	FALSE,	doname,	5,	"NULLNAME"},
  {	"ALIAS",	FALSE,	doname,	5,	"ALIAS"	},
  {	NULL,	FALSE,	null(void(*)(int, char *)),	0,	NULL	}
};
struct {
  char	*token;
  void	(*function)(int, char *);
  int	par1;
  char	*par2;
} parse_ftab5[] = {
  {	"#IF",		doconditional, 1, "#IF"	},
  {	"#IFNOT",	doconditional, 1, "#IFNOT"},
  {	"#ELSE",	doconditional, 0, "#ELSE"},
  {	"#ENDIF",	doconditional, 0, "#ENDIF"},
  {	NULL,		null(void(*)(int, char *)),	0,	NULL	}
};

struct {
  char	*mnem;
} ctrl_table[14] = {
  {	"::"	},
  {	"BEGIN"	},
  {	"WHILE"	},
  {	"{"	},
  {	"DO"	},
  {	"IF"	},
  {	"ELSE"	},
  {	"TAG"	},
  {	"ARRY"	},
  {	"LNKARRY"	},
  {	"LNKOTHER"	},
  {	"SYMBOL"	},
  {	"UNIT"	},
  {	"FCN"	}
};

struct {
  char  *ntoken;
  char  *getname;
  char  *putname;
  char  *getaddr;
  char  *putaddr;
 } lamtable[22] = {
        { "ONE",        "1GETLAM",  "1PUTLAM", "#613B6", "#615E0"},
        { "TWO",        "2GETLAM",  "2PUTLAM", "#613E7", "#615F0"},
        { "THREE",      "3GETLAM",  "3PUTLAM", "#6140E", "#61600"},
        { "FOUR",       "4GETLAM",  "4PUTLAM", "#61438", "#61615"},
        { "FIVE",       "5GETLAM",  "5PUTLAM", "#6145C", "#61625"},
        { "SIX",        "6GETLAM",  "6PUTLAM", "#6146C", "#61635"},
        { "SEVEN",      "7GETLAM",  "7PUTLAM", "#6147C", "#61645"},
        { "EIGHT",      "8GETLAM",  "8PUTLAM", "#6148C", "#61655"},
        { "NINE",       "9GETLAM",  "9PUTLAM", "#6149C", "#61665"},
        { "TEN",       "10GETLAM", "10PUTLAM", "#614AC", "#61675"},
        { "ELEVEN",    "11GETLAM", "11PUTLAM", "#614BC", "#61685"},
        { "TWELVE",    "12GETLAM", "12PUTLAM", "#614CC", "#61695"},
        { "THIRTEEN",  "13GETLAM", "13PUTLAM", "#614DC", "#616A5"},
        { "FOURTEEN",  "14GETLAM", "14PUTLAM", "#614EC", "#616B5"},
        { "FIFTEEN",   "15GETLAM", "15PUTLAM", "#614FC", "#616C5"},
        { "SIXTEEN",   "16GETLAM", "16PUTLAM", "#6150C", "#616D5"},
        { "SEVENTEEN", "17GETLAM", "17PUTLAM", "#6151C", "#616E5"},
        { "EIGHTEEN",  "18GETLAM", "18PUTLAM", "#6152C", "#616F5"},
        { "NINETEEN",  "19GETLAM", "19PUTLAM", "#6153C", "#61705"},
        { "TWENTY",    "20GETLAM", "20PUTLAM", "#6154C", "#61715"},
        { "TWENTYONE", "21GETLAM", "21PUTLAM", "#6155C", "#61725"},
        { "TWENTYTWO", "22GETLAM", "22PUTLAM", "#6156C", "#61735"}
};


struct {
  char	*mnem;
  int	prev;
  int	next;
  char	*table[5];
} tok_table[15] = {
  { "::",-1,0,{ "DOCOL", NULL, NULL,NULL,NULL}},
  { "BEGIN",-1,1, {"BEGIN",NULL,NULL,NULL,NULL}},
  { "WHILE",1,2, {"WHILE","DOCOL",NULL,NULL,NULL}},
  { "REPEAT",2,-1, {"SEMI","REPEAT",NULL,NULL,NULL}},
  { "UNTIL",1,-1, {"UNTIL",NULL,NULL,NULL,NULL}},
  { "AGAIN",1,-1, {"AGAIN",NULL,NULL,NULL,NULL}},
  { "{",-1,3, {"DOLIST",NULL,NULL,NULL,	NULL}},
  { "}",3,-1,{"SEMI",NULL,NULL,NULL,NULL}},
  { "DO",-1,4,{"DO",NULL,NULL,NULL,NULL	}},
  { "LOOP",4,-1,{"LOOP",NULL,NULL,NULL,	NULL }},
  { "+LOOP",4,-1,{"+LOOP",NULL,NULL,NULL,NULL }},
  { "UNIT",-1,12,{"DOEXT",NULL,NULL,NULL,NULL }},
  { "SYMBOL",-1,11,{"DOSYMB",NULL,NULL,	NULL,NULL}},
  { "IF",-1,5,	{"*** IF ***","'","DOCOL",NULL,NULL}},
  { "ELSE",5,6,	{"SEMI","*** ELSE ***",	"'","DOCOL",NULL}}
};

void  rplcompusage( int flag )
{
  fprintf( stderr, "rplcomp %s\n", version );
  if( flag )
    return;
  fprintf( stderr, "Usage: %s [options] ... [sourcefile [outfile [extfile]]]\n", progname );
  fprintf( stderr, "options:\n" );
  fprintf( stderr, "-C\t\tforce commented mode\n" );
  fprintf( stderr, "-d debugfile\tdebug mode, specified output file\n" );
  fprintf( stderr, "-l\t\tMASD will allow labels and constants > 22 characters\n" );
  fprintf( stderr, "-v\t\tprint version\n" );
  fprintf( stderr, "-X\t\tdoesn't kill process in case of error\n" );
}

char	*mystrcpy(char *str)
{
  char	*tmp;

  tmp = (char *)calloc(strlen(str) + 1, sizeof(char));
  CHECK(tmp);
  return (strcpy(tmp, str));
}

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

char	*conc_ext( char *filename, char *ext )
{
  char *buf, *ch;

  buf = (char *)calloc( strlen( filename ) + strlen( ext ) + 1, sizeof( char ) );
  CHECK( buf );
  strcpy( buf, filename );
  ch = strrchr( buf, '.' );
  if( ch )
#if	defined(__MSDOS__)
    if( stricmp( ch + 1, ext ) )
#else
      if( strcmp( ch + 1, ext ) )
#endif
	*ch = '\0';
  strcat( buf, "." );
  strcat( buf, ext );
#if	defined(__MSDOS__)
  ch = buf;
/*  while( *ch )
    *ch++ = (char)toupper( *ch );        wgg do not mess up the user's lower case in long file names */
#endif
  return( buf );
}

int	check_ctrl_stk( int ofs )
{
  if( c_stack_level == 0 )
    return FALSE;
  if( (int)ctrl_stack[ c_stack_level - 1 ] != ofs )
    return FALSE;
  c_stack_level--;
  return TRUE;
}

void	add_ctrl_stk( int ofs )
{
  if( c_stack_level == MAXDEPTH ) {
    fprintf( stderr, "Fatal error: control stack overflow\n" );
    exit( 1 );
  }
  ctrl_stack[ c_stack_level ] = (unsigned short)ofs;
  ctrl_lines[ c_stack_level++ ] = infp->line;
}

void	err_std( char *err )
{
  fprintf( stderr, err );
  exit( 1 );
}

struct LINE	*make_line(void)
{
  struct LINE	*tempo;

  if((tempo = (struct LINE *)calloc(1, sizeof(struct LINE))) == NULL)
    err_std("Cannot malloc LINE BLK\n");
  return tempo;
}


void	output(char *str)
{
  struct LINE *tempo;

  if(arryp == NULL) {
    fputs(str, outfp);
    return;
  }
  tempo = make_line();
  tempo->line = mystrcpy(str);
  if(arryp->type == NULL) {
    if(arryp->dimens == NULL)
      arryp->dimens = tempo;
    else
      arryp->dim_last->next = tempo;
    arryp->dim_last = tempo;
  } else {
    if(arryp->itm_last->item == NULL)
      arryp->itm_last->item = tempo;
    else
      arryp->itm_last->last->next = tempo;
    arryp->itm_last->last = tempo;
  }
}

void	outputf(char *fmt_str, ...)
{
  char	buffer[MAXBUFSIZE];
  va_list	arg_ptr;

  va_start(arg_ptr, fmt_str);
  vsprintf(buffer, fmt_str, arg_ptr);
  va_end(arg_ptr);
  output(buffer);
}

void	out_ch(char c)
{
  char ch[2];

  if(arryp != NULL) {
    ch[0] = c;
    ch[1] = '\0';
    output(ch);
    return;
  }
  putc(c, outfp);
}

void RPLCompError(int err, char *fmt_str, ...)
{
  char	buffer[MAXBUFSIZE];
  va_list	arg_ptr;
  struct	FILES	*fp;

  va_start(arg_ptr, fmt_str);
  if(err)
    err_count++;
  vsprintf(buffer, fmt_str, arg_ptr);
  va_end(arg_ptr);
  fp = infp;
  while(fp->fp == NULL)
    fp--;
  fprintf(outfp, "*| %s: %d: error: %s\n", fp->name, (infp->fp != NULL) ? fp->line : -(fp->line), buffer);
  fprintf(stderr, "%s: %d: %s\n", fp->name, (infp->fp != NULL) ? fp->line : -(fp->line), buffer);
}

void	stack_err(char *str)
{
  RPLCompError(TRUE, "Unmatched control word: %s\n", str);
}

void	writeline(char *fmt_str, ...)
{
  char	buffer[MAXBUFSIZE];
  va_list	arg_ptr;

  va_start(arg_ptr, fmt_str);
  vsprintf(buffer, fmt_str, arg_ptr);
  va_end(arg_ptr);
  output(buffer);
}

void	end_file(void)
{
  if(infp == infiles) {
    while(c_stack_level-- != 0)
      RPLCompError(TRUE, "Unmatched control word in line #%d: %s", ctrl_lines[c_stack_level], ctrl_table[ctrl_stack[c_stack_level]].mnem);
    if(!romid && ext_name != NULL)
      remove(ext_name);
    if(err_count == 0)
      exit(0);
    if(err_count == 1)
      fputs("1 error in RPL source\n", stderr);
    else
      fprintf(stderr, "%d errors in RPL source\n", err_count);
    exit(1);
  }
  if(infp->fp != NULL) {
    fclose(infp->fp);
    infp->fp = NULL;
  }
  FREE(infp->name);
  infp--;
  in_next_file = TRUE;
  commented = force_comment;
  if(infp->fp != NULL)
    fprintf(outfp, "*|| Resuming file %s at line %d\n", infp->name, infp->line);
}


char	*nextqfield(void)
{
  while(fields[fieldnumber] == NULL) rplcompgetline();
  return fields[fieldnumber++];
}

char	*nextfield(void)
{
  char	*cp;
  struct HASH	*hp;

  do {
    while(fields[fieldnumber] == NULL)
      rplcompgetline();
    if((hp = hashfind(hash_root, fields[fieldnumber])) != NULL &&
       hp->flag == HASH_F_DEF) {
      if((++infp - infiles) >= MAXINCLUDE ) {
	RPLCompError(TRUE, "Macro nesting level exceeded");
	infp--;
	hp = NULL;
	return (fields[fieldnumber++]);
      }
      if(fields[++fieldnumber] != NULL)
	cp = mystrcpy(fields[fieldnumber] - token_buf + line_buf);
      else
	cp = "";
      infp->name = cp;
      infp->fp = NULL;
      infp->line = (infp - 1)->line;
      strcpy(line_buf, hp->define);
      dofields();
    }
  } while(hp != NULL && hp->flag == HASH_F_DEF);
  return fields[fieldnumber++];
}

void	dofields(void)
{
  int	count = 0;
  char	*token, *cp;

  token = strcpy(token_buf, line_buf);
  if((cp = strtok(token, "\t\n\v\f\r ")) != NULL) {
    fields[count++] = cp;
    while((cp = strtok(NULL, "\t\n\v\f\r ")) != NULL && count < MAXTOKEN - 1)
      fields[count++] = cp;
  }
  fields[count] = NULL;
  fieldnumber = 0;
}

void	rplcompgetline(void)
{
  char	*cp, ch;
  int i;
  int Output;

  i = strlen(infp->name);
  if (! ((infp->name[i-2]=='.') &&
	     ((infp->name[i-1]=='h') || 
		  (infp->name[i-1]=='H'))))
	Output = TRUE;
  else
    Output = FALSE;

  in_next_file = FALSE;
  *fields = NULL;
  do {
    if(infp->fp != NULL) {
      if(fgets(line_buf, MAXBUFSIZE, infp->fp) == NULL)
	end_file();
      else {
	infp->line++;
	if((cp = strchr(line_buf, '\n')) != NULL)
	  *cp = '\0';
	else
	  do {
	    ch = getc(infp->fp);
	  } while(!feof(infp->fp) && !ferror(infp->fp) && ch != '\n');
	if(line_buf[0] == '*') {
	  if(commented)
	    fputs("*- ", outfp);
	  if (Output)
	  {
  	    fputs(line_buf, outfp);
  	    putc('\n', outfp);
	  }
	} else {
	  if (Output) 
	    outputf("* File\t%s\t%d\n", infp->name, infp->line);
	  dofields();
	  if(commented)
	    fprintf(outfp, "*- %s\n", line_buf);
	  else
	    if(*fields == NULL)
	      putc('\n', outfp);
	}
      }
    } else {
      strcpy(line_buf, infp->name);
      if(line_buf[0] != '\0')
	    FREE(infp->name);
      infp--;
/*	  if (Output) 
        outputf("* File\t%s\t%d\n", infp->name, infp->line);   */ /* wgg this is where ->name gets lost during define expand */
      dofields();
    }
  } while(*fields == NULL);
}

void	prologue(char *token)
{
  unsigned short	value;

  value = (c_stack_level > 0) ? ctrl_stack[c_stack_level - 1] : -1;
  if(value == 8 || value == 9) {
    if(arryp->type != NULL) {
      if(strcmp(arryp->type, token))
	RPLCompError(TRUE, "Mixed types in array: %s, %s", arryp->type, token);
    } else
      arryp->type = token;
  } else
    writeline("\tCON(5)\t=%s\n", token);
}

void	doarry(int link)
{
	/* do an array or linked array */
	/* link == 0:     ARRY:     ARRY dims [ obj ... ]
	           1:  LNKARRY:  LNKARRY dims [ obj _ ... ]
	           2: LNKOTHER: LNKOTHER dims [ obj _ ... ] */

    /* general outline:

get dims, then output our prolog, then start diversion.
Get objects, check for legality, output objects, remove diversion.
    */

  int	dims, items, i, linked, count;
  char	*token, *type;
  struct HASH	*hp;
  struct ITEM *item, *next;
  struct LINE *line;

  add_ctrl_stk(link + 8);
  arryp = push_ary();
  dims = 0;
  items = 1;
  do {
    token = nextfield();
    if(strcmp("[", token)) {
      while((hp = hashfind(hash_root, token)) != NULL && hp->flag != HASH_F_DEF)
	token = hp->define;
      if(strspn(token, str_table.dec) != strlen(token)) {
	RPLCompError(TRUE, "Illegal array dimension number:  %s", token);
	dims = -1; /* force abort */
	c_stack_level--;
	drop_ary(FALSE, arryp->dimens);
	return;
      }
    } else {
      if(dims != -1)
	break;
      c_stack_level--;
      drop_ary(FALSE, arryp->dimens);
      return;
    }
    dims++;
    i = atoi(token);
    items *= i;
    writeline("\tCON(5)\t%d\n", i);
  } while (1);
  if(link == 2)
    arryp->type = "DOOTHER";
  linked = 0;
  count = 0;
  add_obj(arryp);
  while(strcmp("]", (token = nextfield()))) {
    if(strcmp("_", token)) {
      fieldnumber--;
      if(rpl2asmb(1)) {
	linked++;
	count++;
	add_obj(arryp);
      }
    } else {
      add_obj(arryp);
      count++;
    }
  }
  if( arryp->type == NULL || ( link != 0 && linked == 0 ) ) {
    RPLCompError( TRUE, "Array object type not defined" );
    dims = -1;
  }
  if(dims == 0)
    items = count;
  if(items != 0 && count > items) {
    RPLCompError(TRUE, "Too many objects listed for array:  %d > %d", count, items);
    items = count;
    dims = -1;
  }
  if(link == 0 && items != 0 && (linked != count || linked != items)) {
    RPLCompError(TRUE, "ARRY has %d undefined objects.", items - linked);
    dims = -1;
  }
  if(!check_ctrl_stk(link + 8)) {
    RPLCompError(TRUE, "Improper nesting in %s", ctrl_table[link + 8].mnem);
    dims = -1;
  }
  if(dims == -1) {
    next = arryp->items;
    while((item = next) != NULL) {
      drop_ary(FALSE, item->item);
      next = item->next;
      FREE(item);
    }
    drop_ary(FALSE, arryp->dimens);
    FREE(arryp);
    arryp = NULL;
    return;
  }
  type = arryp->type;
  arryp->type = NULL;
  line = arryp->dimens;
  item = arryp->items;
  FREE(arryp);
  arryp = NULL;
  prologue((link != 0) ? "DOLNKARRY" : "DOARRY");
  writeline("\tREL(5)\tLBL%03d\n", label_cnt);
  writeline("\tCON(5)\t=%s\n", type);
  if(dims != 0)
    writeline("\tCON(5)\t%d\n", dims);
  else
    writeline("\tCON(5)\t1\n\tCON(5)\t%d\n", items);
  drop_ary(TRUE, line);
  next = item; /* temporary storage for later */
  if(link) {
    dims = 0;
    count = 0;
    while(item != NULL && dims < items) {
      if( ( item->item != NULL && count != 0 ) || count > 3 ) {
	writeline( "\tNIBHEX\t%0*d\n", count * 5, 0 );
	count = 0;
      }
      if(item->item != NULL)
	writeline("\tREL(5)\tLBL%03dA%03d\n", label_cnt, dims);
      else
	count++;
      dims++;
      item = item->next;
    }
    count += items - dims;
    while(count > 0) {
      if(count > 3)
	writeline("\tNIBHEX\t%020d\n", 0);
      else
	writeline("\tNIBHEX\t%0*d\n", count * 5, 0);
      count -= 4;
    }
    dims = 0;
    while((item = next) != 0 && dims < items) {
      if(link != 0 && item->item != NULL)
	writeline("LBL%03dA%03d\n", label_cnt, dims);
      drop_ary(TRUE, item->item);
      next = item->next;
      FREE(item);
      dims++;
    }
  } else
    while((item = next) != NULL) {
      drop_ary(TRUE, item->item);
      next = item->next;
      FREE(item);
    }
  writeline("LBL%03d\n", label_cnt++);
}

void	out_e_real(char *str)
{
  int	i;

  output("\tNIBHEX\t");
  i = 20;
  while(i > 15)
    out_ch(str[i--]);
  output("\n\tNIBHEX\t");
  i = 15;
  while(i > 0)
    out_ch(str[i--]);
  output("\n\tNIBHEX\t");
  out_ch(*str);
  out_ch('\n');
}

void	outreal(char *str)
{
  int	i;

  output("\tNIBHEX\t");
  i = 15;
  while(i > 12)
    out_ch(str[i--]);
  output("\n\tNIBHEX\t");
  i = 12;
  while(i > 0)
    out_ch(str[i--]);
  output("\n\tNIBHEX\t");
  out_ch(*str);
  out_ch('\n');
}

/* Input:
 *	token	= pointer to token
 *	mant	= mantissa size in nibbles
 *	exp	= exponent size in nibbles
 *	max	= maximum positive exponent
 */
char	*real_exp(char *token, int mant, int exp, long max)
{
  char	*cp = token, num[80];
  int	i, d = 0, j;
  long	value = 0;

  if(strlen(token) >= 80)	/* Ignore too long reals */
    return NULL;

  if(!*cp) return NULL;

  if (strspn(token, "0123456789.eE+-") != strlen(token)) /* if the real doesn't contain only these numbers, then error */
    return NULL;

  num[0] = (*cp != '-') ? '0' : '9';	/* Set sign nibble */
  if(*cp == '-' || *cp == '+')	/* Skip sign nibble */
    cp++;

  if(!*cp) return NULL;

  for(i = 1; i < 80; num[i++] = '\0');
  i = 0;

  if(*cp != '.') {
    while(*cp >= '0' && *cp <= '9')	/* count integer part */
      num[++i] = *cp++;
    if(!i) return NULL;
  }

  /* Here, only . or E or e or the end (JYA) */
  if ( ! ( !*cp || (*cp == '.') || (*cp == 'E') || (*cp == 'e') ) )
    return NULL;

  if(*cp == '.') {
    cp++;
    while(*cp >= '0' && *cp <= '9')	/* count decimal part */
      num[i + ++d] = *cp++;
  }


  while(num[1] == '0') {
    for(j = 1; num[j] != '\0'; j++)	/* normalize */
      num[j] = num[j + 1];
    i--;
  }

  if(num[1] == '\0') {
    for(i = 0; i < mant + exp + 1; i++)
      num[i] = '0';
    num[i] = '\0';
    return(mystrcpy(num));
  }

  j = d + i + 1;
  while(j <= mant)
    num[j++] = '0';

  if ( ! ( !*cp || (*cp == 'E') || (*cp == 'e') ) )
    return NULL;

  if(*cp == 'e' || *cp == 'E') {
    cp++;
    j = FALSE;
    if(*cp == '-' || *cp == '+') {
      j = (*cp == '-') ? TRUE : FALSE;
      cp++;
    }
    if(*cp < '0' || *cp > '9') return NULL;
    while(*cp >= '0' && *cp <= '9') {
      value *= 10;
      value += *cp++ - '0';
    }
    if(j)
      value = -value;
  }
  value += (long)i - 1L;
  if(value < 0L && -value > max) return NULL;
  if(value > max) return NULL;
  if(value < 0L)
    value = 2 * (max + 1) + value;
  while(exp) {
    num[mant + exp--] = (value % 10) + '0';
    value /= 10;
  }
  if(*cp != '\0') return NULL;
  return mystrcpy(num);
}

void	doreal(void)
{
  char	*real, *token;

  token = nextfield();
  if((real = real_exp(token, 12, 3, 499L)) == NULL) {
    RPLCompError(TRUE, "Invalid real number: %s", token);
    return;
  }
  prologue("DOREAL");
  outreal(real);
  FREE(real);
}

void	docmp(void)
{
  char	*re_tok, *im_tok, *re, *im;

  re_tok = mystrcpy(nextfield());
  im_tok = nextfield();
  if((re = real_exp(re_tok, 12, 3, 499L)) == NULL ||
     (im = real_exp(im_tok, 12, 3, 499L)) == NULL) {
    RPLCompError(TRUE, "Invalid complex number: %s %s", re_tok, im_tok);
    if(re != NULL)
      FREE(re);
    FREE(re_tok);
    return;
  }
  FREE(re_tok);
  prologue("DOCMP");
  outreal(re);
  outreal(im);
  FREE(re);
  FREE(im);
}

void	doereal(void)
{
  char	*real, *token;

  token = nextfield();
  if((real = real_exp(token, 15, 5, 49999L)) == NULL) {
    RPLCompError(TRUE, "Invalid extended real number: %s", token);
    return;
  }
  prologue("DOEREL");
  out_e_real(real);
  FREE(real);
}

void	doecmp(void)
{
  char	*re_tok, *im_tok, *re, *im;

  re_tok = mystrcpy(nextfield());
  im_tok = nextfield();
  if((re = real_exp(re_tok, 15, 5, 49999L)) == NULL ||
     (im = real_exp(im_tok, 15, 5, 49999L)) == NULL) {
    RPLCompError(TRUE, "Invalid extended complex number: %s %s", re_tok, im_tok);
    FREE(re_tok);
    if(re != NULL)
      FREE(re);
    return;
  }
  FREE(re_tok);
  prologue("DOECMP");
  out_e_real(re);
  out_e_real(im);
  FREE(re);
  FREE(im);
}

int	isrealnum(char *token)
{
  char	ch = *token;

  if(ch == '-' || ch == '+')
    token++;
  if(strlen(token) != 0 && strspn(token, str_table.dec) == strlen(token))
    return TRUE;
  return FALSE;
}

void	dobint2(char *token)
{
  char	*tok = token;
  int	neg;

  neg = (*tok == '-') ? TRUE : FALSE;
  if(neg || *tok == '+')
    tok++;
  while(*tok == '0')
    tok++;
  if(*tok == '\0')
    tok--;
  if(strlen(tok) > 7 ||
     (!neg && atol(tok) > 0xFFFFFL) ||
     (neg && atol(tok) > 0x80000L)) {
    RPLCompError(TRUE, "Numeric overflow: %s", token);
    return;
  }
  prologue("DOBINT");
  output("\tCON(5)\t");
  if(neg)
    output("(0-");
  output(tok);
  if(neg)
    out_ch(')');
  out_ch('\n');
}

void	dohexnum(char *token)
{
  if(strspn(token, str_table.hex) != strlen(token)) {
    RPLCompError(TRUE, "Invalid hex number: %s", token);
    return;
  }
  while(*token == '0')
    token++;
  if(*token == '\0')
    token--;
  if(strlen(token) > 5) {
    RPLCompError(TRUE, "Numeric overflow: %s", token);
    return;
  }
  prologue("DOBINT");
  output("\tCON(5)\t#");
  while(*token != '\0')
  out_ch((char)((strchr("abcdef", (int)*token)) ? (*token++ - 0x20) : *token++));
  out_ch('\n');
}

void	doromp(char *tok)
{
  char	*token, ch, *r_a, *r_b, *cp;

  token = (tok == NULL) ? mystrcpy(nextfield()) : tok;
  r_a = token;
  if(tok != NULL || !strchr("0123456789", *r_a) ||
     strspn(token, str_table.hex) != strlen(token)) {
    prologue("DOROMP");
    writeline("\tCON(6)\t=~%s\n", token);
    if(tok == NULL)
      FREE(token);
    return;
  }
  r_b = cp = nextfield();
  if(strspn(cp, str_table.hex) != strlen(r_b)) {
    RPLCompError(TRUE, "Invalid hex number: ROMPTR %s %s", token, cp);
    FREE(token);
    return;
  }
  while(*r_a == '0')
    r_a++;
  if(*r_a == '\0')
    r_a--;
  while(*r_b == '0')
    r_b++;
  if(*r_b == '\0')
    r_b--;
  if(strlen(r_a) > 3 || strlen(r_b) > 3) {
    RPLCompError(TRUE, "Numeric overflow: ROMPTR %s %s", token, cp);
    FREE(token);
    return;
  }
  prologue("DOROMP");
  output("\tCON(3)\t#");
  while((ch = *r_a) != '\0')
    out_ch((char)(strchr("abcdef", (int)ch) ? *r_a++ - 0x20 : *r_a++));
  out_ch('\n');
  output("\tCON(3)\t#");
  while((ch = *r_b) != '\0')
    out_ch((char)(strchr("abcdef", (int)ch) ? *r_b++ - 0x20 : *r_b++));
  out_ch('\n');
  FREE(token);
}

void	doflashp(char *tok)
{
  char	*token, ch, *r_a, *r_b, *cp;

  token = (tok == NULL) ? mystrcpy(nextfield()) : tok;
  r_a = token;
  if(tok != NULL || !strchr("0123456789", *r_a) ||
     strspn(token, str_table.hex) != strlen(token)) {
    prologue("DOFLASHP");
    writeline("\tCON(7)\t=^%s\n", token);
    if(tok == NULL)
      FREE(token);
    return;
  }
  r_b = cp = nextfield();
  if(strspn(cp, str_table.hex) != strlen(r_b)) {
    RPLCompError(TRUE, "Invalid hex number: FLASHPTR %s %s", token, cp);
    FREE(token);
    return;
  }
  while(*r_a == '0')
    r_a++;
  if(*r_a == '\0')
    r_a--;
  while(*r_b == '0')
    r_b++;
  if(*r_b == '\0')
    r_b--;
  if(strlen(r_a) > 3 || strlen(r_b) > 4) {
    RPLCompError(TRUE, "Numeric overflow: FLASHPTR %s %s", token, cp);
    FREE(token);
    return;
  }
  prologue("DOFLASHP");
  output("\tCON(3)\t#");
  while((ch = *r_a) != '\0')
    out_ch((char)(strchr("abcdef", (int)ch) ? *r_a++ - 0x20 : *r_a++));
  out_ch('\n');
  output("\tCON(4)\t#");
  while((ch = *r_b) != '\0')
    out_ch((char)(strchr("abcdef", (int)ch) ? *r_b++ - 0x20 : *r_b++));
  out_ch('\n');
  FREE(token);
}

void	doname(int tokflag, char *tok)
{
  char	*token, *cp, *namep, ch;
  unsigned char	hashflag;
  int	len;
  struct HASH	*hp;

  name_buf[0] = 'x';
  token = nextfield();
  namep = strcpy(name_buf + 1, token);
  if(tokflag == 2)		/* xNAME with x */
    namep = name_buf;
  if(tokflag == 3)		/* hNAME without x */
    namep = &name_buf[1];
  if(tokflag >= 4)		/* sNAME, tNAME, ALIAS */
    token = nextfield();
  if((hp = hashfind(hash_root, token)) == NULL && romid_flag == 1) {
    hp = hashadd(hash_root, namep, 1);
    hp->flag = romid_flag;
  } else
    if(hp != NULL) {
      hashflag = hp->flag;
      if(hashflag != HASH_F_0 && hashflag != HASH_F_EXT && hashflag != HASH_F_FEXT) {
	RPLCompError(TRUE, "Illegal name re-use: %s %s", tok, namep);
	return;
      }
    }
  len = strlen(token);
  cp = token;
  while((ch = *cp) != '\0') {
    if(ch != '\\') {
      cp++;
      continue;
    }
    if(cp[1] == '\\') {
      len--;
      cp += 2;
      continue;
    }
    if(strspn(cp + 1, str_table.hex) < 2) {
      RPLCompError(TRUE, "Bad hex character code:  %s", token);
      return;
    }
    len -= 2;
    cp++;
  }
  if(tokflag == 0)
    len = 0;
  if(!romid && romid_num && extfp != NULL) {
    fprintf(extfp, "%c%s\n", romid_flag + 'X', romid_num);
    romid = TRUE;
  }
  if(extfp != NULL) {
    if(tokflag==3)
      fprintf(extfp, "H%s %s\n", namep, token);
    else
      fprintf(extfp, "%x%s %s\n", len, namep, token);
  }
     
  if(tokflag != 5) {
    if(tokflag != 0)
      writeline("\tCON(6)\t=~%s\n", namep);
    writeline("=%s\n", namep);
  }
}

void	doinclob(void)
{
  char	*token;

  token = nextqfield();
  writeline("\tINCLUDE\t%s\n",token);

}

void	donameless( int flag )
{
  char	*token;
  struct HASH	*hp;

  /*
   *	if( romid_flag != 0 )
   *		RPLCompError( TRUE, "Meaningless in xROM:  %s %s", fields[fieldnumber - 1], fields[fieldnumber] );
   *
   */

  token = nextfield();
  if( ( hp = hashfind( hash_root, token ) ) != NULL )
    if( ( flag && hp->flag != HASH_F_0 ) || ( !flag && hp->flag != HASH_F_LOC ) )
      RPLCompError( FALSE, "Illegal name re-use:  %s", token );
  writeline( "%c%s\n", ( flag ) ? '=' : ':', token );
  if( !flag && hp == NULL ) {
    hp = hashadd( hash_root, token, 1 );
    hp->flag = HASH_F_LOC;
  }
}

void	doext(void)
{
  char	*token;
  register unsigned char	flag;
  struct HASH	*hp;

  token = nextfield();
  if((hp = hashfind(hash_root, token)) == NULL) {
    hp = hashadd(hash_root, token, 1);
    hp->flag = HASH_F_EXT;
    return;
  }
  flag = hp->flag;
  if(flag == HASH_F_EXT || flag == 0) {
    hp->flag = HASH_F_EXT;
    return;
  }
  RPLCompError(TRUE, "Illegal symbol re-use:  EXT %s", token);
}


void	dofext(void)
{
  char	*token;
  register unsigned char	flag;
  struct HASH	*hp;

  token = nextfield();
  if((hp = hashfind(hash_root, token)) == NULL) {
    hp = hashadd(hash_root, token, 1);
    hp->flag = HASH_F_FEXT;
    return;
  }
  flag = hp->flag;
  if(flag == HASH_F_FEXT || flag == 0) {
    hp->flag = HASH_F_FEXT;
    return;
  }
  RPLCompError(TRUE, "Illegal symbol re-use:  FEXT %s", token);
}

void	dolocal(void)
{
  char	*token;
  struct HASH	*hp;

  token = nextfield();
  if((hp = hashfind(hash_root, token)) == NULL) {
    hp = hashadd(hash_root, token, 1);
    hp->flag = HASH_F_LOC;
    return;
  }
  if(hp->flag != HASH_F_LOC)
    RPLCompError(TRUE, "Cannot make local:  LOCAL %s", token);
}

void	dodefine( void )
{
  char	*token, *cp;
  struct HASH	*hp;

  token = nextqfield();
  cp = fields[fieldnumber];
  if( cp == NULL )
    cp = "";
  else
    cp += line_buf - token_buf;
  if( ( hp = hashfind( hash_root, token ) ) != NULL ) {
    if( ( hp->flag != HASH_F_DEF ) || ( strcmp( hp->define, cp ) ) )
      RPLCompError( TRUE, "Cannot DEFINE %s", token );
  } else {
    hp = hashadd( hash_root, token, strlen( cp ) + 2 );
    hp->flag = HASH_F_DEF;
    strcpy( hp->define, cp );
  }
  while( ( cp = fields[fieldnumber] ) != NULL && strcmp( "(", cp ) )
    fieldnumber++; /* umh, ugly way of reading anything 'til end of line */
  if( cp != NULL )
    doparen();
  else
    rplcompgetline();
}

void	doromid( int flag )
{
  char	*token;

  token = nextfield();
  if( strspn( token, str_table.hex ) != strlen( token ) || strlen( token ) > 3)
    RPLCompError( TRUE, "Invalid hex number: ROMID %s", token );
  else {
    while( *token == '0' )
      token++;
    if( *token == '\0' )
      token--;
    if( romid && ( ( romid_flag != (unsigned char)flag ) || strcmp( token, romid_num ) ) )
      RPLCompError( TRUE, "Too many ROM IDs: ROMID %s", token );
    else {
      romid_num = mystrcpy( token );
      romid_flag = (unsigned char)flag;
      fprintf( outfp, "* ROMID set to %s\n", token );
    }
  }
}

void	dotitle( char *token )
{
  writeline( "\t%s", token );
  if( fields[fieldnumber] != NULL )
    writeline( "\t%s", fields[fieldnumber] - token_buf + line_buf );
  out_ch( '\n' );
  rplcompgetline();
}

void	doconditional(int flag, char *token )
{
  char *arg;
  writeline("\t%s",token);
  if (flag) {
    arg = nextfield();
    writeline("\t%s",arg );
  }
  out_ch( '\n' );
}

void	dothen(char *token)
{
  int	if_f = FALSE, else_f = FALSE;
  
  if(*token != '[' && (if_f = check_ctrl_stk(5)) == 0 && (else_f = check_ctrl_stk(6)) == 0)
    stack_err(token);
  if(*token != '(') {
    if(*token != '[') {
      output("\tCON(5)\t=SEMI\n");
      output("*** THEN ***\n");
      if(if_f)
	output("\tCON(5)\t=RPIT\n");
      if(else_f)
	output("\tCON(5)\t=RPITE\n");
    }
    else
      output("\tCON(5)\t=THEN\n");
  }
}

void	doparen(void)
{
  char	*token, ch, *tempo;

  if(infp->fp == NULL) {
    rplcompgetline();
    return;
  }
  tempo = nextqfield();
  do {
    token = tempo - token_buf + line_buf;
    if(!commented)
      out_ch('*');
    if(!commented)
      out_ch(' ');
    while((ch = *token) != '\0' && ch != ')') {
      if(!commented)
	out_ch(ch);
      token++;
    }
    if(!commented)
      out_ch('\n');
    if(*token == ')') {
      while(fields[fieldnumber] != NULL &&
	    (token - line_buf) >= (fields[fieldnumber] - token_buf)) {
	fieldnumber++;
      }
      break;
    }
    rplcompgetline();
    tempo = fields[0];
  } while(1);
  if(*++token != '\0' && !strchr("\t\n\v\f\r ", *token)) {
    --fieldnumber;
    fields[fieldnumber] = token - line_buf + token_buf;
  }
}

void	dosemi(char *token)
{
  if(*token != '[')
    if(!check_ctrl_stk(0) && !check_ctrl_stk(12) && !check_ctrl_stk(11))
      stack_err(token);
  if(*token != '(')
    output("\tCON(5)\t=SEMI\n");
  return;
}

int	istoken(char *token)
{
  if(!strcmp(fields[0], token)) {
    if(force_comment)
      writeline("*- %s\n", line_buf);
    else
      writeline("* %s\n", fields[0] - token_buf + line_buf);
    fieldnumber = 0;
    return TRUE;
  }
  if(fields[1] == NULL)
    return FALSE;
  if(strcmp(fields[1], token))
    return FALSE;
  if(fields[0] != token_buf && fields[0] != token_buf + 1)
    return FALSE;
  if(force_comment)
    writeline("*- %s\n%s\n", line_buf, token_buf);
  else
    writeline("* %s\n%s\n", fields[1] - token_buf + line_buf, token_buf);
  fieldnumber = 1;
  return TRUE;
}

void	docode(char *token)
{
  if(!force_comment)
    writeline("* %s\n", token - token_buf + line_buf);
  in_next_file = FALSE;
  commented = FALSE;
  prologue("DOCODE");
  writeline("\tREL(5)\tLBL%03d\n", label_cnt);
  do {
    rplcompgetline();
    if(in_next_file || istoken("RPL")) {
      RPLCompError(TRUE, "Unmatched directive: CODE");
      label_cnt++;
      if(in_next_file)
	return;
      break;
    }
    if(istoken("ENDCODE")) {
      writeline("LBL%03d\n", label_cnt++);
      break;
    }
    if(istoken("ASSEMBLE"))
      continue;
    if(istoken("LOOP")) {
      output("\tA=DAT0\tA\n");
      output("\tD0=D0+\t5\n");
      output("\tPC=(A)\n");
      continue;
    }
    output(line_buf);
    out_ch('\n');
  } while (1);
  fieldnumber++;
  commented = force_comment;
}

void	doendcode(void)
{
  RPLCompError(TRUE, "Unmatched directive: ENDCODE");
}

void	dorpl(void)
{
  if(!force_comment)
    fputs("* RPL\n", outfp);
  commented = force_comment;
}

void	doassemble(char *token)
{
  int	i, flag = FALSE;

  if(!force_comment)
    writeline("* %s\n", token - token_buf + line_buf);
  in_next_file = FALSE;
  commented = FALSE;
  do {
    rplcompgetline();
    if(istoken("RPL")) {
      nextqfield();
      commented = force_comment;
      return;
    }
    if(istoken("ENDCODE")) {
      RPLCompError(TRUE, "Unmatched directive: ENDCODE");
      nextqfield();
      commented = force_comment;
      return;
    }
    if(istoken("ASSEMBLE"))
      continue;
    for(i=0; (parse_ftab4[i].token != NULL && !flag); i++)
      if(istoken(parse_ftab4[i].token)) {
	parse_ftab4[i].function(parse_ftab4[i].par1, parse_ftab4[i].par2);
	flag = TRUE;
      }
    if(flag)
      continue;
    if(istoken("LOOP")) {
      output("\tA=DAT0\tA\n");
      output("\tD0=D0+\t5\n");
      output("\tPC=(A)\n");
    } else {
      output(line_buf);
      out_ch('\n');
    }
  } while(!in_next_file);
  return;
}

int	get_str(char *token, unsigned char sep, int *flag, char **last)
{
  char	ch, *cp, *cptr;

  cptr = str_buf;
  *last = token + 1;
  if(sep != '\0' && *flag == FALSE)
    if(*token++ != '"')
      return -1;
  *flag = FALSE;
  while((*cptr++ = ch = *token++) != '\0') {
		*last = token;
		if(sep != '\0' && ch == '"') {
			if(*token == '"') {
				token++;
				continue;
			}
			*--cptr = '\0';
			if(*token != '\0' && !strchr("\t\n\v\f\r ", *token))
				return -1;
			return cptr - str_buf;
		} else {
			if(ch != '\\')
				continue;
			if(*token == '\0') {
				*flag = TRUE;
				*--cptr = '\0';
				return cptr - str_buf;
			}
			if((cp = strchr(chr_aliases.names, *token)) != NULL) {
				cptr[-1] = chr_aliases.values[cp - chr_aliases.names];
				token++;
				continue;
			}
			if(strspn(token, str_table.hex) < 2)
				return -1;
			cptr[-1] = 16 * ((strchr(str_table.hex, *token++) - str_table.hex) & 0x0F);
			cptr[-1] += (strchr(str_table.hex, *token++) - str_table.hex) & 0x0F;
    }
  }
  if(sep != '\0')
		return -1;
	return cptr - str_buf - 1;
}

void	dostring(char *mnem, char *token, unsigned char sep)
{
  int	flag = FALSE, first_flag = FALSE, token_flag, len, done = 0;
  char	*cptr, *next;
  unsigned char	ch;

  token_flag = (token < token_buf || (token - token_buf) >= MAXBUFSIZE) ? FALSE : TRUE;
  if(token_flag && sep != '\0')
    token += line_buf - token_buf;
  if((len = get_str(token, sep, &flag, &next)) != -1) {
    prologue(mnem);
    first_flag = flag;
    if(first_flag) {
      if(sep != '\0')
	writeline("\tREL(5)\tLBL%03d\n", label_cnt);
      else
	writeline("\tCON(2)\t((LBL%03d)-(*))/2-1\n", label_cnt);
    }
    else {
      if(sep != '\0')
	writeline("\tCON(5)\t%d\n", 2 * len + 5);
      else
	writeline("\tCON(2)\t%d\n", len);
    }
  }
  do {
    cptr = str_buf;
    while(len-- > 0) {
      ch = *cptr++;
      if(ch != '\\' && ch >= ' ' && ch <= '~') {
	if(done++ == 0)
	  output("\tNIBASC\t\\");
	out_ch(ch);
	done = done % 20; /* max 20 characters in one line definition */
	if(done == 0)
	  output("\\\n");
      } else {
	if(done !=  0)
	  output("\\\n");
	done = 0;
	writeline("\tCON(2)\t#%X\n", ch);
      }
    }
    if(flag && !token_flag)
      RPLCompError(TRUE, "Continued %s inside DEFINITION: %s", mnem + 2, token);
    if(!flag || !token_flag) {
      if(done != 0)
	output("\\\n");
      if(first_flag)
	writeline(" LBL%03d\n", label_cnt++);
      if(len == -2)
	RPLCompError(TRUE, "Invalid string:  %s", token);
      if(next < line_buf || (next - line_buf) >= MAXBUFSIZE)
	return;
      next += token_buf - line_buf;
      while(fields[fieldnumber] != NULL && next >= fields[fieldnumber])
	fieldnumber++;
      return;
    }
    rplcompgetline();
    len = get_str(token = line_buf, sep, &flag, &next);
  } while (1);
}

void	dochr(void)
{
  char	*cp, *token;
  unsigned char ch;

  token = nextfield();
  switch(strlen(token)) {
  case 1:
    if(*token == '"') {
      token += line_buf - token_buf;
      if(token[2] == '"') {
	token++;
	nextfield();
      }
    }
    break;
  case 2:
    if(*token == '^' && token[1] >= '?' && token[1] <= '_') {
      token++;
      *token ^= 0x40;
      break;
    }
    if(*token == '\\')
      if((cp = strchr(chr_aliases.names, token[1])) != NULL) {
	*++token = chr_aliases.values[cp - chr_aliases.names];
	break;
      }
  case 3:
    if(*token == '"' && token[2] == '"') {
      token++;
      break;
    }
    if(*token == '\\') {
      cp = token + 1;
      if(strspn(cp, str_table.hex) == 2) {
	*token = ((strchr(str_table.hex, *cp++) - str_table.hex) & 0x0F) << 4;
	*token += (strchr(str_table.hex, *cp) - str_table.hex) & 0x0F;
	break;
      }
    }
  case 4:
    if(*token == '"' && token[1] == '^' &&
       token[2] >= '?' && token[2] <= '_' && token[3] == '"') {
      token += 2;
      *token ^= 0x40;
      break;
    }
    if(*token == '"' && token[1] == '\\')
      if((cp = strchr(chr_aliases.names, token[2])) != NULL && token[3] == '"') {
	*++token = chr_aliases.values[cp - chr_aliases.names];
	break;
      }
  case 5:
    if(*token == '"' && token[1] == '\\') {
      cp = token + 2;
      if(strspn(cp, str_table.hex) == 2 && token[4] == '"') {
	*token = ((strchr(str_table.hex, *cp++) - str_table.hex) & 0x0F) << 4;
	*token += (strchr(str_table.hex, *cp) - str_table.hex) & 0x0F;
	break;
      }
    }
  default:
    RPLCompError(TRUE, "Invalid char: %s", token);
    return;
  }
  prologue("DOCHAR");
  ch = (unsigned char)*token;
  if(ch >= ' ' && ch <= '~' && ch != '\\')
    writeline("\tNIBASC\t\\%c\\\n", ch);
  else
    writeline("\tCON(2)\t%d\n", ch);
}

void	dohxs(char *def, char *tok)
{
  char	*cp1, *cp2, *cp3;
  int	len, done, todo;

  done = 16;
  todo = 0;
  cp2 = cp1 = mystrcpy(nextfield());
  if(strspn(cp1, str_table.hex) != strlen(cp2)) {
    RPLCompError(TRUE, "Invalid hex number: %s %s", tok, cp1);
    FREE(cp1);
    return;
  }
  while(*cp1 != '\0') {
    todo <<= 4;
    /* check is this '+=' or just a '=' ?? */
    todo += ((strchr(str_table.hex, *cp1++) - str_table.hex) & 0x0F);
  }
  if(todo == 0)
    return;
  cp3 = nextfield();
  if(strspn(cp3, str_table.hex) != strlen(cp3)) {
    RPLCompError(TRUE, "Invalid hex number: %s %s %s", tok, cp1, cp3);
    FREE(cp2);
    return;
  }
  len = strlen(cp3);
  if(len > todo) {
    RPLCompError(TRUE, "Numeric overflow: %s %s %s", tok, cp2, cp3);
    FREE(cp2);
    return;
  }
  FREE(cp2);
  prologue(def);
  writeline("\tCON(5)\t%d", todo + 5);
  if(todo == 0) {
    out_ch('\n');
    return;
  }
  while(todo-- > len) {
    if(done == 16) {
      output("\n\tNIBHEX\t");
      done = 0;
    }
    out_ch('0');
    done++;
  }
  while(*cp3 != '\0') {
    if(done == 16) {
      output("\n\tNIBHEX\t");
      done = 0;
    }
    out_ch((char)((strchr("abcdef", *cp3)) ? *cp3++ - 0x20 : *cp3++));
    done++;
  }
  out_ch('\n');
}

/* Added by Mika */

/* MPH: New compiler for infinite precision integers */
void	dozint(char *def, char *tok)
{
  char	*cp1, *cp2, *cp3;
  int   sgn,zeros,blen,todo;

  cp1 = cp2 = mystrcpy(nextfield());

  /* Check optional sign character */

  sgn = 0;
  if(*cp2 == '+')
    cp2++;
  else if(*cp2 == '-') {
    sgn = 9;
    cp2++;
  }

  blen = strlen(cp2);

  /* Check body is entirely decimal */

  if((int)strspn(cp2, str_table.dec) != blen) {
    RPLCompError(TRUE, "Invalid integer: %s %s", tok, cp1);
    FREE(cp1);
    return;
  }

  /* Count number of leading zeros */

  cp3 = cp2;
  
  zeros = 0;
  while(*cp3 == '0') {
    zeros++;
    cp3++;
  }

  /* Output the prolog */

  prologue(def);
  
  /* Special code for zero */

  if(zeros == blen) {
    writeline("\tCON(6)\t6\n");
    return;
  }

  /* Size field */

  writeline("\tCON(5)\t%d",blen-zeros+5+1);

  /* The body is output in reverse order compared to input! */

  output("\n\tNIBHEX\t");
  todo = blen-zeros;
  cp3 += todo;
  while(todo-- > 0) {
    cp3--;
    out_ch(*cp3);
  }
  out_ch((char)('0'+sgn));
  out_ch('\n');

  FREE(cp1);
}


void	dotag(void)
{
  dostring("DOTAG", nextfield(), '\0');
  add_ctrl_stk(7);
  while(!rpl2asmb(1));
  if(!check_ctrl_stk(7))
    RPLCompError(TRUE, "Improper nesting in TAG");
}



void	doinclude(void)
{
  char	*token;
  char *next;
  int flag=FALSE;
    
  token = nextfield();
  if (*token=='"') {                 /* wgg check for long file name in quotes */
		if (get_str(&line_buf[token-token_buf], '"', &flag, &next)>0)   /* str_buf has file name */
			token=str_buf;
  }
  if((infp - infiles) == MAXINCLUDE) {
    RPLCompError(TRUE, "INCLUDE nesting too deep:  %s", token);
    return;
  }
  infp++;
  infp->line = 0;
  infp->name = NULL;
  if((infp->fp = openfile(token, "RPL_INCLUDE", (char *)NULL, "rt", &infp->name)) == NULL) {
    infp--;
    RPLCompError(TRUE, "Cannot open INCLUDE file: %s", token);
    return;
  }
  fprintf(outfp, "*|| Reading from %s\n", infp->name);
  rplcompgetline();
}

void	doendbind(void)
{
  RPLCompError(TRUE, "Unmatched control word: }}");
}

void	dobind(void)
{
  int	i, j, binds, flg;
  char	*token;
  static char tmpname[23][15];
  char s[16];			      /* wgg used for xGETLAM expansions  */
  char  *envlam;
  long int lamaddr = 0;
  if ((envlam = getenv(ENVLAM))) {
    if(strcmp(envlam, "GETLAM") == 0) 
	  lamaddr = -1;       /* wgg generate xGETLAM for {{   }}    */
    else
      lamaddr = strtol(envlam,(char **)NULL,16);
  }

  binds = 0;
  flg = FALSE;
  do {
    token = nextqfield();
    if(strcmp("}}",token)) {
      strncpy(tmpname[binds], token, 14);
      tmpname[binds][15] = '\0';
      binds++;
    }
    else
      flg = TRUE;
  }
  while (!flg && (binds<23));

  if(binds>22) {
    RPLCompError(TRUE, "Too many lambda bindings");
    binds = 22;
  }
  else if(binds==0) {
    RPLCompError(TRUE, "Missing lambda bindings");
    binds = 1;
  }

  /* Output the binding sequence */

  if(binds==1)
    writeline("\tCON(5)\t=1LAMBIND\n");
  else if(binds==2)
    {
      writeline("\tCON(5)\t='\n");
      writeline("\tCON(5)\t=NULLLAM\n");
      writeline("\tCON(5)\t=DUPTWO\n");
      writeline("\tCON(5)\t=DOBIND\n");
    }
  else
    {
      writeline("\tCON(5)\t='\n");
      writeline("\tCON(5)\t=NULLLAM\n");
      writeline("\tCON(5)\t=%s\n", lamtable[binds-1].ntoken);
      writeline("\tCON(5)\t=NDUPN\n");
      writeline("\tCON(5)\t=DOBIND\n");
    }

  /*  wgg  */
  /*  With or without leading "=" equates lead to SLOAD errors about duplicate definitions                */
  /*  Although the code runs fine, the errors are annoying.                                               */
  /*  T1 = 2GETLAM does not work because the 2 makes it look like a number, T1 = (=2GETLAM) gets identified */
  /*  but is an error because externals are not allowed in the =                                          */
  /*  When we output abs values, assembler is ok, even allows re-define BUT SLOAD gives a duplicate error */
  /*  if some other module reuses the name.  In the latter case, SLOAD uses the first definition - this means */
  /*  if the first def was 1GETLAM and the second needed 5GETLAM both get 1GETLAM (different nullam envs but  */
  /*  the wrong xGETLAM calls!                                                                            */

  /* Output the lambda name equates  */
  for(i=0; i<binds; i++) {
    j = binds-i-1;
	if (lamaddr == -1) {  /* wgg - Process the lambda name equates as DEFINE; allow reDEFINE as a new lambda */
      strcpy(s, tmpname[i]);
      stuff_lambda(s, lamtable[j].getname, j+1); 
      strcat(s, "@");
      stuff_lambda(s, lamtable[j].getname, j+1);
      strcpy(s, tmpname[i]);
      strcat(s, "!");
      stuff_lambda(s, lamtable[j].putname, j+1); 
      strcpy(s, "!");
      strcat(s, tmpname[i]);
      stuff_lambda(s, lamtable[j].putname, j+1); 
	}
    else
		if (lamaddr) {                            /* wgg correct HP49 {{ }} generation, constant used to be *16 !! */
		  writeline("=%s\t=\t#%lX\t* %s\n",  tmpname[i], lamaddr + j * 10, lamtable[j].getname);
		  writeline("=%s@\t=\t#%lX\t* %s\n", tmpname[i],lamaddr + j * 10, lamtable[j].getname);
		  writeline("=%s!\t=\t#%lX\t* %s\n", tmpname[i],lamaddr + j * 10 + DIFPUTGET, lamtable[j].putname);
		  writeline("=!%s\t=\t#%lX\t* %s\n", tmpname[i],lamaddr + j * 10 + DIFPUTGET, lamtable[j].putname);
		}
		else {
		  writeline("=%s\t=\t%s\t* %s\n",  tmpname[i], lamtable[j].getaddr, lamtable[j].getname);
		  writeline("=%s@\t=\t%s\t* %s\n", tmpname[i], lamtable[j].getaddr, lamtable[j].getname);
		  writeline("=%s!\t=\t%s\t* %s\n", tmpname[i], lamtable[j].putaddr, lamtable[j].putname);
		  writeline("=!%s\t=\t%s\t* %s\n", tmpname[i], lamtable[j].putaddr, lamtable[j].putname);
		}
  }  
}

/* wgg used for the GETLAM processing  */
int stuff_lambda(char *tmpname, char *lamname, int j)    //    wgg - enter one DEFINE entry into hash table
{
  struct HASH	*hp;							/* wgg   */

  if( ( hp = hashfind( hash_root, tmpname ) ) != NULL ) {
    if( ( hp->flag != HASH_F_DEF ) || ( hp->lambda == 0 ) )        /* we allow a reDEFINE for lambda variables as a new lambda ONLY */
      RPLCompError( TRUE, "Cannot reDEFINE %s as a lambda", tmpname );
	return FALSE;
  } else {
    hp = hashadd( hash_root, tmpname, strlen( lamname ) + 2 );
    hp->flag = HASH_F_DEF;
    hp->lambda = j;                            /* we will track which lambda it is but only use the non-zero for testing */
    strcpy( hp->define, lamname );
	writeline("*\t\t%s\t = %s\n", tmpname, lamname);
    return TRUE;
  }  
}        


int	parse_token(char *token)
{
  int	i = 0, flag = FALSE, link;
  char	**cp;

  if( ( *token == '(' && token[strlen( token ) - 1] == ')' ) ||
      ( *token == '[' && token[strlen( token ) - 1] == ']' ) ) {
    flag = TRUE;
    ++token;
    token[strlen( token ) - 1] = '\0';
  }
  while(i < 15) {
    if(!strcmp(token, tok_table[i].mnem)) {
      if(flag) {
	token--;
	if(*token == '(')
	  token[strlen(token)] = ')';
	else
	  if(*token == '[')
	    token[strlen(token)] = ']';
      }
      if(*token == '[') {
	writeline("\tCON(5)\t=%.*s\n", strlen(token) - 2, token + 1);
	return TRUE;
      }
      cp = tok_table[i].table;
      link = tok_table[i].next;
      if(*token != '(') {
	if(link == 3 || link == 0 || link == 12 || link == 11)
	  prologue(*cp);
	else
	  while(*cp != NULL) {
	    writeline((**cp == '*') ? "%s\n" : "\tCON(5)\t=%s\n", *cp);
	    cp++;
	  }
      }
      if(tok_table[i].prev != -1)
	if(check_ctrl_stk(tok_table[i].prev) == FALSE)
	  stack_err(token);
      if(link != -1)
	add_ctrl_stk(link);
      if(*token == '(')
	return TRUE;
      if(link != 3 && link != 0 && link != 12 && link != 11)
	return TRUE;
      link = c_stack_level;
      while(link <= c_stack_level)
	rpl2asmb(1);
      return TRUE;
    }
    i++;
  }
  if(flag) {
    token--;
    if(*token == '(')
      token[strlen(token)] = ')';
    else
      if(*token == '[')
	token[strlen(token)] = ']';
  }
  return FALSE;
}

int	rpl2asmb(int count)
     /* count is number of tokens to parse */
{
  int	i, step, flag, ret_flag = TRUE;
  char	*token, *real;
  struct HASH	*hp;

  step = (count == 0) ? 0 : 1;
  while((count -= step) >= 0) {
    token = nextfield();
    flag = FALSE;
    if(!parse_token(token)) {
      for(i=0; (parse_ftab1[i].token != NULL && !flag); i++)
	if(!strcmp(parse_ftab1[i].token, token)) {
	  parse_ftab1[i].function(token);
	  ret_flag = parse_ftab1[i].flag;
	  flag = TRUE;
	}
      if(flag)
	continue;
      for(i=0; (parse_ftab2[i].token != NULL && !flag); i++)
	if(!strcmp(parse_ftab2[i].token, token)) {
	  parse_ftab2[i].function();
	  ret_flag = parse_ftab2[i].flag;
	  flag = TRUE;
	}
      if(flag)
	continue;
      for(i=0; (parse_ftab3[i].token != NULL && !flag); i++)
	if(!strcmp(parse_ftab3[i].token, token)) {
	  parse_ftab3[i].function(parse_ftab3[i].parameter);
	  ret_flag = parse_ftab3[i].flag;
	  flag = TRUE;
	}
      if(flag)
	continue;
      for(i=0; (parse_ftab4[i].token != NULL && !flag); i++)
	if(!strcmp(parse_ftab4[i].token, token)) {
	  parse_ftab4[i].function(parse_ftab4[i].par1, parse_ftab4[i].par2);
	  ret_flag = parse_ftab4[i].flag;
	  flag = TRUE;
	}
      if(flag)
	continue;

      for(i=0; (parse_ftab5[i].token != NULL && !flag); i++)
	if(!strcmp(parse_ftab5[i].token, token)) {
	  parse_ftab5[i].function(parse_ftab5[i].par1, parse_ftab5[i].par2);
	  flag = TRUE;
	}
      if(flag)
	continue;
      if(*token == '"') {
	dostring("DOCSTR", token, '"');
	continue;
      }
      if(!strcmp(token, "#")) {
	dohexnum(nextfield());
	continue;
      }
      if(*token == '#')
	if(strspn(token + 1, str_table.hex) == (strlen(token) - 1)) {
	  dohexnum(++token);
	  continue;
	}
      if(!strcmp(token, "$")) {
	dostring("DOCSTR", nextfield(), '"');
	continue;
      }
      if(!strcmp(token, "LAM")) {
	dostring("DOLAM", nextfield(), '\0');
	continue;
      }
      if(!strcmp(token, "ID")) {
	dostring("DOIDNT", nextfield(), '\0');
	continue;
      }
      if(!strcmp(token, "ROMPTR")) {
	doromp(NULL);
	continue;
      }
      if(!strcmp(token, "FLASHPTR")) {
	doflashp(NULL);
	continue;
      }
      if(!strcmp(token, "HXS")) {
	dohxs("DOHSTR", "HXS");
	continue;
      }
      if(!strcmp(token, "GROB")) {
	dohxs("DOGROB", "GROB");
	continue;
      }
      /* Added by Mika */
      /* MPH: Compile INT as infinite precision integer */
      if(!strcmp(token,"ZINT")) {
	dozint("DOINT","ZINT");
	continue;
      }

      if(!strcmp(token, "PTR")) {
	writeline("\tCON(5)\t=%s\n", nextfield());
	continue;
      }
      if(!strcmp(token, "[#]")) {
	prologue("DOBINT");
	ret_flag = FALSE;
	continue;
      }
      if(isrealnum(token)) {
	dobint2(token);
	continue;
      }
      /* We now try to see if the token is a real (JYA) */
      if((real = real_exp(token, 12, 3, 499L)) != NULL) {
	prologue("DOREAL");
	outreal(real);
	FREE(real);
	continue;
      }
      hp = hashfind(hash_root, token);
      if(hp != NULL && hp->flag == HASH_F_EXT)
	doromp(token);
      else
	if(hp != NULL && hp->flag == HASH_F_FEXT)
	  doflashp(token);
	else
	  writeline("\tCON(5)\t%c%s\n", (hp != NULL && hp->flag == HASH_F_LOC) ? ':' : '=', token);
    }
  }
  return ret_flag;
}

void rplcompget_options( int argc, char **argv )
{
  char	i;

  while( ( i = getopt( argc, argv, "Cd:vXl" ) ) != (char)( -1 ) )
    switch( i ) {
    case 'C':
      DEBUG_PRN( "Forcing comments !\n" );
      force_comment = commented = TRUE;
      break;
    case 'd':
      debug = TRUE;
      dbgname = optarg;
      DEBUG_PRN( "Debug file is '%s'\n", dbgname );
      break;
    case 'v':
      rplcompusage( TRUE );
      exit( 0 );
      break;
    case 'X':
      exiterr = FALSE;
      break;
    case 'l':
      MASD_LongCteError= FALSE;
      break;      
    default:
      rplcompusage( FALSE );
      exit( -1 );
      break;
    }
}

#ifdef dll
int	rplcompmain( int argc, char **argv )
#else
int	main( int argc, char **argv )
#endif
{
  char *std_sign = "-";

  outfp = stdout;
  extfp = NULL;
  dbgfile = stdout;
  out_name = NULL;
  progname = *argv;
  rplcompget_options( argc, argv );
  if( ( argc - optind ) > 4 ) {
    rplcompusage( FALSE );
    exit( -2 );
  }
  switch( argc - optind + 1 ) {
  case 4:
    /*			if( !strcmp( argv[optind + 2], std_sign ) )
			ext_name = std_sign;
			else
    */				ext_name = argv[optind+2];
  /*				ext_name = conc_ext( argv[optind + 2], "ext" ); */
  case 3:
    /*			if( !strcmp( argv[optind + 1], std_sign ) )
			out_name = std_sign;
			else
    */				out_name = argv[optind+1];
				/* out_name = conc_ext( argv[optind + 1], "a" ); */
  case 2:
    /*			if( !strcmp( argv[optind], std_sign ) )
			in_name = std_sign;
			else
    */				in_name = argv[optind]; /* conc_ext( argv[optind], "s" ); */
  if( out_name == NULL)
    out_name = conc_ext( argv[optind], "a" );
  break;
  case 1:
    out_name = in_name = std_sign;
    break;
  default:
    rplcompusage( FALSE );
    exit( -3 );
    break;
  }

  infp = infiles;
  infp->fp = stdin;
  infp->name = mystrcpy( in_name );
  if( strcmp( in_name, std_sign ) )
    if( !( infp->fp = fopen( in_name, "r" ) ) ) {
      perror( in_name );
      exit( 1 );
    }
  if( strcmp( out_name, std_sign ) )
    if( !( outfp = fopen( out_name, "w" ) ) ) {
      perror( out_name );
      exit( 1 );
    }
  if( ext_name != NULL ) {
    if( strcmp( in_name, std_sign ) ) {
      if( !( extfp = fopen( ext_name, "w" ) ) ) {
	perror( ext_name );
	exit( 1 );
      }
    }
    else
      extfp = stdout;
  }
  if( dbgname != NULL && strcmp( dbgname, std_sign ) )
    if( !( dbgfile = fopen( dbgname, "wt" ) ) ) {
      perror( dbgname );
      exit( 1 );
    }

  hash_root = hashmake( 1253 );
  rpl2asmb( 0 );
  return(0);
}
