/*
** 2011 June 09
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Interface definitions for XJD1
*/
#ifndef _XJD1_H
#define _XJD1_H

/* Error codes */
#define XJD1_OK       0      /* No error */
#define XDJ1_ERROR    1      /* Generic error */


/* Execution context */
typedef struct xjd1_context xjd1_context;

/* A database connection */
typedef struct xjd1 xjd1;

/* A prepared statement */
typedef struct xjd1_stmt xjd1_stmt;

/* Create, setup, and destroy an execution context */
int xjd1_context_new(xjd1_context**);
int xjd1_context_config(xjd1_context*, int, ...);
int xjd1_context_delete(xjd1_context*);

/* Operators for xjd1_context_config() */
#define XJD1_CONTEXT_LOG   1

/* Open and close a database connection */
int xjd1_open(xjd1_context*, const char *zURI, xjd1**);
int xdj1_config(xjd1*, int, ...);
int xjd1_close(xjd1*);

/* Create a new prepared statement */
int xjd1_stmt_new(xjd1*, const char*, xjd1_stmt**, int*);
int xjd1_stmt_delete(xjd1_stmt*);
int xjd1_stmt_config(xjd1_stmt*, int, ...);

/* Process a prepared statement */
int xjd1_stmt_step(xjd1_stmt*);
int xjd1_stmt_rewind(xjd1_stmt*);
int xjd1_stmt_value(xjd1_stmt*, const char**);

/* Return true if zStmt is a complete query statement */
int xjd1_complete(const char *zStmt);

#define _XJD1_H
