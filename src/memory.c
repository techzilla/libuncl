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
** Pooled memory allocation
*/
#include "xjd1Int.h"

/*
** Create a new memory allocation pool.  Return a pointer to the
** memory pool or NULL on OOM error.
*/
Pool *xjd1PoolNew(void){
  Pool *p = malloc( sizeof(*p) );
  if( p ) memset(p, 0, sizeof(*p));
  return p;
}

/*
** Clear a memory allocation pool.  That is to say, free all the
** memory allocations associated with the pool, though do not free
** the memory pool itself.
*/
void xjd1PoolClear(Pool *p){
  PoolChunk *pChunk, *pNext;
  for(pChunk = p->pChunk; pChunk; pChunk = pNext){
    pNext = pChunk->pNext;
    free(pChunk);
  }
  memset(p, 0, sizeof(*p));
}

/*
** Allocate N bytes of memory from the memory allocation pool.
*/
#define POOL_CHUNK_SIZE 3000
void *xjd1PoolMalloc(Pool *p, int N){
  N = (N+7)&~7;
  if( N>POOL_CHUNK_SIZE/4 ){
    PoolChunk *pChunk = malloc( N + 8 );
    if( pChunk==0 ) return 0;
    pChunk->pNext = p->pChunk;
    p->pChunk = pChunk;
    return &((char*)pChunk)[8];
  }else{
    void *x;
    if( p->nSpace<N ){
      PoolChunk *pChunk = malloc( POOL_CHUNK_SIZE + 8 );
      if( pChunk==0 ) return 0;
      pChunk->pNext = p->pChunk;
      p->pChunk = pChunk;
      p->pSpace = (char*)pChunk;
      p->pSpace += 8;
      p->nSpace = POOL_CHUNK_SIZE;
    }
    x = p->pSpace;
    p->pSpace += N;
    p->nSpace -= N;
    return x;
  }
}
void *xjd1PoolMallocZero(Pool *p, int N){
  void *x = xjd1PoolMalloc(p,N);
  if( x ) memset(x, 0, (N+7)&~7);
  return x;
}

/*
** Create a duplicate of a string using a memory pool.  Or if pPool==0
** obtain the memory for the duplicate from malloc().
*/
char *xjd1PoolDup(Pool *pPool, const char *z, int n){
  char *zOut;
  if( n<0 ) n = xjd1Strlen30(z);
  if( pPool ){
    zOut = xjd1PoolMalloc(pPool, n+1);
  }else{
    zOut = malloc( n+1 );
  }
  if( zOut ){
     memcpy(zOut, z, n);
     zOut[n] = 0;
  }
  return zOut;
}