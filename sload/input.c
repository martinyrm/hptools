#ifdef dll
#include "dllfile.h"
#endif
#include "sload.h"
#include "debug.h"

#include "input.h"


/* local declarations */
int column, lastcolumn;
char *lastptr;

char read_char(void)
{
    char c = *ptr;

    lastcolumn = column;
    lastptr = ptr;

    if (c == '\t') {
	column += 8 - (column - 1) % 8;
	++ptr;
    } else {
	if (c != '\0') {
	    ++ptr;
	    ++column;
	}
    }
    return c;
}

void unread_char(void)
{
    ptr = lastptr;
    column = lastcolumn;
}

int getfield(char *fieldp, int maxlength, char *separators)
{
    char *tmp, *tabp;
    int length = 0;
    int is_endquote = FALSE;

    if (strchr(separators, *ptr) == (char *) NULL) {	/* NOT a separator */
	if (*ptr == '"') { /* name starts with ", will use long name option */
		ptr++;	/* adjust starting pointer and column */
		column++;
		if ((tmp = strchr(ptr,'"')) == (char *) NULL) { /* there's no ending " */
		    length = strlen(ptr);
		    tmp = &ptr[length];
		}
		else {
		    *tmp = '\0';	/* Remove ending " */
		    is_endquote = TRUE;
		    length = tmp - ptr;
		}
		/* Removing trailing non-visible characters */
		/* all remaining ending separator will be removed */
     		while(strchr(" ,\t\n", *(--tmp) ) ) {
        	   *tmp = '\0';
        	   length--;
        	}
        	tmp++;
	}
	else {
		if ((tmp = strpbrk(ptr, separators)) == (char *) NULL) {
		    length = strlen(ptr);
		    tmp = &ptr[length];
		} else {
		    length = tmp - ptr;
		}
	}
	
	if (length > maxlength) {
	    ERRMSG1("Field too long");
	    length = maxlength;
	}

	(void) strncpy(fieldp, ptr, length);

	tabp = strchr(ptr, '\t');
	if (tabp != (char *) NULL && tabp < tmp) {
	    /* at least one tab in the field */
	    while (ptr < tmp)
		(void) read_char();	/* read_char() adjusts column */
	} else {
	    /* no tabs in the field */
	    column += length;
	    ptr = tmp;
	}
        if (is_endquote) {	/* skip endquote if necessary */
	    column++; ptr++;
	}
    }
    fieldp[length] = '\0';
    return (length != 0);
}

void skipwhite(void)
{
    char c;

    while ((c = *ptr) == ' ' || c == '\t') {
	++ptr;
	if (c == ' ')
	    ++column;
	else
	    column += 8 - (column - 1) % 8;
    }
}

void skipsep(void)
{
    if (*ptr == ',')
	(void) read_char();
    skipwhite();
}

char *sloadgetline(char *lineptr, FILE *file)
{
    char *pt;

    column = 0;		/* Reset column counter */

    if (fgets(lineptr, LINE_LEN, file) == (char *) NULL)
	return (char *) NULL;
    /* Check if the line is longer than LINE_LEN */
    if (strlen(lineptr) == LINE_LEN-1 && lineptr[LINE_LEN-1] != '\n') {
	char tempbuf[LINE_LEN];

	/* This line is too long */
	ERRMSG1("Input line too long (extra characters ignored)");
	do {
	    pt = fgets(tempbuf, LINE_LEN, file);
	} while (pt != (char *) NULL && strlen(tempbuf) == LINE_LEN-1
			    && tempbuf[LINE_LEN-1] != '\n');
    }
    if ((pt=strchr(lineptr, '\n')) != (char *) NULL) /* \n -> end of string */
	*pt = '\0';
    /* Check for a CR at the end of the line */
    if ((pt=strrchr(lineptr, '\r')) != (char *) NULL && pt[1] == '\0')
	*pt = '\0';
    if (*lineptr == '\032')	/* ^Z at start of line */
	*lineptr = '\0';	/* Ignore this line */
    return lineptr;
}

i_32 read_addr(void)
{
    char c;
    i_32 addr = 0;

    /* Read an address, either '*' or <hex string> */
    skipsep();	/* skip any leading whitespace */
    c = read_char();
    if (c == '*') {
	/* Use current address */
	addr = next_addr;
	/* Check for illegal character following this */
	c = *ptr;
	if (c != '\0' && strchr(CSEPARATORS, c) != (char *) NULL) {
	    ERRMSG1("Invalid address (* must be used alone)");
	    addr = -1;
	}
	return addr;
    }
    while (strchr(CSEPARATORS, c) == (char *) NULL) {
	if (! isxdigit(c)) {
	    ERRMSG2("Unrecognized hexadecimal character '%c'", c);
	    addr = -1;
	    break;
	}
	addr = (addr << 4) + fromhex(c);
	c = read_char();
    }
    if (addr != -1)
	unread_char();

    if (addr >= MAX_NIBBLES) {
	ERRMSG1("Address too large");
	addr = -1;
    }

    return addr;
}

i_16 read_cmd(void)
{
    i_16 nextcmd = NUll;
    char c;

    /* Get two characters, return ((first<<8)+second) */

    /* Skip any leading whitespace */
    skipsep();

    /* Ready to get a command now - read the first character into 'c' */
    c = read_char();
    /* If 'c' is '\0', nextcmd is NUll (('\0' << 8) + '\0') */
    if (islower(c))
	c = toupper(c);
    nextcmd = c << 8;
    
    /* Read second character */
    c = read_char();
    if (islower(c))
	c = toupper(c);
    if (strchr(CSEPARATORS, c) == (char *) NULL)
	nextcmd |= c & 0xFF;
    else
	unread_char();	/* Terminator character -- put it back */

    /* Skip past remaining text and first separator */
    /* Note that strchr("..", '\0') matches on the terminating '\0' */
    while (strchr(CSEPARATORS, c) == (char *) NULL)
	c = read_char();

    unread_char();	/* put back last character read (terminator char) */

    return nextcmd;
}

char *read_filespec(void)
{
    static char filename[LINE_LEN+1];

    skipsep();	/* Skip any leading whitespace */
    if (getfield(filename, LINE_LEN, SEPARATORS) == FALSE) {
	/* No file specifier found on line */
	return (char *) NULL;
    }
    return filename;
}

char *read_symbol(char *symbptr)
{
    skipsep();	/* Skip any leading whitespace */
    if (getfield(symbptr, SYMBOL_LEN, SEPARATORS) == FALSE) {
	/* No symbol name found on line */
	return (char *) NULL;
    }
    return symbptr;
}

char *read_string(void)
{	/* get any string left on line.  Loses column info */

    char *end, sep;

    skipsep();		/* skip whitspace */
    if (! *ptr) 	/* die if no string */
	return (char *) NULL;
    if ((sep = *ptr) == '"' || sep == '\'') {	/* if quoted */
	ptr++;
	if ((end = strrchr(ptr, sep)) != (char *) NULL)
	    *end = '\0';
    }
    return ptr;
}

void copyline(char *outline, char *inpline, int length)
{
    int col = 0;
    char c;
    static char blank[] = "        ";

    while ((c = *inpline++) && col < length) {
	if (c != '\t') {
	    /* regular character */
	    *outline++ = c;
	    ++col;
	} else {
	    /* found a tab */
	    (void) strcpy(outline, &blank[col % 8]);
	    outline += 8 - col % 8;
	    col += 8 - col % 8;
	}
    }
    *outline = '\0';
}
