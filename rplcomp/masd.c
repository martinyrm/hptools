
#ifdef  HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef  HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifndef macintosh
# include <malloc.h>
#endif
#include <ctype.h>

#include "rplcomp.h"
#include "envfile.h"

#ifdef dll
#include "dllfile.h"
#endif

int MASD_LongCteError= TRUE;

static char Masd_InstructionField[MAXBUFSIZE];
static char Masd_GlobalLabel[MAXBUFSIZE];
int Compilationflags = 0;

void  Masd_getline(void)
{
  char  *cp, ch;

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
  outputf("* File\t%s\t%d\n", infp->name, infp->line);
  dofields();
  if(commented)
    fprintf(outfp, "*- %s\n", line_buf);
  else
    if(*fields == NULL)
      putc('\n', outfp);
      }
    } else {
      strcpy(line_buf, infp->name);
      if(line_buf[0] != '\0')
  FREE(infp->name);
      infp--;
      outputf("* File\t%s\t%d\n", infp->name, infp->line);
      dofields();
    }
  } while(*fields == NULL);
}

char  *Masd_nextqfield(void)
{
  while((fields[fieldnumber] == NULL) || ('%'==fields[fieldnumber][0]))
    Masd_getline();
  return fields[fieldnumber++];
}

char  *Masd_just_show_token(void)
{
  while((fields[fieldnumber] == NULL) || ('%'==fields[fieldnumber][0]))
    Masd_getline();
  return fields[fieldnumber];
}

void  Masd_parse_str(char *token, unsigned char sep)
{ char *a = strstr(line_buf, token);
  if (NULL==a)

  RPLCompError(TRUE,"Can not find first token in string");
  else
  {
    a++;
    while ((a[0]!='\0') && (a[0]!='\n') && (a[0]!=sep))
      if ('\\'==a[0])
      {
        outputf("\tCON(2)\t#%c%c\n", a[1], a[2]);
        a++; a++; a++;
      } else
      {
        outputf("\tNIBASC\t\\%c\\\n", a[0]);
        a++;
      }
    if (a[0]!=sep)
      RPLCompError(TRUE,"Error in string");
    a++;
    strcpy(line_buf, a);
    dofields();
  }
  
}


void Masd_doSkip(char *text)
{
  if (0==strcmp("{", Masd_nextqfield()))
    output(text);
  else
    RPLCompError(TRUE, "{ expected");
}

int Masd_CompField(char *s, char *cmp)
     /* pseudo code: l = ! ou &
                Z = A, B, C ou D
                r = A ou B
                n = 0 ou 1
                x = 0, 1, 2, 3 ou 4
*/
{
  int i=0;

  if (strlen(s)<strlen(cmp)) return FALSE;
  for (i=0; cmp[i]!=0; i++)
    { if (cmp[i]=='l') { if (((s[i]!='!') && (s[i]!='&'))) return FALSE; }
    else if (cmp[i]=='Z') { if (!((s[i]=='A') || (s[i]=='B') || (s[i]=='C') || (s[i]=='D'))) return FALSE; }
    else if (cmp[i]=='r') { if (((s[i]!='A') && (s[i]!='C'))) return FALSE; }
    else if (cmp[i]=='n') { if (((s[i]!='0') && (s[i]!='1'))) return FALSE; }
    else if (cmp[i]=='x') { if (((s[i]!='0') && (s[i]!='1') && (s[i]!='2') && (s[i]!='3') && (s[i]!='4'))) return FALSE; }
    else if (cmp[i]!=s[i]) return FALSE;
    }
  return TRUE;
}

void Masd_ExtractField(char *token)
{
  char *str;
  str= strchr(token, '.');
  if (NULL==str) {
    str = Masd_nextqfield();
    strcpy(Masd_InstructionField, str);
  }
  else
    { str[0]='\0';
    strcpy(Masd_InstructionField, &(str[1]));
    }
}

void Masd_OutputLabel(int saveGlobal, char *label)
{
  if (0==strlen(label))
    RPLCompError(TRUE, "Label Expected");
  else
    if ('.'==label[0])
      outputf("%s%s\n", Masd_GlobalLabel, label);
    else if ('_'==label[0])
      outputf("%s\n", label);
    else
      { if (saveGlobal)
  strcpy(Masd_GlobalLabel, label);
      if (label[0] == '=') label++;
      outputf("=%s\n", label);
      }
}

void Masd_OutputLabel2(int saveGlobal, char *label)
{
  if (0==strlen(label))
    RPLCompError(TRUE, "Label Expected");
  else
    if ('.'==label[0])
      outputf("%s%s", Masd_GlobalLabel, label);
    else if ('_'==label[0])
      outputf("%s", label);
    else
      { if (saveGlobal)
  strcpy(Masd_GlobalLabel, label);
      if (label[0] == '=') label++;
      outputf("=%s", label);
      }
}

void Masd_OutputLabel3(int saveGlobal, char *label)
{
  if (0==strlen(label))
    RPLCompError(TRUE, "Label Expected");
  else
	  if (MASD_LongCteError && (22<=strlen(label))) /* as the EQU is tab alligned, max size=31-8=23- (1 chr pour le =)=22 */ 
	    RPLCompError(TRUE, "Label too long");
    if ('.'==label[0])
      outputf("%s%s", Masd_GlobalLabel, label);
    else if ('_'==label[0])
      outputf("%s", label);
    else
      { if (saveGlobal)
  strcpy(Masd_GlobalLabel, label);
      if (label[0] == '=') label++;
      outputf("=%s", label);
      }
}

void Masd_InvertTest(char *token, int b, int p)
{
  static char testinvert1[]="01=#";
  char *buf;

  static char testinvert2[]="10#=";
  if (!b) 
    RPLCompError(TRUE,"This test can not be inverted");
  else 
    { buf = strchr(testinvert1, token[p]);
    if (buf == NULL)
      { char tmp;
      if (('<'==token[p]) || ('>'==token[p]))
  { tmp= token[p] ^ 2;
  token[p]= '\0';
  if ('='==token[p+1])
    outputf("%s%c%s", token, tmp, &(token[p+2]));
  else
    outputf("%s%c=%s", token, tmp, &(token[p+1]));
  } else
    RPLCompError(TRUE, "A valid test was expected");
      } else
  { token[p]= testinvert2[buf - testinvert1];
  output(token);
  }
    }
}

void Masd_DoGoto(char *gototext, int endofgoto, char *token)
{
  outputf("\t%s\t", gototext);
  if ((int)strlen(token)!=endofgoto)
    Masd_OutputLabel(FALSE, &(token[endofgoto]));
  else
    Masd_OutputLabel(FALSE, Masd_nextqfield());
}

void Masd_DoGoin(char *gototext, char *token)
{
  outputf("\t%s(5)\t(", gototext);
  if ((int)strlen(token)!=5)
    Masd_OutputLabel2(FALSE, token+5);
  else
    Masd_OutputLabel2(FALSE, Masd_nextqfield());
  outputf(")-(*)\n");
}

void Masd_DoGoto_Test(char *gototext, int endofgoto, char *token, char *testtext, int b, int p)
{
  Masd_InvertTest(testtext, b, p);
  output("\tSKIPYES{\n");
  Masd_DoGoto(gototext, endofgoto, token);
  output("\t}\n");
}

void GoyesProcess(char *token, int b, int p)
{
  char *str;
  str= Masd_nextqfield();
  if (!strcmp(str,"RTNYES") || !strcmp(str,"RTY")) { outputf("%s\tRTNYES\n", token); }
  else if (!strcmp(str,"GOYES"))                     { output(token); Masd_DoGoto("GOYES", 5, str); } 
  else if (!strcmp(str,"->"))                        { output(token); Masd_DoGoto("GOYES", 2, str); } 
  else if (!strcmp(str,"ì"))                         { output(token); Masd_DoGoto("GOYES", 1, str); } 
  else if (!strcmp(str,"SKIPYES"))                   { output(token); Masd_doSkip("\tSKIPYES{\n"); }
  else if (!strcmp(str,"{"))                         { outputf("%s\tSKIPYES{\n", token); }
  else if (!strcmp(str,"ì{"))                        { Masd_InvertTest(token,b,p); output("\tSKIPYES{\n"); }
  else if (!strcmp(str,"->{"))                       { Masd_InvertTest(token,b,p); output("\tSKIPYES{\n"); }
  else if (!strcmp(str,"GOTOL"))                     { Masd_DoGoto_Test("GOLONG", 5, str, token, b, p); }
  else if (!strcmp(str,"GOLONG"))                    { Masd_DoGoto_Test("GOLONG", 6, str, token, b, p); }
  else if (!strcmp(str,"GOTO"))                      { Masd_DoGoto_Test("GOTO", 4, str, token, b, p); }
  else if (!strcmp(str,"GOVLNG"))                    { Masd_DoGoto_Test("GOVLNG", 6, str, token, b, p); }
  else if (!strcmp(str,"GOSUBL"))                    { Masd_DoGoto_Test("GOSUBL", 6, str, token, b, p); }
  else if (!strcmp(str,"GOSBVL"))                    { Masd_DoGoto_Test("GOSBVL", 6, str, token, b, p); }
  else if (!strcmp(str,"GOSUB"))                     { Masd_DoGoto_Test("GOSUB", 5, str, token, b, p); }
  else if (Masd_CompField(str,"EXIT"))               { outputf("%s\t%s\n", token, str); }
  else if (Masd_CompField(str,"UP"))                 { outputf("%s\t%s\n", token, str); } 
  else RPLCompError(TRUE,"Need something after a test");
}

void strcatchr(char *str, char chr)
{
  char tmp[2];
  tmp[0]= chr;
  tmp[1]= '\0';
  strcat(str, tmp);
}

void Masd_Translateexp(char *output, char **exp);

void Masd_Translateterm(char *output, char **exp)
{
  if ('#'==*exp[0])
  { (*exp)++;
    while ((*exp[0]>='0') && (*exp[0]<='9')) 
    { strcatchr(output, *exp[0]); 
      (*exp)++; 
    }
  } else if ('$'==*exp[0])
  { (*exp)++; 
    strcatchr(output, '#');
    while ((32<*exp[0]) && (NULL!=strchr("0123456789ABCDEF", *exp[0])))
    { strcatchr(output, *exp[0]); 
      (*exp)++;
    }
  } else if ('('==*exp[0])
  { (*exp)++;
    strcatchr(output, '(');
    Masd_Translateexp(output, exp);
    strcatchr(output, ')');
  } else if (('0'<=*exp[0]) && ('9'>=*exp[0]))
  { while (('0'<=*exp[0]) && ('9'>=*exp[0]))
    { strcatchr(output, *exp[0]);
      (*exp)++;
    }
  }
  else if ('='==*exp[0])
  { while ((32<*exp[0]) && (NULL==strchr("()+-*/""", *exp[0])))
    { strcatchr(output, *exp[0]);
      (*exp)++;
    }
  } else if ('"'==*exp[0])
  { strcatchr(output, '(');
    (*exp)++;
    while ((32<*exp[0]) && ('"'!=*exp[0]))
    { strcatchr(output, *exp[0]);
      (*exp)++;
    }
    (*exp)++;
    strcatchr(output, ')');
  } else if ('.'==*exp[0])
  { strcatchr(output, '(');
    strcat(output, Masd_GlobalLabel);
    while ((32<*exp[0]) && (NULL==strchr("()+-*/""", *exp[0])))
    { strcatchr(output, *exp[0]);
      (*exp)++;
    }
    strcatchr(output, ')');
  } else if ('_'==*exp[0])
  { strcatchr(output, '(');
    while ((32<*exp[0]) && (NULL==strchr("()+-*/""", *exp[0])))
    { strcatchr(output, *exp[0]);
      (*exp)++;
    }
    strcatchr(output, ')');
  } else
  { strcat(output, "(=");
    while ((32<*exp[0]) && (NULL==strchr("()+-*/""", *exp[0])))
    { strcatchr(output, *exp[0]);
      (*exp)++;
    }
    strcatchr(output, ')');
  }
}

void Masd_Translateexp(char *output, char **exp)
{
  char temp[MAXBUFSIZE];
  while (TRUE)
    { Masd_Translateterm(output, exp);
    if      ('+'==*(exp[0])) { strcat(output, "+"); (*exp)++; }
    else if ('-'==*(exp[0])) { strcat(output, "-"); (*exp)++; }
    else if ('*'==*(exp[0])) { sprintf(temp, "(%s)*", output); strcpy(output, temp); (*exp)++; }
    else if ('/'==*(exp[0])) { sprintf(temp, "(%s)/", output); strcpy(output, temp); (*exp)++; }
    else if (')'==*(exp[0])) { (*exp)++; break; }
    else if ('\0'==*(exp[0])) { break; }
    else { RPLCompError(TRUE, "Error in expression, at pos '%s'", *exp); break; };
    }
}

void Masd_Expressionprintf(char *output, char *exp, int FinalCR)
{
  if (0==strlen(exp))
    exp= Masd_nextqfield();
  Masd_Translateexp(output, &exp);
  if (FinalCR)
    strcat(output, "\n");
}

void Masd_outputExpression(char *str, int FinalCR)
{
  static char tmp[MAXBUFSIZE];
  tmp[0]= '\0';
  Masd_Expressionprintf(tmp, str, FinalCR);
  output(tmp);
}

int Masd_GetCompileFlagMask(char * token)
{
  Masd_ExtractField(token);
  if (1==strlen(Masd_InstructionField))
  { 
    if (('0'<=Masd_InstructionField[0]) && ('9'>=Masd_InstructionField[0]))
      return 1<<(Masd_InstructionField[0]-'0');
    else
      RPLCompError(TRUE, "Invalid Compilation Flag Number");
    return 0;
  }
  if ((2==strlen(Masd_InstructionField)) && ('1'==Masd_InstructionField[0]))
  { 
    if (('0'<=Masd_InstructionField[1]) && ('5'>=Masd_InstructionField[1]))
      return 1<<(Masd_InstructionField[1]-'0'+10);
    else
      RPLCompError(TRUE, "Invalid Compilation Flag Number");
    return 0;
  }
  RPLCompError(TRUE, "Invalid Compilation Flag Number");
  return 0;
}

void Masd_parseToken(char *inputtoken)
{ 
  static char token[MAXBUFSIZE];
  static char str[MAXBUFSIZE];

  /* preliminary tests */
  if (0==strlen(inputtoken))
    return;
  if (MAXBUFSIZE<strlen(inputtoken))
    { RPLCompError(TRUE, "Token too long");
    return;
    }
  
  /* comments */
  if ('%'==token[0])
    { Masd_getline();
    return;
    }
  
  /* then, there is no slide effects risks. I work in my own memeory */
  strcpy(token, inputtoken);
  
  /* C+P+1 instruction */
  if (0==strcmp(token, "C+P+1"))
    { output("\tC+P+1\n");
    return;
    }
  
  /* R+ <=> R=R+ 9or R- or R! or R& */
  if (Masd_CompField(token,"Z+") || Masd_CompField(token,"Z-") || Masd_CompField(token,"Zl"))
    { sprintf(str, "%c=%s", token[0], token);
    Masd_parseToken(str);
    return;
    }
  
  /* Dn+ <=> Dn=Dn+ (or Dn-) */
  if (Masd_CompField(token,"Dn+") || Masd_CompField(token,"Dn-"))
    { sprintf(str, "D%c=%s", token[1], token);
    Masd_parseToken(str);
    return;
    }
  
  /* Labels */
  if (Masd_CompField(token,"*"))
    { Masd_OutputLabel(TRUE, &(token[1]));
    return;
    }
  
  /* R=R+ or R=R- cte */
  if (Masd_CompField(token,"Z=Z+") || Masd_CompField(token,"Z=Z-"))
    { if (!(Masd_CompField(token,"Z=Z+Z")) && !(Masd_CompField(token,"Z=Z-Z")))
      { Masd_ExtractField(token);
      outputf("\t%c=%c%c\t", token[0], token[2], token[3]);
      Masd_outputExpression(&(token[4]), FALSE);
      outputf("\t%s\n", Masd_InstructionField);
      return;
      }
    }
  
  /* Dn=Dn+ Dn=Dn- cte */
  if (Masd_CompField(token,"Dn=Dn+") || Masd_CompField(token,"Dn=Dn-"))
    { if (token[1] != token[4]) { RPLCompError(TRUE,"Unknown Instruction"); return; }
    outputf("\tD%c%c\t", token[4], token[5]);
    Masd_outputExpression(&(token[6]), TRUE);
    return;
    }
  
  /* fild with . in R acces or SRB + problem with W token */
  if (Masd_CompField(token,"Z=Rx") || Masd_CompField(token,"Rx=Z") || Masd_CompField(token,"ZRxEX") ||
      Masd_CompField(token,"ZSRB"))
    { Masd_ExtractField(token);
    if (0==strcmp("W", Masd_InstructionField))
      outputf("\t%s\n", token);
    else
      outputf("\t%s.f\t%s\n", token, Masd_InstructionField);
    return;
    }

  /* Special testo for XSRC and XSLC */
  if ( Masd_CompField(token,"ZSLC") || Masd_CompField(token,"ZSRC"))
  { outputf("\t%s\n", token);
    return;
  }

  /* fild with . in logic, poke, peek, abit=, c=p, p=c, cpex, a=a+, a=a-, st=, math */
  if (!Masd_CompField(token,"ZSRC") && (Masd_CompField(token,"Z=Z") || Masd_CompField(token,"r=DATn") || Masd_CompField(token,"DATn=r") ||
      Masd_CompField(token,"ZSR") || Masd_CompField(token,"ZSL") || Masd_CompField(token,"Z=0") || 
      Masd_CompField(token,"Z=-Z") || Masd_CompField(token, "ZZEX")))
    { Masd_ExtractField(token);
    outputf("\t%s\t%s\n", token, Masd_InstructionField);
    return;
    }

  /* Special test for C=PC */
  if ( Masd_CompField(token,"C=PC"))
  { outputf("\t%s\n", token);
    return;
  }
  if (Masd_CompField(token,"rBIT=n") || Masd_CompField(token,"C=P") || Masd_CompField(token,"P=C") ||
      Masd_CompField(token,"CPEX") || Masd_CompField(token,"ST=n") || Masd_CompField(token, "HST=0"))
    { Masd_ExtractField(token);
    outputf("\t%s\t", token);
    Masd_outputExpression(Masd_InstructionField, TRUE);
    return;
    }

  /* D0= */
  if (Masd_CompField(token, "Dn=(2)") || Masd_CompField(token, "Dn=(4)") || Masd_CompField(token, "Dn=(5)"))
    { outputf("\tD%c=(%c)\t", token[1], token[4]);
    Masd_outputExpression(&(token[6]), TRUE);
    return;
    }

  if (Masd_CompField(token, "Dn="))
    { if ((5==strlen(token)) || (7==strlen(token)) || (8==strlen(token)))
      { outputf("\tD%c=(%d)\t%s\n", token[1], strlen(token)-3, &(token[3]));
      RPLCompError(FALSE, "Thou shall not use constants, it's BAD!");
      return;
      }
    }

  /* "normal" instructions */
  if (Masd_CompField(token,"RTN") ||   Masd_CompField(token,"SET") ||   Masd_CompField(token,"RSTK=C") ||
      Masd_CompField(token,"C=RSTK") ||Masd_CompField(token,"CLRST") || Masd_CompField(token,"C=ST") ||
      Masd_CompField(token,"ST=C") ||  Masd_CompField(token,"CSTEX") || Masd_CompField(token,"P=P+1") ||
      Masd_CompField(token,"P=P-1") || Masd_CompField(token,"RTI") ||   Masd_CompField(token,"Dn=r") ||
      Masd_CompField(token,"rDnEX") || Masd_CompField(token,"rDnXS") || Masd_CompField(token,"OUT=C") ||
      Masd_CompField(token,"r=IN") ||  Masd_CompField(token,"UNCNFG") ||Masd_CompField(token,"CONFIG") ||
      Masd_CompField(token,"C=ID") ||  Masd_CompField(token,"SHUTDN") ||Masd_CompField(token,"INTON") ||
      Masd_CompField(token,"RSI") ||   Masd_CompField(token,"BUSC") ||  Masd_CompField(token,"PC=(r)") ||
      Masd_CompField(token,"PC=r") ||  Masd_CompField(token,"r=PC") ||
      Masd_CompField(token,"INTOFF") ||Masd_CompField(token,"C=C+P+1") ||
      Masd_CompField(token,"RESET") || Masd_CompField(token,"SREQ?") || Masd_CompField(token,"rPCEX") ||
      Masd_CompField(token,"XM=0") ||  Masd_CompField(token,"SB=0") ||  Masd_CompField(token,"SR=0") ||
      Masd_CompField(token,"MP=0") ||  Masd_CompField(token,"CLRHST") || Masd_CompField(token,"P+1") ||
      Masd_CompField(token,"P-1"))
    { outputf("\t%s\n", token);
    return;
    }

  /* tests */
  if ('?'==token[0])
    { /* standard tests */
      if (Masd_CompField(token,"?Z=Z") || Masd_CompField(token,"?Z#Z") || Masd_CompField(token,"?Z<Z") ||
    Masd_CompField(token,"?Z>Z") || Masd_CompField(token,"?Z<=Z") || Masd_CompField(token,"?Z>=Z") ||
    Masd_CompField(token,"?Z=0") || Masd_CompField(token,"?Z#0"))
  { Masd_ExtractField(token);
  sprintf(str, "\t%s\t%s\n", token, Masd_InstructionField);
  GoyesProcess(str,TRUE,3); 
  return;
  }

      /* st tests */
      if (Masd_CompField(token,"?ST="))
  { Masd_ExtractField(token);
  sprintf(str, "\t%s\t", token);
  Masd_Expressionprintf(str, Masd_InstructionField, TRUE);
  GoyesProcess(str,TRUE,4); 
  return;
  }

      /* abit tests */
      if (Masd_CompField(token,"?ZBIT="))
  { Masd_ExtractField(token);
  sprintf(str, "\t%s\t", token);
  Masd_Expressionprintf(str, Masd_InstructionField, TRUE);
  GoyesProcess(str,TRUE,6); 
  return;
  }

      /* P tests */
      if (Masd_CompField(token,"?P=") || Masd_CompField(token,"?P#"))
  { sprintf(str, "\t?P%c\t%s\n", token[2], &(token[3]));
  GoyesProcess(str,TRUE,3);
  return;
  }

      /* SM, SB, SR, MP tests */
      if (Masd_CompField(token,"?XM=0") || Masd_CompField(token,"?MP=0") ||
    Masd_CompField(token,"?SB=0") || Masd_CompField(token,"?SR=0"))
  { sprintf(str, "\t%s\n", token);
  GoyesProcess(str,FALSE,0);
  return;
  }

      /* HST test */
      if (Masd_CompField(token,"?HST=0"))
  { Masd_ExtractField(token);
  sprintf(str, "\t%s\t", token);
  Masd_Expressionprintf(str, Masd_InstructionField, TRUE);
  GoyesProcess(str,FALSE,0); 
  return;
  }
    }

  /* GO instructions */
  if (Masd_CompField(token,"G"))
    { if (Masd_CompField(token,"GOTOL"))           { Masd_DoGoto("GOLONG", 5, token); return; }
    else if (Masd_CompField(token,"GOLONG"))     { Masd_DoGoto("GOLONG", 6, token); return; }
    else if (Masd_CompField(token,"GOTOC"))      { Masd_DoGoto("SKIPNC{\n\tGOTO", 5, token); output("\t}\n"); return; }
    else if (Masd_CompField(token,"GOTONC"))      { Masd_DoGoto("SKIPC{\n\tGOTO", 6, token); output("\t}\n"); return; }
    else if (Masd_CompField(token,"GOTO"))       { Masd_DoGoto("GOTO", 4, token); return; }
    else if (Masd_CompField(token,"GOVLNG"))     { Masd_DoGoto("GOVLNG", 6, token); return; }
    else if (Masd_CompField(token,"GOSUBL"))     { Masd_DoGoto("GOSUBL", 6, token); return; }
    else if (Masd_CompField(token,"GOSBVL"))     { Masd_DoGoto("GOSBVL", 6, token); return; }
    else if (Masd_CompField(token,"GOSUB"))      { Masd_DoGoto("GOSUB", 5, token); return; }
    else if (Masd_CompField(token,"GOC"))        { Masd_DoGoto("GOC", 3, token); return; }
    else if (Masd_CompField(token,"GONC"))       { Masd_DoGoto("GONC", 4, token); return; }
    else if (Masd_CompField(token,"G2"))         { Masd_DoGoto("REL(2)", 2, token); return; }
    else if (Masd_CompField(token,"G3"))         { Masd_DoGoto("REL(3)", 2, token); return; }
    else if (Masd_CompField(token,"G4"))         { Masd_DoGoto("REL(4)", 2, token); return; }
    else if (Masd_CompField(token,"G5"))         { Masd_DoGoto("REL(5)", 2, token); return; }
    else if (Masd_CompField(token,"GOIN2"))      { Masd_DoGoto("REL(2)", 5, token); return; }
    else if (Masd_CompField(token,"GOIN3"))      { Masd_DoGoto("REL(3)", 5, token); return; }
    else if (Masd_CompField(token,"GOIN4"))      { Masd_DoGoto("REL(4)", 5, token); return; }
    else if (Masd_CompField(token,"GOIN5"))      { Masd_DoGoto("REL(5)", 5, token); return; }
    else if (Masd_CompField(token,"GOINC"))      { Masd_DoGoin("LC", token); return; }
    else if (Masd_CompField(token,"GOINA"))      { Masd_DoGoin("LA", token); return; }
    }

  /* skips */
  if (Masd_CompField(token,"EXIT"))               { outputf("\t%s\n", token); return; }
  if (Masd_CompField(token,"UP"))                 { outputf("\t%s\n", token); return; } 
  if (Masd_CompField(token, "SK"))
    { if (!strcmp(token, "SKIPL"))       { Masd_doSkip("\tSKIPL{\n"); return; }
    else if (!strcmp(token, "SKUBL"))  { Masd_doSkip("\tSKUBL{\n"); return; }
    else if (!strcmp(token, "SKUB"))   { Masd_doSkip("\tSKUB{\n"); return; } 
    else if (!strcmp(token, "SKNC"))   { Masd_doSkip("\tSKIPNC{\n"); return; }
    else if (!strcmp(token, "SKC"))    { Masd_doSkip("\tSKIPC{\n"); return; }
    else if (!strcmp(token, "SKIPNC")) { Masd_doSkip("\tSKIPNC{\n"); return; }
    else if (!strcmp(token, "SKIPC"))  { Masd_doSkip("\tSKIPC{\n"); return; }
    else if (!strcmp(token, "SKIP"))   { Masd_doSkip("\tSKIP{\n"); return; }
    }
  if (0==strcmp(token, "{")) { output("\t{\n"); return; }
  if (0==strcmp(token, "}"))
    { char *tmp;
    tmp= Masd_just_show_token();
    if (0==strcmp(tmp, "SKELSE"))     { Masd_nextqfield(); Masd_doSkip("\t}SKELSE{\n"); return; }
    else if (0==strcmp(tmp, "SKEC"))  { Masd_nextqfield(); Masd_doSkip("\t}SKEC{\n"); return; }
    else if (0==strcmp(tmp, "SKENC")) { Masd_nextqfield(); Masd_doSkip("\t}SKENC{\n"); return; }
    else if (0==strcmp(tmp, "SKLSE")) { Masd_nextqfield(); Masd_doSkip("\t}SKLSE{\n"); return; }
    else 
      { output("\t}\n");
      return;
      }
    }
  if (0==strcmp(token, "STROBJ"))
    { output("\tCON(5)\t");
    Masd_outputExpression("", TRUE);
    Masd_doSkip("\tSKIP5{\n");
    return;
    }
  if (0==strcmp(token, "STRING"))
    { output("\tCON(5)\t=DOCSTR\n");
    Masd_doSkip("\tSKIP5{\n");
    return;
    }

  /* $ and EXP( */
  if (Masd_CompField(token, "EXP("))
  { sprintf(str, "$%s", &(token[3]));
    strcpy(token, str);
  }
  if ('$'==token[0])
  { if (1>=strlen(token))
    { RPLCompError(FALSE, "A $ should not be alone like this");
      return;
    }
    if ('/'==token[1])
    { int i;
      int len;
      len= strlen(token);
      for (i=len-1; i>1; i--)
        outputf("\tNIBHEX\t%c\n", token[i]);
      return;
    }
    if ('('==token[1])
    { int i;
      int j;
      for (i=2; i<(int)strlen(token); i++)
    if (')'==token[i])
    { j= i+1;
      token[i]= '\0';
    }
      outputf("\tCON(%s)\t", &(token[2]));
      Masd_outputExpression(&(token[j]), TRUE);
      return;
    }
    { int i;  
      int len;
      len= strlen(token);
      for (i=1; i<len; i++)
        outputf("\tNIBHEX\t%c\n", token[i]);
      return;
    }
  }


  /* P= instruction */
  if (Masd_CompField(token,"P="))
    { outputf("\tP=\t%s\n", &(token[2]));
    return;
    }

  /* standard masd macros */
  if (0==strcmp(token, "SAVE"))    { output("\tGOSBVL\t=SAVPTR\n"); return; }
  if (0==strcmp(token, "LOAD"))    { output("\tGOSBVL\t=GETPTR\n"); return; }
  if (0==strcmp(token, "RPL"))     { output("\tA=DAT0\tA\n\tD0=D0+\t5\n\tPC=(A)\n"); return; }
  if (0==strcmp(token, "LOADRPL")) { output("\tGOSBVL\t=GETPTRLOOP\n"); return; }
  if (0==strcmp(token, "~"))       { output("\tCON(5)\t='\n"); }
  if (162 == (unsigned char) token[0])  { Masd_parse_str(token, (char)162); return; }
  if ('"' == (unsigned char) token[0])  { Masd_parse_str(token, '"'); return; }

  /* LC, LA and friends */
  if(Masd_CompField(token, "LC(") || Masd_CompField(token, "LA("))
  { int i;
    int j=-1;
    for (i=3; i<(int)strlen(token); i++)
      if (')'==token[i])
	  { j= i+1;
        token[i]= '\0';
	  }

	  if (-1==j) 
        RPLCompError(TRUE, "Error in instruction %s", token);

      if (0==strlen(&(token[j])))
      { outputf("\t%s)\t", token);
        Masd_outputExpression("", TRUE);
      }
      else  
      { outputf("\t%s)\t", token);
        Masd_outputExpression(&(token[j]), TRUE);
      }
      return;
    }

  if((0==strcmp(token, "LC")) || (0==strcmp(token, "LA")))
    { outputf("\t%sHEX\t%s\n", token, Masd_nextqfield());
    return;
    }

  /* DC, DCCP, CP= */
  if (0==strcmp(token, "DCE"))
  { Masd_OutputLabel3(FALSE, Masd_nextqfield());
    output("\tEQU\t");
    Masd_outputExpression(Masd_nextqfield(), TRUE);
    return;
  }

  if (0==strcmp(token, "DC"))
  { Masd_OutputLabel3(FALSE, Masd_nextqfield());
    output("\tEQU\t#");
    outputf("%s\n", Masd_nextqfield());
    return;
  }

  if (0==strcmp(token, "DCCP"))
  {
    strcpy(str, Masd_nextqfield());
    Masd_OutputLabel3(FALSE, Masd_nextqfield());
    outputf("\tALLOC\t%s\n", str);
    return;
  }

  if (Masd_CompField(token, "CP="))
  {
    outputf("\tABASE\t#%s\n", &(token[3]));
    return;
  }

   /* Set compilation flags */
  if (Masd_CompField(token, "!FL=n"))
  { if ('0'==token[4])
      Compilationflags= Compilationflags & !Masd_GetCompileFlagMask(token);
    else
      Compilationflags= Compilationflags | Masd_GetCompileFlagMask(token);
    return;
  }

  if (Masd_CompField(token, "!?FL=n"))
  { 
    if ('0'==token[5])
    {
      if (0!=(Compilationflags & Masd_GetCompileFlagMask(token)))
        Masd_getline();
    }
    else 
    { if (0==(Compilationflags & Masd_GetCompileFlagMask(token)))
        Masd_getline();
    }
    return;
  }
  if (!strcmp(token,"INCLOB")) {
    char *buf;
    outputf("\t%s\t",token);
    buf = Masd_nextqfield();
    outputf("%s\n",buf);
    return;
  }
  if ('/' == *token) {
    outputf("\t%s\n",token);
    return;
  }
  /* else */
  RPLCompError(TRUE, "Unknown instruction %s", token);
}

void doassemblem(char *token)
{
  while (TRUE)
    { token= Masd_nextqfield();
    if (!strcmp(token, "!RPL"))  break;
    if (!strcmp(token, "ENDCODE")) 
	{ RPLCompError(TRUE, "Unmatched directive: ENDCODE"); break; }
    Masd_parseToken(token);
    }
}

void docodem(char *token)
{
  outputf("\tCON(5)\t=DOCODE\n\tREL(5)\tLBL%03d\n", label_cnt);
  while (TRUE)
    { token= Masd_nextqfield();
    if (!strcmp(token, "!RPL"))    { RPLCompError(TRUE, "Unmatched directive: !RPL"); break; }
    if (!strcmp(token, "ENDCODE")) { outputf("LBL%03d\n", label_cnt++); break; }
    Masd_parseToken(token);
    }
}

