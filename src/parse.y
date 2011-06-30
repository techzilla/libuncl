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
*****************************************************************************
** Language grammar
*/

// All token codes are small integers with #defines that begin with "TK_"
%token_prefix TK_

// The type of the data attached to each token is Token.  This is also the
// default type for non-terminals.
//
%token_type {Token}
%default_type {Token}

// The generated parser function takes a 4th argument as follows:
%extra_argument {Parse *p}

// The name of the generated procedure that implements the parser
// is as follows:
%name xjd1Parser

// Handle syntax errors.
%syntax_error {
  p->errCode = XJD1_SYNTAX;
  xjd1Error(p->pConn, XJD1_SYNTAX, "syntax error near \"%.*s\"",
            p->sTok.n, p->sTok.z);
}

// The following text is included near the beginning of the C source
// code file that implements the parser.
//
%include {
#include "xjd1Int.h"

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() be a no-op
*/
#define yytestcase(X)

} // end %include

// Input is a single XJD1 command
%type cmd {Command*}
%destructor cmd {(void)p;}
input ::= cmd(X) SEMI.   {p->pCmd = X;}

/////////////////////////// Expression Processing /////////////////////////////
//

%include {
  /* A JSON literal for a real number */
  static JsonNode *jsonReal(Parse *p, Token *pTok){
    JsonNode *pNew = xjd1JsonNew(p->pPool);
    if( pNew ){
      pNew->eJType = XJD1_REAL;
      pNew->u.r = atof(pTok->z);
    }
    return pNew;
  }

  /* Convert a token into a zero-terminated string */
  static char *tokenStr(Parse *p, Token *pTok){
    return pTok ? xjd1PoolDup(p->pPool, pTok->z, pTok->n) : 0;
  }

  /* A JSON literal for a string */
  static JsonNode *jsonString(Parse *p, Token *pTok){
    JsonNode *pNew = xjd1JsonNew(p->pPool);
    if( pNew ){
      pNew->eJType = XJD1_STRING;
      pNew->u.z = tokenStr(p, pTok);
      xjd1DequoteString(pNew->u.z, pTok->n);
    }
    return pNew;
  }

  /* A JSON literal for a boolean or NULL */
  static JsonNode *jsonType(Parse *p, int eJType){
    JsonNode *pNew = xjd1JsonNew(p->pPool);
    if( pNew ){
      pNew->eJType = eJType;
    }
    return pNew;
  }
}
%type jvalue {JsonNode*}
jvalue(A) ::= INTEGER(X).              {A = jsonReal(p,&X);}
jvalue(A) ::= FLOAT(X).                {A = jsonReal(p,&X);}
jvalue(A) ::= STRING(X).               {A = jsonString(p,&X);}
jvalue(A) ::= TRUE.                    {A = jsonType(p,XJD1_TRUE);}
jvalue(A) ::= FALSE.                   {A = jsonType(p,XJD1_FALSE);}
jvalue(A) ::= NULL.                    {A = jsonType(p,XJD1_NULL);}


%left OR.
%left AND.
%right NOT.
%left IS LIKEOP BETWEEN IN NE EQEQ.
%left GT LE LT GE.
%right ESCAPE.
%left BITAND BITOR LSHIFT RSHIFT.
%left PLUS MINUS.
%left STAR SLASH REM.
%left CONCAT.
%left COLLATE.
%right BITNOT.

%include {
  /* Generate an Expr object from an identifer token */
  static Expr *idExpr(Parse *p, Token *pTok){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_ID;
      pNew->eClass = XJD1_EXPR_TK;
      pNew->u.id.zId = tokenStr(p, pTok);
    }
    return pNew;
  }

  /* Generate an Expr object that is a binary operator on two
  ** other Expr objects. */
  static Expr *biExpr(Parse *p, Expr *pLeft, int eOp, Expr *pRight){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = eOp; 
      pNew->eClass = XJD1_EXPR_BI;
      pNew->u.bi.pLeft = pLeft;
      pNew->u.bi.pRight = pRight;
    }
    return pNew;
  }
  /* Generate an Expr object for an lvalue */
  static Expr *lvalueExpr(Parse *p, Expr *pLeft, Token *pId){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_DOT; 
      pNew->eClass = XJD1_EXPR_LVALUE;
      pNew->u.lvalue.pLeft = pLeft;
      pNew->u.lvalue.zId = tokenStr(p, pId);
    }
    return pNew;
  }

  /* Generate an Expr object that is a function call. */
  static Expr *funcExpr(Parse *p, Token *pFName, ExprList *pArgs){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_FUNCTION; 
      pNew->eClass = XJD1_EXPR_FUNC;
      pNew->u.func.zFName = tokenStr(p, pFName);
      pNew->u.func.args = pArgs;
    }
    return pNew;
  }

  /* Generate an Expr object that is a subquery. */
  static Expr *subqExpr(Parse *p, Query *pQuery){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_SELECT;
      pNew->eClass = XJD1_EXPR_Q;
      pNew->u.subq.p = pQuery;
    }
    return pNew;
  }

  /* Generate an Expr object that is a structure */
  static Expr *stExpr(Parse *p, ExprList *pList){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_STRUCT;
      pNew->eClass = XJD1_EXPR_STRUCT;
      pNew->u.st = pList;
    }
    return pNew;
  }

  /* Generate an Expr object that is an array7 */
  static Expr *arExpr(Parse *p, ExprList *pList){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_ARRAY;
      pNew->eClass = XJD1_EXPR_ARRAY;
      pNew->u.ar = pList;
    }
    return pNew;
  }

  /* Generate an Expr object that is JSON value literal */
  static Expr *jsonExpr(Parse *p, JsonNode *pNode){
    Expr *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eType = TK_JVALUE;
      pNew->eClass = XJD1_EXPR_JSON;
      pNew->u.json.p = pNode;
    }
    return pNew;
  }

  /* Append a new expression to an expression list.  Allocate the
  ** expression list object if necessary. */
  static ExprList *apndExpr(Parse *p, ExprList *pList, Expr *pExpr, Token *pT){
    ExprItem *pItem;
    if( pList==0 ){
      pList = xjd1PoolMallocZero(p->pPool, sizeof(*pList)+4*sizeof(ExprItem));
      if( pList==0 ) return 0;
      pList->nEAlloc = 0;
    }
    if( pList->nEAlloc<=pList->nEItem ){
      ExprItem *pNew;
      int n = pList->nEAlloc*4;
      if( n==0 ) n = 10;
      pNew = xjd1PoolMalloc(p->pPool, sizeof(ExprItem)*n);
      if( pNew==0 ) return pList;
      memcpy(pNew, pList->apEItem, pList->nEItem*sizeof(ExprItem));
      pList->nEAlloc = n;
      pList->apEItem = pNew;
    }
    pItem = &pList->apEItem[pList->nEItem++];
    pItem->zAs = tokenStr(p, pT);
    pItem->pExpr = pExpr;
    return pList;
  }
}

%type lvalue {Expr*}
lvalue(A) ::= ID(X).                   {A = idExpr(p,&X);}
lvalue(A) ::= lvalue(X) DOT ID(Y).     {A = lvalueExpr(p,X,&Y);}
lvalue(A) ::= lvalue(X) LB expr(Y) RB. {A = biExpr(p,X,TK_LB,Y);}

%type expr {Expr*}
expr(A) ::= lvalue(X).               {A = X;}
expr(A) ::= jvalue(X).               {A = jsonExpr(p,X);}
expr(A) ::= LC structlist(X) RC.     {A = stExpr(p,X);}
expr(A) ::= LC RC.                   {A = stExpr(p,0);}
expr(A) ::= LB arraylist(X) RB.      {A = arExpr(p,X);}
expr(A) ::= LB RB.                   {A = arExpr(p,0);}

%type structlist {ExprList*}
structlist(A) ::= ID|STRING(Y) COLON expr(Z).    {A = apndExpr(p,0,Z,&Y);}
structlist(A) ::= structlist(X) COMMA ID|STRING(Y) COLON expr(Z).
                                                 {A = apndExpr(p,X,Z,&Y);}
%type arraylist {ExprList*}
arraylist(A) ::= expr(Y).                        {A = apndExpr(p,0,Y,0);}
arraylist(A) ::= arraylist(X) COMMA expr(Y).     {A = apndExpr(p,X,Y,0);}



expr(A) ::= ID(X) LP exprlist(Y) RP.  {A = funcExpr(p,&X,Y);}
expr(A) ::= expr(X) AND(OP) expr(Y).  {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) OR(OP) expr(Y).              {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) LT|GT|GE|LE(OP) expr(Y).     {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) EQEQ|NE(OP) expr(Y).         {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) BITAND|BITOR|LSHIFT|RSHIFT(OP) expr(Y).
                                                 {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) PLUS|MINUS(OP) expr(Y).      {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) STAR|SLASH|REM(OP) expr(Y).  {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) CONCAT(OP) expr(Y).          {A = biExpr(p,X,@OP,Y);}
%type likeop {int}
likeop(A) ::= LIKEOP(OP).                        {A = @OP;}
likeop(A) ::= NOT LIKEOP(OP).                    {A = 128+@OP;}
expr(A) ::= expr(X) likeop(OP) expr(Y). [LIKEOP] {A = biExpr(p,X,OP,Y);}
expr(A) ::= expr(X) IS(OP) expr(Y).              {A = biExpr(p,X,@OP,Y);}
expr(A) ::= expr(X) IS NOT expr(Y).              {A = biExpr(p,X,TK_NOT_IS,Y);}
expr(A) ::= NOT(OP) expr(X).                     {A = biExpr(p,X,@OP,0);}
expr(A) ::= BITNOT(OP) expr(X).                  {A = biExpr(p,X,@OP,0);}
expr(A) ::= MINUS(OP) expr(X). [BITNOT]          {A = biExpr(p,X,@OP,0);}
expr(A) ::= PLUS(OP) expr(X). [BITNOT]           {A = biExpr(p,X,@OP,0);}
expr(A) ::= LP select(X) RP.                     {A = subqExpr(p,X);}
expr(A) ::= LP expr(X) RP.                       {A = X;}


%type exprlist {ExprList*}
%type nexprlist {ExprList*}
exprlist(A) ::= nexprlist(X).     {A = X;}
exprlist(A) ::= .                 {A = 0;}
nexprlist(A) ::= expr(X).                     {A = apndExpr(p,0,X,0);}
nexprlist(A) ::= nexprlist(X) COMMA expr(Y).  {A = apndExpr(p,X,Y,0);}

//////////////////////// The SELECT statement /////////////////////////////////
//
cmd(A) ::= select(X).  {
  Command *pNew = xjd1PoolMalloc(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_SELECT;
    pNew->u.q.pQuery = X;
  }
  A = pNew;
}

%left UNION EXCEPT.
%left INTERSECT.
%type select {Query*}
%type eselect {Query*}
%type oneselect {Query*}

%include {
  /* The value of a LIMIT ... OFFSET ... clause. */
  typedef struct LimitOffset {
    Expr *pLimit;
    Expr *pOffset;
  } LimitOffset;

  /* The value of a GROUP BY ... HAVING ... clause */
  typedef struct GroupByHaving {
    ExprList *pGroupBy;
    Expr *pHaving;
  } GroupByHaving;

  /* Construct a simple query object */
  static Query *simpleQuery(
    Parse *p,
    Expr *pRes,
    DataSrc *pFrom,
    Expr *pWhere,
    GroupByHaving *pGroupBy,
    ExprList *pOrderBy,
    LimitOffset *pLimit
  ){
    Query *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eQType = TK_SELECT;
      pNew->u.simple.pRes = pRes;
      pNew->u.simple.pFrom = pFrom;
      pNew->u.simple.pWhere = pWhere;
      pNew->u.simple.pGroupBy = pGroupBy ? pGroupBy->pGroupBy : 0;
      pNew->u.simple.pHaving = pGroupBy ? pGroupBy->pHaving : 0;
      pNew->u.simple.pOrderBy = pOrderBy;
      pNew->u.simple.pLimit = pLimit ? pLimit->pLimit : 0;
      pNew->u.simple.pOffset = pLimit ? pLimit->pOffset : 0;
    }
    return pNew;
  }

  /* Construct a compound query object */
  static Query *compoundQuery(
    Parse *p,
    Query *pLeft,
    int eOp,
    Query *pRight
  ){
    Query *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eQType = eOp;
      pNew->u.compound.pLeft = pLeft;
      pNew->u.compound.pRight = pRight;
    }
    return pNew;
  }
}


select(A) ::= oneselect(X).                        {A = X;}
select(A) ::= eselect(X) UNION(OP) eselect(Y).     {A=compoundQuery(p,X,@OP,Y);}
select(A) ::= eselect(X) UNION ALL(OP) eselect(Y). {A=compoundQuery(p,X,@OP,Y);}
select(A) ::= eselect(X) EXCEPT(OP) eselect(Y).    {A=compoundQuery(p,X,@OP,Y);}
select(A) ::= eselect(X) INTERSECT(OP) eselect(Y). {A=compoundQuery(p,X,@OP,Y);}
eselect(A) ::= select(X).                          {A = X;}
eselect(A) ::= expr(X).       // must be (SELECT...)
  {A = X->u.subq.p;}

oneselect(A) ::= SELECT expr_opt(S) from(F) where_opt(W)
                    groupby_opt(G) orderby_opt(O) limit_opt(L).
  {A = simpleQuery(p,S,F,W,&G,O,&L);}


// The result set of an expression can be either an JSON expression
// or nothing.
//
%type expr_opt {Expr*}
expr_opt(A) ::= .                       {A = 0;}
expr_opt(A) ::= expr(X).                {A = X;}


// A complete FROM clause.
//
%type from {DataSrc*}
%type fromlist {DataSrc*}
%type fromitem {DataSrc*}
%include {
  /* Create a new data source that is a named table */
  static DataSrc *tblDataSrc(Parse *p, Token *pTab, Token *pAs){
    DataSrc *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eDSType = TK_ID;
      pNew->u.tab.zName = tokenStr(p, pTab);
      pNew->zAs = tokenStr(p, pAs);
    }
    return pNew;
  }

  /* Create a new data source that is a join */
  static DataSrc *joinDataSrc(Parse *p, DataSrc *pLeft, DataSrc *pRight){
    DataSrc *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eDSType = TK_COMMA;
      pNew->u.join.pLeft = pLeft;
      pNew->u.join.pRight = pRight;
    }
    return pNew;
  }

  /* Create a new subquery data source */
  static DataSrc *subqDataSrc(Parse *p, Query *pSubq, Token *pAs){
    DataSrc *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eDSType = TK_SELECT;
      pNew->u.subq.q = pSubq;
      pNew->zAs = tokenStr(p, pAs);
    }
    return pNew;
  }

  /* Create a new data source that is a FLATTEN or EACH operator */
  static DataSrc *flattenDataSrc(
    Parse *p,
    DataSrc *pLeft,
    Token *pOp,
    ExprList *pArgs
  ){
    DataSrc *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eDSType = TK_FLATTENOP;
      pNew->u.flatten.pNext = pLeft;
      pNew->u.flatten.cOpName = pOp->z[0];
      pNew->u.flatten.pList = pArgs;
    }
    return pNew;
  }
}
from(A) ::= .                                    {A = 0;}
from(A) ::= FROM fromlist(X).                    {A = X;}
fromlist(A) ::= fromitem(X).                     {A = X;}
fromlist(A) ::= fromlist(X) COMMA fromitem(Y).   {A = joinDataSrc(p,X,Y);}
fromitem(A) ::= ID(X).                           {A = tblDataSrc(p,&X,0);}
fromitem(A) ::= ID(X) AS ID(Y).                  {A = tblDataSrc(p,&X,&Y);}
fromitem(A) ::= LP select(X) RP AS ID(Y).        {A = subqDataSrc(p,X,&Y);}
fromitem(A) ::= fromitem(X) FLATTENOP(Y) LP eachexpr_list(Z) RP.
                                                 {A = flattenDataSrc(p,X,&Y,Z);}

%type eachexpr_list {ExprList*}
eachexpr_list(A) ::= expr(Y).                         {A = apndExpr(p,0,Y,0);}
eachexpr_list(A) ::= expr(Y) AS ID(Z).                {A = apndExpr(p,0,Y,&Z);}
eachexpr_list(A) ::= eachexpr_list(X) COMMA expr(Y).  {A = apndExpr(p,X,Y,0);}
eachexpr_list(A) ::= eachexpr_list(X) COMMA expr(Y) AS ID(Z).
                                                      {A = apndExpr(p,X,Y,&Z);}

%type groupby_opt {GroupByHaving}
groupby_opt(A) ::= .                            {A.pGroupBy=0; A.pHaving=0;}
groupby_opt(A) ::= GROUP BY exprlist(X).        {A.pGroupBy=X; A.pHaving=0;}
groupby_opt(A) ::= GROUP BY exprlist(X) HAVING expr(Y).
                                                {A.pGroupBy=X; A.pHaving=Y;}

%type orderby_opt {ExprList*}
%type sortlist {ExprList*}
orderby_opt(A) ::= .                          {A = 0;}
orderby_opt(A) ::= ORDER BY sortlist(X).      {A = X;}
sortlist(A) ::= sortlist(X) COMMA expr(Y) sortorder(Z).
                                              {A = apndExpr(p,X,Y,Z.n?&Z:0);}
sortlist(A) ::= expr(Y) sortorder(Z).         {A = apndExpr(p,0,Y,Z.n?&Z:0);}
sortorder(A) ::= ASCENDING(X).    {A = X;}
sortorder(A) ::= DESCENDING(X).   {A = X;}
sortorder(A) ::= .                {A.z=""; A.n=0;}

%type limit_opt {LimitOffset}
limit_opt(A) ::= .                             {A.pLimit=0; A.pOffset=0;}
limit_opt(A) ::= LIMIT expr(X).                {A.pLimit=X; A.pOffset=0;}
limit_opt(A) ::= LIMIT expr(X) OFFSET expr(Y). {A.pLimit=X; A.pOffset=Y;}

%type where_opt {Expr*}
where_opt(A) ::= .                  {A = 0;}
where_opt(A) ::= WHERE expr(X).     {A = X;}

///////////////////// TRANSACTIONS ////////////////////////////
//
cmd ::= BEGIN ID.
cmd ::= ROLLBACK ID.
cmd ::= COMMIT ID.

///////////////////// The CREATE COLLECTION statement ////////////////////////
//
cmd(A) ::= CREATE COLLECTION ifnotexists(B) tabname(N). {
  Command *pNew = xjd1PoolMalloc(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_CREATECOLLECTION;
    pNew->u.crtab.ifExists = B;
    pNew->u.crtab.zName = tokenStr(p, &N);
  }
  A = pNew;
}
%type ifnotexists {int}
ifnotexists(A) ::= .                    {A = 0;}
ifnotexists(A) ::= IF NOT EXISTS.       {A = 1;}
tabname(A) ::= ID(X).                   {A = X;}

////////////////////////// The DROP COLLECTION ///////////////////////////////
//
cmd(A) ::= DROP COLLECTION ifexists(B) tabname(N). {
  Command *pNew = xjd1PoolMalloc(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_DROPCOLLECTION;
    pNew->u.crtab.ifExists = B;
    pNew->u.crtab.zName = tokenStr(p, &N);
  }
  A = pNew;
}
%type ifexists {int}
ifexists(A) ::= IF EXISTS.  {A = 1;}
ifexists(A) ::= .           {A = 0;}


/////////////////////////// The DELETE statement /////////////////////////////
//
cmd(A) ::= DELETE FROM tabname(N) where_opt(W). {
  Command *pNew = xjd1PoolMalloc(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_DELETE;
    pNew->u.del.zName = tokenStr(p, &N);
    pNew->u.del.pWhere = W;
  }
  A = pNew;
}

////////////////////////// The UPDATE command ////////////////////////////////
//
cmd(A) ::= UPDATE tabname(N) SET setlist(L) where_opt(W) upsert_opt(U). {
  Command *pNew = xjd1PoolMalloc(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_UPDATE;
    pNew->u.update.zName = tokenStr(p, &N);
    pNew->u.update.pWhere = W;
    pNew->u.update.pChng = L;
    pNew->u.update.pUpsert = U;
  }
  A = pNew;
}

%type setlist {ExprList*}
setlist(A) ::= setlist(X) COMMA lvalue(Y) EQ expr(Z). {
   A = apndExpr(p,X,Y,0);
   A = apndExpr(p,A,Z,0);
}
setlist(A) ::= lvalue(Y) EQ expr(Z). {
   A = apndExpr(p,0,Y,0);
   A = apndExpr(p,A,Z,0);
}

%type upsert_opt {Expr*}
upsert_opt(A) ::= .                       {A = 0;}
upsert_opt(A) ::= ELSE INSERT expr(X).    {A = X;}



////////////////////////// The INSERT command /////////////////////////////////
//
cmd(A) ::= INSERT INTO tabname(N) VALUE expr(V). {
  Command *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_INSERT;
    pNew->u.ins.zName = tokenStr(p, &N);
    pNew->u.ins.pValue = V;
  }
  A = pNew;
}
cmd(A) ::= INSERT INTO tabname(N) select(Q). {
  Command *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
  if( pNew ){
    pNew->eCmdType = TK_INSERT;
    pNew->u.ins.zName = tokenStr(p, &N);
    pNew->u.ins.pQuery = Q;
  }
  A = pNew;
}

////////////////////////// The PRAGMA command /////////////////////////////////
//
%include {
  static Command *makePrag(Parse *p, Token *pName, Expr *pValue){
    Command *pNew = xjd1PoolMallocZero(p->pPool, sizeof(*pNew));
    if( pNew ){
      pNew->eCmdType = TK_PRAGMA;
      pNew->u.prag.zName = tokenStr(p, pName);
      pNew->u.prag.pValue = pValue;
    }
    return pNew;
  }
}
cmd(A) ::= PRAGMA ID(N).                {A = makePrag(p,&N,0);}
cmd(A) ::= PRAGMA ID(N) EQ expr(V).     {A = makePrag(p,&N,V);}
cmd(A) ::= PRAGMA ID(N) LP expr(V) RP.  {A = makePrag(p,&N,V);}
