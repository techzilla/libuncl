/*
** 2011 June 10
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
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
