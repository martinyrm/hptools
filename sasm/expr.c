#include "sasm.h"

#include "expr.h"
#include "symbols.h"
#include "opcode.h"

#ifdef dll
#include "dllfile.h"
#endif

EXPRPTR left, right;

char operator[] =  {START,'(',')','!','&','~','+','-','*','/','%','^',
		    /* <, >, ==, <=, >=, <> */
		    '<','>','=',(char)LESSEQUAL,(char)MOREEQUAL,(char)DIFFERENT,'\0'};
int precedence[] =   {1,   2,  3,  5,  6,  7,  7,  7,  8,  8,  9,  10, 4, 4, 4, 4, 4, 4, 0};

EXPR e_stack[MAXSTACK];
int o_stack[MAXSTACK];
int e_ptr, o_ptr;

int absexpr(EXPRPTR e)
{
    return (e->e_extcount == 0 && e->e_relcount == 0);
}

char seechar(void)
{
    if (strchr(SEPARATORS, *ptr) == (char *) NULL)
	return *ptr;
    return '\0';
}

char nextchar(void)
{
    if (strchr(SEPARATORS, *ptr) == (char *) NULL) {
      /* ++column; */ /* Do not calculate a column while in an expression */
	return *ptr++;
    }
    return '\0';
}

int getop(char c)
{
    char *p;

    if ((p = (char *)strchr(opstring, c)) == (char *) NULL)
	return 0;
    switch (c) {
      /* Test two characters operator */
    case '=':
      /* Must be followed by an other = */
      if (*ptr != '=')
	return 0;
      else {
	ptr++;
	return (p - operator);
      }
      break;
    case '<':
      /* Can be followed by a = or > */
      if (*ptr == '=') {
	ptr++;
	return (strchr(opstring, LESSEQUAL) - operator);
      }
      if (*ptr == '>') {
	ptr++;
	return (strchr(opstring, DIFFERENT) - operator);
      }
      return (p - operator);
      break;
    case '>':
      /* Can be followed by a = */
      if (*ptr == '=') {
	ptr++;
	return (strchr(opstring, MOREEQUAL) - operator);
      }
      else
	return (p - operator);
      break;
    default: /* Not a special character */
      return (p - operator);
    }
}

int pushop(char c)
{
  if (++o_ptr >= MAXSTACK) {
    errmsg("Operator stack overflow");
    --o_ptr;
    return FALSE;
  }
  o_stack[o_ptr] = strchr(operator, c) - operator;
  return TRUE;
}
    
int seeop(void)
{
    if (o_ptr < 0) {
	errmsg("Operator stack underflow");
	return 0;
    }
    return o_stack[o_ptr];
}

int popop(void)
{
  if (o_ptr < 0) {
    errmsg("Operator stack underflow");
    return 0;
  }
  return o_stack[o_ptr--];
}
    
int pushexpr(EXPRPTR e)
{
    if (++e_ptr >= MAXSTACK) {
	errmsg("Expression stack overflow");
	--e_ptr;
	return FALSE;
    }
    e_stack[e_ptr] = *e;
    return TRUE;
}
    
EXPRPTR popexpr(void)
{
    if (e_ptr < 0) {
	errmsg("Expression stack underflow");
	return (EXPRPTR) NULL;
    }
    return &e_stack[e_ptr--];
}

int getsymbol(void)
{
  int i = 0;
  int equals, local;
  char c;
  SYMBOLPTR sym;
  static EXPR exp;
  
  exp.e_value = 0;
  exp.e_extcount = exp.e_relcount = 0;
  exp.e_name[0] = '\0';
  
  equals = (seechar() == '=');
  local = (seechar() == ':');
  
  while ((c=nextchar()) != '\0') {
    if (c == ')') {
      /*  --column; */
      --ptr;
      break;
    }
    if (i < SYMBOL_LEN + (equals | local))
      exp.e_name[i++] = c;
    /* else clause here to error on label too long */
  }
  exp.e_name[i] = '\0';
  
  if (strcmp(exp.e_name, "*") == 0) {
    exp.e_name[0] = '\0';
    exp.e_value = pc;
    exp.e_relcount = !absolute;
    return (pushexpr(&exp));
  }
  if( !strcmp( exp.e_name, "--" ) && label_index_m2 > 0 )
    sprintf( exp.e_name, "lBm2%X", label_index_m2 - 1 );
  else if( !strcmp( exp.e_name, "-" ) && label_index_m1 > 0 )
    sprintf( exp.e_name, "lBm1%X", label_index_m1 - 1 );
  else if ( !strcmp( exp.e_name, "+" ) )
    sprintf( exp.e_name, "lBp1%X", label_index_p1 );
  else if ( !strcmp( exp.e_name, "++" ) )
    sprintf( exp.e_name, "lBp2%X", label_index_p2 );


  if ((sym = findsymbol(exp.e_name)) == (SYMBOLPTR) NULL) {
    /* Symbol is not in symbol table at all */
    if (!pass2) {
      exp.e_extcount = 1;
      errmsg("Undefined symbol");
      expr_unknown = TRUE;
      return (pushexpr(&exp));
    }
    /* This is pass 2, symbol is not in symbol table */
    if (!equals)
      errmsg("Undefined symbol");
    /* Need to add the symbol to the table (for line/fill reference) */
    sym = addsymbol(exp.e_name, (i_32) 0, 0);
  } else {
    /* Symbol is in symbol table */
    i = sym->s_flags & (S_EXTERNAL | S_ENTRY | S_RDSYMB | S_RESOLVED);
    if (!equals && (i == S_EXTERNAL || i == 0))
      errmsg("Undefined symbol");
  }
  if ((sym->s_flags & S_RESOLVED) == 0)
    expr_resolved = FALSE;
  if (pass2) {
    if (equals && !(sym->s_flags & S_RESOLVED))
      sym->s_flags |= S_EXTERNAL;
    addlineref(sym);
  }
  exp.e_extcount = equals;
  if (exp.e_extcount == 0)
    exp.e_relcount = ((sym->s_flags & (S_RELOCATABLE | S_RESOLVED))
		      == (S_RELOCATABLE | S_RESOLVED));
  exp.e_value = sym->s_value;
  if (!exp.e_extcount)
    exp.e_name[0] = '\0';
  return (pushexpr(&exp));
}

int nextexpr(char c)
/* c is the first character of the expression {== *ptr} */
{
  static EXPR temp;
  char buffer[9];
  i_32 i, j, value = 0;
  
  temp.e_extcount = 0;
  temp.e_relcount = 0;
  temp.e_name[0] = '\0';
  
  switch (c) {
  case '#':
    (void) nextchar();	/* skip the '#' */
    j = 0;
    while (isxdigit(ptr[j]))
      ++j;
    
    if (j == 0 || j > 8) {
      if (j > 8)
	errmsg("Invalid HEX constant (too many digits)");
      else 
	errmsg("Invalid HEX constant (not HEX digit)");
      return FALSE;
    }
    for (i=0; i < j; i++)
      value = (value << 4) + (i_32)fromhex((char)toupper(ptr[i]));                /* wgg added cast (char)   */
    /*    column += j; */ /* Do not calculate the column while in an expression */
    ptr += j;
    break;
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    while ((c=seechar()) != '\0' && isdigit(c))
      value = value * 10 + (nextchar() & 0x7f) - '0';
    break;
  case '\\':
  case '\'':
  case '"':
    if (c == '\\' || quote_ok) { /* if quote_ok false, fall into default */
      if (!getasc(buffer, 8)) {
	if (strlen(buffer) == 8)
	  errmsg("Invalid ASC constant (too large)");
	else
	  errmsg("Invalid ASC constant");
	return FALSE;
      }
      reverse(buffer);
      breverse(buffer);
      j = strlen(buffer);
      for (i=0; i < j; i++)
	value = (value << 4) + fromhex(buffer[i]);
      break;
    }
    /* NOTE! If quote_ok is FALSE, '\'' case falls through to default */
  case '%':
    if (!no_bin) {
      nextchar();	/* Skip the percent */
      j = 0;
      while ( (c= seechar()) != '\0' && ( strchr("01",c)) ) {
	value *= 2;
	value += (nextchar() & 0x7F ) - '0';
	j++;
      }
      if (j == 0) {
	errmsg("Invalid BIN constant (not BIN digit)");
	return FALSE;
      }
      break;
    }
  default:
    return (getsymbol());
  }
  temp.e_value = value;
  return (pushexpr(&temp));
}

int doop(int op)
{
    unsigned char c = operator[op];
    int all_ok = TRUE;
    i_32 count;

    if (c == START)
	return TRUE;
    if (c == '(') {
	errmsg("Unmatched '(' in expression");
	return FALSE;
    }

    if ((right = popexpr()) == (EXPRPTR) NULL
		    || (left = popexpr()) == (EXPRPTR) NULL)
	return FALSE;

    switch (c) {
    case '+':
	left->e_value += right->e_value;
	left->e_relcount += right->e_relcount;
	if (left->e_extcount == 0)
	    (void) strcpy(left->e_name, right->e_name);
	else {
	    if (right->e_extcount != 0) {
		errmsg("Can't add two external expressions");
		all_ok = FALSE;
	    }
	}
	left->e_extcount += right->e_extcount;
	break;
    case '-':
	left->e_value -= right->e_value;
	left->e_relcount -= right->e_relcount;
	if (right->e_extcount != 0) {
	    errmsg("Can't have external reference on right side of -");
	    all_ok = FALSE;
	}
	break;
    case '~':
	if (left->e_extcount == 0 && left->e_relcount == 0) {
	    left->e_value *= 256;
	    left->e_value += right->e_value;
	    left->e_extcount = right->e_extcount;
	    left->e_relcount = right->e_relcount;
	    (void) strcpy(left->e_name, right->e_name);
	} else {
	    errmsg("Can't have external reference on left side of ~");
	    all_ok = FALSE;
	}
	break;
    case '!':
    case '&':
    case '*':
    case '/':
    case '%':
    case '^':
    case '<':
    case '>':
    case '=':
    case LESSEQUAL:
    case MOREEQUAL:
    case DIFFERENT:
	if (left->e_extcount != 0 || left->e_relcount != 0
		|| right->e_extcount != 0 || right->e_relcount != 0) {
	    errmsg("Can't use !,&,*,/,%,^,<,>,<=,>= or == with external reference");
	    all_ok = FALSE;
	    break;
	}
	switch (c) {
	case '!':
	    left->e_value |= right->e_value;
	    break;
	case '&':
	    left->e_value &= right->e_value;
	    break;
	case '*':
	    left->e_value *= right->e_value;
	    break;
	case '/':
	    if (right->e_value != 0)
		left->e_value /= right->e_value;
	    else {
		errmsg("Divide by zero (result set to 0)");
		left->e_value = 0;
	    }
	    break;
	case '%':
	    if (right->e_value != 0)
		left->e_value %= right->e_value;
	    else {
		errmsg("Modulo zero (result set to 0)");
		left->e_value = 0;
	    }
	    break;
	case '^':
	    count = right->e_value;
	    right->e_value = left->e_value;
	    if (count >= 0) {
		left->e_value = 1;
		while (count-- > 0)
		    left->e_value *= right->e_value;
	    } else {
		errmsg("Exponent less than zero (result set to 0)");
		left->e_value = 0;
	    }
	    break;
	case '<':
	  left->e_value = (left->e_value < right->e_value);
	  break;
	case '>':
	  left->e_value = (left->e_value > right->e_value);
	  break;
	case '=':
	  left->e_value = (left->e_value == right->e_value);
	  break;
	case LESSEQUAL:
	  left->e_value = (left->e_value <= right->e_value);
	  break;
	case MOREEQUAL:
	  left->e_value = (left->e_value >= right->e_value);
	  break;
	case DIFFERENT:
	  left->e_value = (left->e_value != right->e_value);
	  break;
	}
	break;
    default:
	/* Unknown operator */
	errmsg("Unrecognized operator");
	all_ok = FALSE;
	break;
    }
    return (all_ok && pushexpr(left));
}

EXPRPTR doexpr(void)
{
    int op, needexpr = TRUE;
    int result;
    char c;

    expr_resolved = FALSE;
    if (!isfield()) {
	errmsg("Missing expression");
	return (EXPRPTR) NULL;
    }
    e_ptr = o_ptr = -1;

    expr_resolved = TRUE;
    expr_unknown = FALSE;

    (void) pushop(START);
    while ((c = seechar()) != '\0') {
	if (needexpr) {
	    DBG2("doexpr: expecting expression, first character is '%c'", c);
	    if (c == '(') {
		DBG1("doexpr: got a left parenthesis");
		if (!pushop(nextchar())) {
		    expr_resolved = FALSE;
		    return (EXPRPTR) NULL;
		}
	    } else {
		DBG1("doexpr: evaluating an expression with nextexpr()");
		if (nextexpr(c))
		    needexpr = FALSE;
		else {
		    expr_resolved = FALSE;
		    return (EXPRPTR) NULL;
		}
	    }
	} else {
	    DBG2("doexpr: expecting operator, first character is '%c'", c);
	    if ((op=getop(nextchar())) != 0) {
	      c = operator[op];
		DBG1("doexpr: matched an operator; checking precedence rules");
		while (precedence[seeop()] >= precedence[op]) {
		    DBG2("doexpr: precedence causes evaluation of '%c'",
				operator[seeop()]);
		    if (!doop(popop())) {
			expr_resolved = FALSE;
			return (EXPRPTR) NULL;
		    }
		}
		if (c == ')') {
		    DBG1("doexpr: right parenthesis; evaluating operators");
		    while ((c = operator[seeop()]) != START
			    && c != '('
			    && (result = doop(popop())))
			;
		    if (c == START) {
			errmsg("Unmatched ')' in expression");
			expr_resolved = FALSE;
			return (EXPRPTR) NULL;
		    }

		    if (c == '(') {
			DBG1("doexpr: found matching left parenthesis");
			(void) popop();	/* throw away the left paren */
		    } else
			if (!result) {
			    expr_resolved = FALSE;
			    return (EXPRPTR) NULL;
			}
		} else {
		    DBG2("doexpr: got an operator '%c'", c);
		    if (pushop(c))
			needexpr = TRUE;
		    else {
			expr_resolved = FALSE;
			return (EXPRPTR) NULL;
		    }
		}
	    } else {
		DBG2("doexpr: unrecognized operator '%c'", operator[op]);
		errmsg("Unrecognized operator");
	    }
	}
	if (blanks_in_expr && (*ptr == ' ' || *ptr == '\t')) {
	    /* whitespace allowed and present */
	    DBG1("doexpr: skipping white space in expression");
	    skipwhite();
	    if (!isfield()) {
		/* The next non-white character is past the comment column */
		break;	/* break from the while() loop */
	    }
	}
    }
    op = getop(START);
    while (seeop() != op) {
	DBG2("doexpr: evaluating operator '%c'", operator[seeop()]);
	if (!doop(popop())) {
	    expr_resolved = FALSE;
	    return (EXPRPTR) NULL;
	}
    }
    if (e_ptr == 0 && o_ptr == 0 &&
		(e_stack[0].e_relcount + e_stack[0].e_extcount <= 1)) {
	if ((entry->op_flags & MACRO) == 0)
	    e_stack[0].e_value += entry->op_bias;
	return(&e_stack[0]);
    } else {
	if (e_ptr != 0)
	    errmsg("More data than operators in expression");
	if (o_ptr != 0)
	    errmsg("Operator left on stack");
	if (e_ptr == 0 && o_ptr == 0)
	    errmsg("Too many relocatable/external references");
	expr_resolved = FALSE;
	return (EXPRPTR) NULL;
    }
}
