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


static void usage(const char *argv0){
  fprintf(stderr, "usage: %s DATABASE\n", argv0);
  exit(1);
}

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


int main(int argc, char **argv){
  xjd1 *pDb;
  int rc, n;
  char *zStmt = 0;
  int nStmt = 0;
  int nAlloc = 0;
  int isTTY = 1;
  int parserTrace;
  int testJson;
  const char *zPrompt = "xjd1> ";
  char zLine[2000];

  parserTrace = find_option(argv, &argc, "trace", 0, 0)!=0;
  testJson = find_option(argv, &argc, "json", 0, 0)!=0;
  if( argc!=2 ) usage(argv[0]);
  rc = xjd1_open(0, argv[1], &pDb);
  if( rc!=XJD1_OK ){
    fprintf(stderr, "cannot open \"%s\": %s\n", argv[1], xjd1_errmsg(pDb));
    exit(1);
  }
  xjd1_config(pDb, XJD1_CONFIG_PARSERTRACE, parserTrace);
  isTTY = isatty(0);
  while( !feof(stdin) ){
    if( isTTY ) printf("%s", zPrompt);
    if( fgets(zLine, sizeof(zLine), stdin)==0 ) break;
    if( testJson ){
      JsonNode *pNode = xjd1JsonParse(zLine);
      if( pNode ){
        String out;
        xjd1StringInit(&out, 0, 0);
        xjd1JsonRender(&out, pNode);
        xjd1JsonFree(pNode);
        printf("%s\n", xjd1StringText(&out));
        xjd1StringClear(&out);
      }
      continue;
    }
    n = strlen(zLine);
    if( n+nStmt+1 >= nAlloc ){
      nAlloc = nAlloc + n + nStmt + 1000;
      zStmt = realloc(zStmt, nAlloc);
      if( zStmt==0 ){
        fprintf(stderr, "out of memory\n");
        exit(1);
      }
    }
    memcpy(&zStmt[nStmt], zLine, n+1);
    nStmt += n;
    if( xjd1_complete(zStmt) ){
      xjd1_stmt *pStmt;
      rc = xjd1_stmt_new(pDb, zStmt, &pStmt, &n);
      if( rc!=XJD1_OK ){
        fprintf(stderr, "ERROR: %s\n", xjd1_errmsg(pDb));
      }else{
        char *zTrace = xjd1_stmt_debug_listing(pStmt);
        if( zTrace ) printf("%s", zTrace);
        free(zTrace);
        while( xjd1_stmt_step(pStmt)==XJD1_ROW ){
          const char *zVal;
          xjd1_stmt_value(pStmt, &zVal);
          printf("%s\n", zVal);
        }
      }
      xjd1_stmt_delete(pStmt);
      zPrompt = "xjd1> ";
      nStmt = 0;
    }else{
      zPrompt = " ...> ";
    }
  }
  xjd1_close(pDb);
  return 0;
}
