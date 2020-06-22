#ifndef _SATURN_H_
#define _SATURN_H_

/* headers for LIF and Saturn file information */

typedef unsigned char	b_8;
typedef unsigned short	b_16;
typedef unsigned long	b_32;
typedef char		i_8;
typedef short		i_16;
typedef long		i_32;

typedef char		bit_8[1];
typedef char		bit_16[2];
typedef char		bit_32[4];

/* ubit_* is a different name only for purposes of documentation */
typedef unsigned char		ubit_8[1];
typedef unsigned char		ubit_16[2];
typedef unsigned char		ubit_32[4];

#ifdef	__MSDOS__

#define DIRC '\\'
#define DRIVE ':'
#define BIN_READONLY "rb"
#define TEXT_READONLY "rt"
#define BIN_WRITE "wb"
#define TEXT_WRITE "wt"
#define BIN_READWRITE "w+b"
#define TEXT_READWRITE "w+t"
#define BIN_APPEND "ab"
#define TEXT_APPEND "at"
#define PATH_SEPARATOR ';'

#else	/*MSDOS*/

#define DIRC '/'
#define DRIVE '\0'
#define BIN_READONLY "r"
#define TEXT_READONLY "r"
#define BIN_WRITE "w"
#define TEXT_WRITE "w"
#define BIN_READWRITE "w+"
#define TEXT_READWRITE "w+"
#define BIN_APPEND "a"
#define TEXT_APPEND "a"
#define PATH_SEPARATOR ':'

#endif	/*MSDOS*/

#undef	TRUE
#define	TRUE	1

#undef	FALSE
#define	FALSE	0

#ifndef	UNKNOWN
#define	UNKNOWN	-1
#endif

#define VOLUMEID    0x8000

#define	SEC_TEXT    0xE0D5
#define	SDATA	    0xE0D0
#define	DATA	    0xE0F0
#define	SEC_DATA    0xE0F1
#define	BIN	    0xE204
#define	LEX	    0xE208
#define	KEY	    0xE20C
#define	BASIC	    0xE214
#define	FORTH	    0xE218
#define	ROM	    0xE21C
#define	HP71	    0xE200

struct LIF {
    char    name[10];
    ubit_16 type;
    ubit_32 start;
    ubit_32 length;
    b_8     time[6]; /* YY MM DD HH MM SS */
    ubit_16 volume;
    ubit_32 impl;
};

struct VOLUME {
    ubit_16 lifid;
    char    label[6];
    bit_32  start;
    ubit_16 s3000;
    ubit_16 dummy0;
    bit_32  dirlen;
    ubit_16 version;
    ubit_16 zero;
};

#define SAT_ID "Saturn3"

#define romid saturnromid
typedef struct SATURN {
    char    id[7];
    ubit_16 length;
    ubit_32 nibbles;
    ubit_16 symbols;
    ubit_16 references;
    ubit_32 start;
    ubit_8  absolute;
    ubit_8  filler;
    char    date[26];
    char    title[40];
    char    softkeys[20];
    char    version[26];
    ubit_32 romid;
    char    reserved[117];
  /* Size of the Header must be 256 bytes */
} OBJHEAD, *OBJHEADPTR;

#define OS_RESOLVED 0x8000
#define OS_RELOCATABLE 0x4000
#define OS_REFERENCEMASK 0x3FFF

typedef struct o_symbol {
    char os_name[SYMBOL_LEN];
    ubit_16 os_fillcount;
    bit_32 os_value;
} O_SYMBOL, *O_SYMBOLPTR;

#define SYMRECLEN sizeof(struct o_symbol)
#define SYMBS_PER_RECORD (int) (252 / sizeof(struct o_symbol))

typedef struct o_fillref {
    ubit_8 of_class;
    ubit_8 of_subclass;
    bit_32 of_address;
    bit_32 of_bias;
    bit_16 of_length;
    char   of_reserved[30];
} O_FILLREF, *O_FILLREFPTR;


#endif
