#include "sload.h"
#include "debug.h"

#include "code.h"

/* Code access routines:
 *	int getnib(address) -- returns the contents of (address)
 *	void putnib(address, value) -- sets (address) to (value)
 *	void readcode(address, nibs) -- read 'nibs' nibbles from curfile
 *	void writecode(address, nibs) -- write 'nibs' nibbles to objfile
 * i_32 address;
 * int value;
 * i_32 nibs;
 */

static char *codeptr[MAX_NIBBLES / NIB_BLOCKSIZE + 1];
static char *nullptr = NULL;

char *getblock(void)
{
    char *temp;

    temp = calloc((unsigned) 1, (unsigned) BYTE_BLOCKSIZE);
    if (temp == (char *) NULL)
	error("G_BLK: %s", "unable to get more memory");
    DEBUG_PRN("Address %ld: allocated %d bytes (getblock)", (long) temp,
							    BYTE_BLOCKSIZE);
    return temp;
}

char *getnullptr(void)
{
    if (nullptr == (char *) NULL) {
	/* Need to get a code block for the null pointer */
	nullptr = getblock();
    }
    return nullptr;
}

int getnib(i_32 address)
{
    int block = ((b_32) address) >> NIB_BLOCKBITS;
    char *byteptr;

    DEBUG_PRN("Getting nibble from address %05lX", (long) address);
    if (codeptr[block] == (char *) NULL)
	return 0;			/* block has never been written to */
    byteptr = codeptr[block] + ((address & NIB_OFFSETMASK) >> 1);
    /* byteptr points to the byte containing the nibble */
    if (address & 1)
	return (*byteptr & 0x0F);
    else
	return (*byteptr >> 4) & 0x0F;
}

void putnib(i_32 address, int value)
{
    int block = ((b_32) address) >> NIB_BLOCKBITS;
    char *byteptr;

    DEBUG_PRN("Putting nibble to address %05lX", (long) address);
    if (codeptr[block] == (char *) NULL) {
	/* block has never been written to -- allocate memory here */
	codeptr[block] = getblock();
    }
    byteptr = codeptr[block] + ((address & NIB_OFFSETMASK) >> 1);
    /* byteptr points to the byte containing the nibble */
    if (address & 1)
	*byteptr = (*byteptr & 0xF0) | (value & 0xF);
    else
	*byteptr = (*byteptr & 0x0F) | ((value & 0xF) << 4);
}

void putbyte(i_32 address, int value)
{
    int block = ((b_32) address) >> NIB_BLOCKBITS;
    register char *byteptr;
    register i_16 v = value & 0xff;

    DEBUG_PRN("Putting byte to address %05lX", (long) address);
    if (codeptr[block] == (char *) NULL) {
	/* block has never been written to -- allocate memory here */
	codeptr[block] = getblock();
    }
    byteptr = codeptr[block] + ((address & NIB_OFFSETMASK) >> 1);
    /* byteptr points to the byte containing the nibble */
    *byteptr = (v | (v << 8)) >> 4;		/* swap nibbles */
}

#define PUTBYTE(byte) { \
    *codep++ = byte; \
    if (--bytes_left <= 0) { \
	if ((codep=codeptr[++cur_block]) == (char *) NULL) \
	    codep = codeptr[cur_block] = getblock(); \
	bytes_left = BYTE_BLOCKSIZE; \
    } \
}

void readcode(i_32 addr, i_32 nibs)
{
    /* Read 'nibs' nibbles from 'curfile', write them starting at 'addr' */
    /* Algorithm:
     *  Check if 'addr' is even or odd:
     *    If even, read (nibs / 2) bytes from the file, putting the bytes
     *      directly into code memory.
     *    If odd, must do shifts/etc to the incoming bytes (still can read
     *      the nibbles a byte a a time, however)
     *  Use getc(FILE) to read a byte from the file.
     *  The "write into code memory" is somewhat more complicated than
     *    indicated above, as special efforts need to be taken to deal
     *    with block boundaries.  The details of this are hidden in the
     *    PUTBYTE() macro (see definition above).
     */

    i_32 bytes_left;	/* Bytes left in this code block */
    i_32 cur_block;	/* Current block pointer */
    char *codep;	/* Pointer to code block */
    register int new_byte; /* Value of new byte */
    register int next_byte; /* Value of next byte read */
    int byte_offset;	/* Byte offset within the initial block */

    cur_block = ((b_32) addr) >> NIB_BLOCKBITS;

    if ((codep=codeptr[cur_block]) == (char *) NULL) {
	/* block has never been written -- allocate memory here */
	codep = codeptr[cur_block] = getblock();
    }
    byte_offset = (addr & NIB_OFFSETMASK) >> 1;
    bytes_left = BYTE_BLOCKSIZE - byte_offset;
    codep += byte_offset;
    /* codep points to the byte containing the nibble */
    if (addr & 1) {
	if ((next_byte=getc(curfile)) == EOF)
	    error("error reading from %s", curname);

	/* Use PUTBYTE to write the first nibble, together with the other */
	/* nibble of the byte, which is already in the code record        */
	new_byte = ((next_byte >> 4) & 0xF) | (*codep & 0xF0);
	PUTBYTE(new_byte);

	--nibs;				/* One nibble already written */

	while (nibs >= 2) {
	    new_byte = (next_byte & 0xF) << 4;	/* Set even nibble */
	    if ((next_byte=getc(curfile)) == EOF)
		error("error reading from %s", curname);
	    new_byte |= (next_byte >> 4) & 0xF;	/* Set odd nibble */
	    PUTBYTE(new_byte);
	    nibs -= 2;
	}
    } else {
	i_32 write_bytes = nibs >> 1; /* Number of BYTES to write */
	register i_32 rc = bytes_left;

	if (rc > write_bytes)	/* don't read more than asked for */
	    rc = write_bytes;
	while (write_bytes > 0) {
	    if (rc != fread(codep, 1, rc, curfile))
		error("error reading from %s", curname);
	    codep += rc;
	    write_bytes -= rc;
	    if ((bytes_left -= rc) <= 0) {
		if ((codep=codeptr[++cur_block]) == (char *) NULL)
		    codep = codeptr[cur_block] = getblock();
		bytes_left = BYTE_BLOCKSIZE;
	    }
	    if ((rc = write_bytes) > bytes_left) rc = bytes_left;
	}

	if (nibs & 1) {
	    /* One nibble left to do; read it */
	    if ((next_byte=getc(curfile)) == EOF)
		error("error reading from %s", curname);
	    next_byte >>= 4;
	}
    }

    /* Check if one more nibble to write */
    if (nibs & 1) {
	new_byte = ((next_byte & 0xF) << 4) | ((*codep >> 4) & 0xF);
	PUTBYTE(new_byte);
    }
}

#define NEXTBYTE(codep, cur_block, bytes_left) { \
    if (--bytes_left <= 0) { \
	codep = codeptr[++cur_block]; \
	if (codep == (char *) NULL) \
	    codep = getnullptr(); \
	bytes_left = BYTE_BLOCKSIZE; \
    } \
}

#define GETBYTE(byte, codep, cur_block, bytes_left) { \
    byte = *codep++; \
    NEXTBYTE(codep, cur_block, bytes_left); \
}

void writecode(i_32 addr, i_32 nibs)
{
    /* Write 'nibs' nibbles to 'objfile', starting with nibble 'addr'. */
    /* Algorithm:  see the comments for readcode() */
    /* This routine is similar except for the direction of transfer, */
    /* and the provisions for hex code */

    i_32 bytes_left;	/* Bytes left in this code block */
    i_32 cur_block;	/* Current block pointer */
    char *codep;	/* Pointer to code block */
    register int new_byte; /* Value of new byte */
    register int next_byte; /* Value of next byte read */
    int byte_offset;	/* Byte offset within the initial block */
    i_32 fill_bytes;	/* Number of trailing bytes to write */

    if (hexcode) {
	/* Write '0's until starting address */
	bytes_left = addr;
	while (bytes_left-- > 0) {
	  if (fputc('0', objfile) == EOF)
	    error("WR_CO: error writing to %s", objname);
	}
	/* Hex code simply loops calling getnib() and putc() */
	while (nibs-- > 0) {
	    if (fputc("0123456789ABCDEF"[getnib(addr++)], objfile) == EOF)
		error("WR_CO: error writing to %s", objname);
	}
	return;
    }

    /* If codeonly == TRUE, write the code from 0 through (addr+nibs)   */
    /* Note that the starting address is really 0.                      */
    if (codeonly) {
	/* Change addr and nibs to reflect the updated values */
	/* Write the code from 0 through (addr+nibs)        */
      if (!compile_absolute) {
	nibs += addr;
	addr = 0;
      }
    }
    cur_block = ((b_32) addr) >> NIB_BLOCKBITS;

    byte_offset = (addr & NIB_OFFSETMASK) >> 1;
    bytes_left = BYTE_BLOCKSIZE - byte_offset;

    codep=codeptr[cur_block];
    if (codep == (char *) NULL)
	codep = getnullptr();
    codep += byte_offset;
    /* codep points to the byte containing the nibble */

    fill_bytes = (256 - (((nibs + 1) / 2) & 0xFF)) & 0xFF;

    if (addr & 1) {
	/* Odd base address -- manipulate nibbles */

	GETBYTE(next_byte, codep, cur_block, bytes_left);

	while (nibs >= 2) {
	    if (codeonly) {
		new_byte = next_byte & 0xF;
		GETBYTE(next_byte, codep, cur_block, bytes_left);
		new_byte |= next_byte & 0xF0;
	    } else {
		new_byte = (next_byte & 0xF) << 4;	/* Set even nibble */
		GETBYTE(next_byte, codep, cur_block, bytes_left);
		new_byte |= (next_byte >> 4) & 0xF;	/* Set odd nibble */
	    }
	    if (putc(new_byte, objfile) == EOF)
		error("WR_CO: error writing to %s", objname);
	    nibs -= 2;
	}
    } else {
	/* Even base address -- copy bytes to output file */

	i_32 write_bytes = nibs >> 1; /* Number of BYTES to write */

	if (codeonly)
	    while (--write_bytes >= 0) {
	        GETBYTE(new_byte, codep, cur_block, bytes_left);
		    /* Swap nibbles in the byte for -H option */
		    new_byte = ((new_byte & 0xF) <<4) | ((new_byte >> 4) & 0xF);
	        if (putc(new_byte, objfile) == EOF)
		    error("WR_CO: error writing to %s", objname);
	    }
	else {
	    register i_32 rc = bytes_left;

	    if (rc > write_bytes)	/* don't read more than asked for */
	        rc = write_bytes;
	    while (write_bytes > 0) {
	        if (fwrite(codep, 1, rc, objfile) != rc)
		    error("WR_Cw: error writing to %s", objname);
	        codep += rc;
	        write_bytes -= rc;
	        if ((bytes_left -= rc) <= 0) {
	            if ((codep=codeptr[++cur_block]) == (char *) NULL)
		        codep = getnullptr();
		    bytes_left = BYTE_BLOCKSIZE;
	        }
	        if ((rc = bytes_left) > write_bytes) rc = write_bytes;
	    }
	}

	if (nibs & 1) {
	    /* One nibble left; read it */
	    GETBYTE(new_byte, codep, cur_block, bytes_left);
	    next_byte = new_byte >> 4;
	}
    }
    /* Check for an odd number of nibbles written to the file */
    if (nibs & 1) {
	/* One nibble left; write it to the file */
	if (codeonly)
	    new_byte = next_byte & 0xF;
	else
	    new_byte = (next_byte & 0xF) << 4;

	if (putc(new_byte, objfile) == EOF)
	    error("WR_CO: Error writing to %s", objname);
    }
    if (codeonly == FALSE) {
	/* Fill to end of code record */
	while (fill_bytes-- > 0) {
	    if (putc(0, objfile) == EOF)
		error("WR_CO: error writing to %s", objname);
	}
    }
}

/* P(x) = x^16 + x^14 + x^13 + x^11 + 1;  1 0110 1000 0000 0001 */
#define POFX 0x6801 /* Polynomial function */

#define DOCRC(nib, hibit, lowbit, j, bit, dofx) \
    for (j = hibit; j != lowbit; j>>=1) { \
	bit = ((j & nib) != 0); \
	if (dofx & 0x8000) \
	    dofx = (((dofx & 0x7FFF) << 1) + bit) ^ POFX; \
	else \
	    dofx = (dofx << 1) + bit; \
    }

/* SETUPCRC sets the read pointer to the byte containing the first nibble  */
/* Note that if read_addr is odd, only one nibble of the byte will be read */
/* initially in CHECKCRC.  The exit condition from CHECKCRC leaves the     */
/* pointer in the same relative position as SETUPCRC.                      */

#define SETUPCRC(read_addr, cur_block, byte_offset, bytes_left, codep) { \
    cur_block = ((b_32) read_addr) >> NIB_BLOCKBITS; \
    byte_offset = (read_addr & NIB_OFFSETMASK) >> 1; \
    bytes_left = BYTE_BLOCKSIZE - byte_offset; \
    codep=codeptr[cur_block]; \
    if (codep == (char *) NULL) \
	codep = getnullptr(); \
    codep += byte_offset; \
}

#define CHECKCRC(read_addr,value,codep,cur_block,bytes_left,j,bit,dofx) { \
    if (read_addr & 1) { \
	GETBYTE(value, codep, cur_block, bytes_left); \
/*	DOCRC(value, 8, 0, j, bit, dofx); */\
	dofx = (dofx << 4) ^ (value & 0xf) ^ Crc[dofx >> 12 & 0xf];\
    } \
    for (i=(read_addr+1)& (~ (i_32) 1); i<read_addr+READSIZE-1; i+=2) { \
	GETBYTE(value, codep, cur_block, bytes_left); \
/*	DOCRC(value, 128, 0, j, bit, dofx); */\
	dofx = (dofx << 4) ^ (value >> 4 & 0xf) ^ Crc[dofx >> 12 & 0xf];\
	dofx = (dofx << 4) ^ (value & 0xf) ^ Crc[dofx >> 12 & 0xf];\
    } \
    if (read_addr & 1) { \
	/* Don't use GETBYTE, as it advances the code pointer! */ \
	/* Select the high nibble of the byte */ \
/*	DOCRC(*codep, 128, 8, j, bit, dofx); */\
	dofx = (dofx << 4) ^ (*codep >> 4 & 0xf) ^ Crc[dofx >> 12 & 0xf]; \
    } \
}

/**************************************

Lewis CRC 

From Dan Rudolph, Stan Blascow.

Converted to nibble-wide algorithm based on symbolic execution of four
bits at a time:

Initial CRC	     New nibble
ABCD EFGH IJKL MNOP abcd
  AA  A             A
   B B B             B
     CC C             C
     AA A             A
      DD  D            D
      AA  A            A
      BB  B            B
--------------------------
     AAAA A         A AA
     BB   B          B B
     CC C             C
      DD  D            D

Therefore, a table indexed by ABCD can be built as the net value to XOR
in after the new nibble has been shifted in.  This table is called Crc[]
in the code that follows.

**************************************/

i_32 computecrc(i_32 start, i_32 offset, i_32 no_reads, i_32 no_sectors)
{
  /* 'start' is the address of the first nibble to read */
    /* 'offset' is the nibble offset of the second half of the block */
    /* 'no_reads' is the number of reads per sector */
    /* 'no_sectors' is the number of sectors to be read */
    /* The total number of nibbles checked by this routine is: */
    /*   no_sectors * no_reads * (2 * offset) * READSIZE */

    /* P(x) = x^16 + x^14 + x^13 + x^11 + 1;  1 0110 1000 0000 0001 */
    static b_16 Crc[] = {0x0000, 0x6801, 0xd002, 0xb803,  /* pre-computed */
			 0xc805, 0xa004, 0x1807, 0x7006,  /* CRC sums */
			 0xf80b, 0x900a, 0x2809, 0x4008,
			 0x300e, 0x580f, 0xe00c, 0x880d};

    i_32 read_addr1, read_addr2;/* Current read address */
    i_32 loop_reads;		/* Loop counter for no_reads */
    i_32 dofx = 0;		/* Polynomial divisor */
    i_32 i;			/* loop counter */
    i_32 value;			/* Nibble value */
    i_32 left1, left2;		/* Bytes left in this code block */
    int byte_offset;		/* Byte offset within the initial block */
    i_32 block1, block2;	/* Current block pointer */
    char *codep1, *codep2;	/* Pointer to code block */

    DEBUG_PRN("COMPUTECRC: Start = %05lX, Offset = %05lX",(long)start,(long)offset);
    DEBUG_PRN("COMPUTECRC: Reads = %05lX, Sectors = %05lX",
				    (long) no_reads, (long) no_sectors);

    while (no_sectors-- > 0) {
	read_addr1 = start;
	SETUPCRC(read_addr1, block1, byte_offset, left1, codep1);
	read_addr2 = start + offset;
	SETUPCRC(read_addr2, block2, byte_offset, left2, codep2);

	loop_reads = no_reads;
	while (loop_reads-- > 0) {
	    CHECKCRC(read_addr1, value, codep1, block1, left1, j, bit, dofx);
	    read_addr1 += READSIZE;
	    CHECKCRC(read_addr2, value, codep2, block2, left2, j, bit, dofx);
	    read_addr2 += READSIZE;
	}
	start += offset * 2;
    }
    return dofx & 0xffff;
}

#define CHECKKCRC(read_addr,value,codep,cur_block,bytes_left,j,bit,dofx) { \
    if (read_addr & 1) { \
	GETBYTE(value, codep, cur_block, bytes_left); \
	dofx = (dofx >> 4) ^ (((value ^ dofx) & 0xf) * 0x1081);\
    } \
    for (i=(read_addr+1)& (~ (i_32) 1); i<read_addr+READSIZE-1; i+=2) { \
	GETBYTE(value, codep, cur_block, bytes_left); \
	dofx = (dofx >> 4) ^ ((((value >> 4) ^ dofx) & 0xf) * 0x1081);\
	dofx = (dofx >> 4) ^ (((value ^ dofx) & 0xf) * 0x1081);\
    } \
    if (read_addr & 1) { \
	/* Don't use GETBYTE, as it advances the code pointer! */ \
	/* Select the high nibble of the byte */ \
	dofx = (dofx >> 4) ^ ((((*codep >> 4) ^ dofx) & 0xf) * 0x1081);\
    } \
}

/**************************************


Kermit CRC 

From "KERMIT - A File Transfer Protocol" by Frank da Cruz, Digital
Press, 1987.  P. 257.

**************************************/

i_32 computekcsum(i_32 start, i_32 end)
{
  /* 'start' is the address of the first nibble to read */
    /* 'end' is the address of the first nibble of the stored CRC */

    /* P(x) = 1 + x^5 + x^12 + x^16;  1 0000 1000 0001 (0001) */

    i_32 read_addr;		/* Current read address */
    i_32 dofx = 0;		/* Polynomial divisor */
    i_32 i;			/* loop counter */
    i_32 value;			/* Nibble value */
    i_32 bytes_left;		/* Bytes left in this code block */
    int byte_offset;		/* Byte offset within the initial block */
    i_32 cur_block;		/* Current block pointer */
    char *codep;		/* Pointer to code block */

    DEBUG_PRN("COMPUTEKCSUM: Start = %05lX, End = %05lX",(long)start,(long)end);

    read_addr = start;
    cur_block = ((b_32) read_addr) >> NIB_BLOCKBITS;
    byte_offset = (read_addr & NIB_OFFSETMASK) >> 1;
    bytes_left = BYTE_BLOCKSIZE - byte_offset;
    codep=codeptr[cur_block];
    if (codep == (char *) NULL)
	codep = getnullptr();
    codep += byte_offset;

    if (read_addr & 1) {
	GETBYTE(value, codep, cur_block, bytes_left);
	dofx = (dofx >> 4) ^ (((value ^ dofx) & 0xf) * 0x1081);
    }
    for (i=(read_addr+1)& (~ (i_32) 1); i<end-1; i+=2) {
	GETBYTE(value, codep, cur_block, bytes_left);
	dofx = (dofx >> 4) ^ ((((value >> 4) ^ dofx) & 0xf) * 0x1081);
	dofx = (dofx >> 4) ^ (((value ^ dofx) & 0xf) * 0x1081);
    }
    if (end & 1) {
	/* Don't use GETBYTE, as it advances the code pointer! */
	/* Select the high nibble of the byte */
	dofx = (dofx >> 4) ^ ((((*codep >> 4) ^ dofx) & 0xf) * 0x1081);
    } 
    dofx &= 0xffff;
    dofx |= dofx << 16;		/* swap nibble order */
    return (dofx & 0xf0f00) >> 4 | (dofx & 0xf0f000) >> 12;
}

i_32 computekcrc(i_32 start, i_32 offset, i_32 no_reads, i_32 no_sectors)
{
  /* 'start' is the address of the first nibble to read */
    /* 'offset' is the nibble offset of the second half of the block */
    /* 'no_reads' is the number of reads per sector */
    /* 'no_sectors' is the number of sectors to be read */
    /* The total number of nibbles checked by this routine is: */
    /*   no_sectors * no_reads * (2 * offset) * READSIZE */

    /* P(x) = 1 + x^5 + x^12 + x^16;  1 0000 1000 0001 (0001) */

    i_32 read_addr1, read_addr2;/* Current read address */
    i_32 loop_reads;		/* Loop counter for no_reads */
    i_32 dofx = 0;		/* Polynomial divisor */
    i_32 i;			/* loop counter */
    i_32 value;			/* Nibble value */
    i_32 left1, left2;		/* Bytes left in this code block */
    int byte_offset;		/* Byte offset within the initial block */
    i_32 block1, block2;	/* Current block pointer */
    char *codep1, *codep2;	/* Pointer to code block */

    DEBUG_PRN("COMPUTEKCRC: Start = %05lX, Offset = %05lX",(long)start,(long)offset);
    DEBUG_PRN("COMPUTEKCRC: Reads = %05lX, Sectors = %05lX",
				    (long) no_reads, (long) no_sectors);

    while (no_sectors-- > 0) {
	read_addr1 = start;
	SETUPCRC(read_addr1, block1, byte_offset, left1, codep1);
	read_addr2 = start + offset;
	SETUPCRC(read_addr2, block2, byte_offset, left2, codep2);

	loop_reads = no_reads;
	while (loop_reads-- > 0) {
	    CHECKKCRC(read_addr1, value, codep1, block1, left1, j, bit, dofx);
	    read_addr1 += READSIZE;
	    CHECKKCRC(read_addr2, value, codep2, block2, left2, j, bit, dofx);
	    read_addr2 += READSIZE;
	}
	start += offset * 2;
    }
    return chksum(dofx,0xffff);
}
/**********************************************************************/
int chksum(int crc, int oldcks)
{	/* calculate proper checksum to produce final CRC of FFFF */

    unsigned short q, r = crc, s = oldcks;

/* first take the last 4-nib back out of the CRC */
    r = (((r >> 12) * 0x811) ^ (r << 4) ^ (s & 0xf));
    s >>= 4;
    r = (((r >> 12) * 0x811) ^ (r << 4) ^ (s & 0xf));
    s >>= 4;
    r = (((r >> 12) * 0x811) ^ (r << 4) ^ (s & 0xf));
    s >>= 4;
    r = (((r >> 12) * 0x811) ^ (r << 4) ^ (s & 0xf));

/* now force the  crank to turn our way. */
    s = 0xf831;		/* required MSNs to make goal */
    q = (q<<4) | ((r ^ s) & 0xf);	/* get 1st (least sig) nib */
    r = (r>>4) ^ ((s & 0xf) * 0x1081);
    s >>= 4;
    q = (q<<4) | ((r ^ s) & 0xf);
    r = (r>>4) ^ ((s & 0xf) * 0x1081);
    s >>= 4;
    q = (q<<4) | ((r ^ s) & 0xf);
    r = (r>>4) ^ ((s & 0xf) * 0x1081);
    s >>= 4;
    q = (q<<4) | ((r ^ s) & 0xf);
    return q;
}

/**************************************


Erni CRC  (not really a CRC, but it does detect order perterbations on
the target)

From Jim Pearson's code.

**************************************/

i_32 computeecrc(i_32 start, i_32 end)
{

    i_32 read_addr;		/* Current read address */
    i_32 dofx = 0;		/* Polynomial divisor */
    i_32 value;			/* Nibble value */
    i_32 bytes_left;		/* Bytes left in this code block */
    int byte_offset;		/* Byte offset within the initial block */
    i_32 cur_block;		/* Current block pointer */
    char *codep;		/* Pointer to code block */

    DEBUG_PRN("COMPUTEECRC: Start = %05lX, End = %05lX",(long)start, (long)end);

    read_addr = start;
    SETUPCRC(read_addr, cur_block, byte_offset, bytes_left, codep);
    if (read_addr < end && read_addr++ & 1) {
	GETBYTE(value, codep, cur_block, bytes_left);
	dofx = (dofx << 4) + (value & 0xf) + ((dofx >> 4) & 0xf) + 1;
	dofx += ((dofx & 0xf000) >> 12) + 1;
    } 
    for (read_addr&=(~ (i_32) 1); read_addr < (end&~(i_32) 1); read_addr+=2) { 
	GETBYTE(value, codep, cur_block, bytes_left); 
	dofx = (dofx << 4) + ((value >> 4) & 0xf) + ((dofx >> 4) & 0xf) + 1;
	dofx += ((dofx & 0xf000) >> 12) + 1;
	dofx = (dofx << 4) + (value & 0xf) + ((dofx >> 4) & 0xf) + 1;
	dofx += ((dofx & 0xf000) >> 12) + 1;
    } 
    if (read_addr < end) {
	/* Don't use GETBYTE, as it advances the code pointer! */ 
	/* Select the high nibble of the byte */ 
	dofx = (dofx << 4) + ((*codep >> 4) & 0xf)+((dofx >> 4) & 0xf) + 1;
	dofx += ((dofx & 0xf000) >> 12) + 1;
    } 
    dofx &= 0xffff;
    dofx |= dofx << 16;		/* swap nibble order */
    return (dofx & 0xf0f00) >> 4 | (dofx & 0xf0f000) >> 12;
}
