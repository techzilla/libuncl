/*
** Copyright (c) 2011 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the Simplified BSD License (also
** known as the "2-Clause License" or "FreeBSD License".)
**
** This program is distributed in the hope that it will be useful,
** but without any warranty; without even the implied warranty of
** merchantability or fitness for a particular purpose.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*************************************************************************
** This file contains C code that implements a simple command-line
** wrapper shell around xjd1.
*/
#include "xjd1Int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>


/*
** Remove n elements from argv beginning with the i-th element.
*/
void remove_from_argv(char **argv, int *pArgc, int i, int n){
  int j;
  int argc = *pArgc;
  for(j=i+n; j<argc; i++, j++){
    argv[i] = argv[j];
  }
  *pArgc = i;
}


/*
** Look for a command-line option.  If present, return a pointer.
** Return NULL if missing.
**
** hasArg==0 means the option is a flag.  It is either present or not.
** hasArg==1 means the option has an argument.  Return a pointer to the
** argument.
*/
const char *find_option(
  char **argv,
  int *pArgc,
  const char *zLong,
  const char *zShort,
  int hasArg
){
  int i;
  int nLong;
  const char *zReturn = 0;
  int argc = *pArgc;
  assert( hasArg==0 || hasArg==1 );
  nLong = strlen(zLong);
  for(i=1; i<argc; i++){
    char *z;
    if (i+hasArg >= argc) break;
    z = argv[i];
    if( z[0]!='-' ) continue;
    z++;
    if( z[0]=='-' ){
      if( z[1]==0 ){
        remove_from_argv(argv, &argc, i, 1);
        break;
      }
      z++;
    }
    if( strncmp(z,zLong,nLong)==0 ){
      if( hasArg && z[nLong]=='=' ){
        zReturn = &z[nLong+1];
        remove_from_argv(argv, &argc, i, 1);
        break;
      }else if( z[nLong]==0 ){
        zReturn = argv[i+hasArg];
        remove_from_argv(argv, &argc, i, 1+hasArg);
        break;
      }
    }else if( zShort && strcmp(z,zShort)==0 ){
      zReturn = argv[i+hasArg];
      remove_from_argv(argv, &argc, i, 1+hasArg);
      break;
    }
  }
  *pArgc = argc;
  return zReturn;
}

/*
** State information
*/
typedef struct Shell Shell;
struct Shell {
  xjd1 *pDb;           /* Open database connection */
  const char *zFile;   /* Current filename */
  int nLine;           /* Current line number */
  FILE *pIn;           /* Input file */
  int isTTY;           /* True if pIn is a TTY */
  int isTest;          /* True if the --test flag is used */
  int parseTrace;      /* True if the --parser-trace flag is used */
  String testOut;      /* Output from a test case */
  char zModule[50];    /* Mame of current test module */
  char zTestCase[50];  /* Name of current testcase */
  String inBuf;        /* Buffered input text */
  int nCase;           /* Number of --testcase commands seen */
  int nTest;           /* Number of tests performed */
  int nErr;            /* Number of test errors */
};

/*
** True for space characters.
*/
static int shellIsSpace(char c){
  return c==' ' || c=='\t' || c=='\r' || c=='\n';
}

/*
** Command:  .quit
*/
static void shellQuit(Shell *p, int argc, char **argv){
  exit(0);
}

/*
** Command:  .testcase NAME
*/
static void shellTestcase(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    int n = strlen(argv[1]);
    if( n>=sizeof(p->zTestCase) ) n = sizeof(p->zTestCase)-1;
    memcpy(p->zTestCase, argv[1], n);
    p->zTestCase[n] = 0;
    xjd1StringTruncate(&p->testOut);
  }
}

/*
** Return non-zero if string z matches glob pattern zGlob and zero if the
** pattern does not match.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
**      '#'       Matches any sequence of one or more digits with an
**                optional + or - sign in front
*/
static int strglob(const char *zGlob, const char *z){
  int c, c2;
  int invert;
  int seen;

  while( (c = (*(zGlob++)))!=0 ){
    if( c=='*' ){
      while( (c=(*(zGlob++))) == '*' || c=='?' ){
        if( c=='?' && (*(z++))==0 ) return 0;
      }
      if( c==0 ){
        return 1;
      }else if( c=='[' ){
        while( *z && strglob(zGlob-1,z)==0 ){
          z++;
        }
        return (*z)!=0;
      }
      while( (c2 = (*(z++)))!=0 ){
        while( c2!=c ){
          c2 = *(z++);
          if( c2==0 ) return 0;
        }
        if( strglob(zGlob,z) ) return 1;
      }
      return 0;
    }else if( c=='?' ){
      if( (*(z++))==0 ) return 0;
    }else if( c=='[' ){
      int prior_c = 0;
      seen = 0;
      invert = 0;
      c = *(z++);
      if( c==0 ) return 0;
      c2 = *(zGlob++);
      if( c2=='^' ){
        invert = 1;
        c2 = *(zGlob++);
      }
      if( c2==']' ){
        if( c==']' ) seen = 1;
        c2 = *(zGlob++);
      }
      while( c2 && c2!=']' ){
        if( c2=='-' && zGlob[0]!=']' && zGlob[0]!=0 && prior_c>0 ){
          c2 = *(zGlob++);
          if( c>=prior_c && c<=c2 ) seen = 1;
          prior_c = 0;
        }else{
          if( c==c2 ){
            seen = 1;
          }
          prior_c = c2;
        }
        c2 = *(zGlob++);
      }
      if( c2==0 || (seen ^ invert)==0 ) return 0;
    }else if( c=='#' ){
      if( (z[0]=='-' || z[0]=='+') && (z[1]>='0' && z[1]<='9') ) z++;
      if( z[0]<'0' || z[0]>'9' ) return 0;
      z++;
      while( z[0]>='0' && z[0]<='9' ){ z++; }
    }else{
      if( c!=(*(z++)) ) return 0;
    }
  }
  return *z==0;
}

/*
** Command:  .result PATTERN
*/
static void shellResult(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    char *zRes = xjd1StringText(&p->testOut);
    if( zRes==0 ) zRes = "";
    p->nTest++;
    if( strcmp(argv[1], zRes) ){
      p->nErr++;
      fprintf(stderr, " Test failed: %s\n Expected: [%s]\n      Got: [%s]\n",
        p->zTestCase, argv[1], zRes);
    }
  }
}
/*
** Command:  .glob PATTERN
*/
static void shellGlob(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    char *zRes = xjd1StringText(&p->testOut);
    if( zRes==0 ) zRes = "";
    p->nTest++;
    if( strglob(argv[1], zRes) ){
      p->nErr++;
      fprintf(stderr, " Test failed: %s\n Expected: [%s]\n      Got: [%s]\n",
        p->zTestCase, argv[1], zRes);
    }
  }
}
/*
** Command:  .notglob PATTERN
*/
static void shellNotGlob(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    char *zRes = xjd1StringText(&p->testOut);
    if( zRes==0 ) zRes = "";
    p->nTest++;
    if( strglob(argv[1], zRes)==0 ){
      p->nErr++;
      fprintf(stderr, " Test failed: %s\n Expected: [%s]\n      Got: [%s]\n",
        p->zTestCase, argv[1], zRes);
    }
  }
}

/*
** Command:  .open FILENAME
*/
static void shellOpenDB(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    int rc;
    if( p->pDb ) xjd1_close(p->pDb);
    rc = xjd1_open(0, argv[1], &p->pDb);
    if( rc!=XJD1_OK ){
      fprintf(stderr, "%s:%d: cannot open \"%s\"\n",
              p->zFile, p->nLine, argv[1]);
      p->nErr++;
      p->pDb = 0;
    }
  }    
}
/*
** Command:  .new FILENAME
*/
static void shellNewDB(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    if( p->pDb ) xjd1_close(p->pDb);
    p->pDb = 0;
    unlink(argv[1]);
    shellOpenDB(p, argc, argv);
  }
}

/* Forward declaration */
static void processOneFile(Shell*, const char*);

/*
** Command:  .read FILENAME
** Read and process text from a file.
*/
static void shellRead(Shell *p, int argc, char **argv){
  if( argc>=2 ){
    processOneFile(p, argv[1]);
  }
}

/*
** Process a command intended for this shell - not for the database.
*/
static void processMetaCommand(Shell *p){
  char *z;
  int n;
  char *azArg[2];
  int nArg;
  int i;
  static const struct shcmd {
    const char *zCmd;                   /* Name of the command */
    void (*xCmd)(Shell*,int,char**);    /* Procedure to run the command */
    const char *zHelp;                  /* Help string */
  } cmds[] = {
    { "quit",     shellQuit,      ".quit"               },
    { "testcase", shellTestcase,  ".testcase NAME"      },
    { "result",   shellResult,    ".result TEXT"        },
    { "glob",     shellGlob,      ".glob PATTERN"       },
    { "notglob",  shellNotGlob,   ".notglob PATTERN"    },
    { "read",     shellRead,      ".read FILENAME"      },
    { "open",     shellOpenDB,    ".open DATABASE"      },
    { "new",      shellNewDB,     ".new DATABASE"       },
  };

  /* Remove trailing whitespace from the command */
  z = xjd1StringText(&p->inBuf);
  n = xjd1StringLen(&p->inBuf);
  while( n>0 && shellIsSpace(z[n-1]) ){ n--; }
  z[n] = 0;

  /* Find the command name text and its argument (if any) */
  z++;
  azArg[0] = z;
  for(i=0; z[i] && !shellIsSpace(z[i]); i++){}
  if( z[i] ){
    z[i] = 0;
    z += i+1;
    while( shellIsSpace(z[0]) ) z++;
    azArg[1] = z;
    nArg = 2;
  }else{
    azArg[1] = 0;
    nArg = 1;
  }

  /* Find the command */
  for(i=0; i<sizeof(cmds)/sizeof(cmds[0]); i++){
    if( strcmp(azArg[0], cmds[i].zCmd)==0 ){
      cmds[i].xCmd(p, nArg, azArg);
      break;
    }
  }
  if( i>=sizeof(cmds)/sizeof(cmds[0]) ){
    p->nErr++;
    fprintf(stderr, "%s:%d: unknown command \".%s\"\n", 
            p->zFile, p->nLine, azArg[0]);
  }
  xjd1StringTruncate(&p->inBuf);
}

/*
** Run a single statment.
*/
static void processOneStatement(Shell *p, const char *zCmd){
  xjd1_stmt *pStmt;
  int N, rc;
  rc = xjd1_stmt_new(p->pDb, zCmd, &pStmt, &N);
  if( rc==XJD1_OK ){
    do{
      rc = xjd1_stmt_step(pStmt);
      if( rc==XJD1_ROW ){
        const char *zValue;
        xjd1_stmt_value(pStmt, &zValue);
        if( p->zTestCase[0]==0 ){
          printf("%s\n", zValue);
        }else{
          if( xjd1StringLen(&p->testOut)>0 ){
            xjd1StringAppend(&p->testOut, " ", 1);
          }
          xjd1StringAppend(&p->testOut, zValue, -1);
        }
      }
    }while( rc==XJD1_ROW );
    xjd1_stmt_delete(pStmt);
  }else{
    fprintf(stderr, "%s:%d: ERROR: %s\n",
            p->zFile, p->nLine, xjd1_errmsg(p->pDb));
    p->nErr++;
  }
}

/*
** Process one or more database commands
*/
static void processScript(Shell *p){
  char *z, c;
  int i;
  if( p->pDb==0 ) return;
  while( xjd1StringLen(&p->inBuf) ){
    z = xjd1StringText(&p->inBuf);
    for(i=0; z[i]; i++){
      if( z[i]!=';' ) continue;
      c = z[i+1];
      z[i+1] = 0;
      if( !xjd1_complete(z) ){
        z[i+1] = c;
        continue;
      }
      processOneStatement(p, z);
      z[i+1] = c;
      while( shellIsSpace(z[i+1]) ){ i++; }
      xjd1StringRemovePrefix(&p->inBuf, i+1);
      break;
    }
  }
}

/*
** Process a single file of input by reading the text of the file and
** sending statements into the XJD1 database connection.
*/
static void processOneFile(
  Shell *p,               /* Current state of the shell */
  const char *zFile       /* Name of file to process.  "-" for stdin. */
){
  Shell savedState;
  char *zIn;
  char zLine[1000];

  savedState = *p;
  if( strcmp(zFile, "-")==0 ){
    p->pIn = stdin;
    p->isTTY = 1;
  }else{
    p->pIn = fopen(zFile, "r");
    if( p->pIn==0 ){
      fprintf(stderr, "%s:%d: cannot open \"%s\"\n", p->zFile, p->nLine, zFile);
      p->nErr++;
      return;
    }
    p->isTTY = 0;
  }
  p->zFile = zFile;
  p->nLine = 0;
  while( !feof(p->pIn) ){
    if( p->isTTY ){
      printf("%s", xjd1StringLen(&p->inBuf)>0 ? " ...> " : "xjd1> ");
    }
    if( fgets(zLine, sizeof(zLine), p->pIn)==0 ) break;
    p->nLine++;
    xjd1StringAppend(&p->inBuf, zLine, -1);
    zIn = xjd1StringText(&p->inBuf);
    if( zIn[0]=='.' ){
      processMetaCommand(p);
    }else{
      processScript(p);
    }
  }
  if( p->isTTY==0 ){
    fclose(p->pIn);
  }
  p->zFile = savedState.zFile;
  p->nLine = savedState.nLine;
  p->pIn = savedState.pIn;
  p->isTTY = savedState.isTTY;
}


int main(int argc, char **argv){
  int i;
  Shell s;

  memset(&s, 0, sizeof(s));
  if( argc>1 ){
    for(i=1; i<argc; i++){
      processOneFile(&s, argv[i]);
    }
  }else{
    processOneFile(&s, "-");
  }
  if( s.pDb ) xjd1_close(s.pDb);
  xjd1StringClear(&s.inBuf);
  xjd1StringClear(&s.testOut);
  return 0;
}
