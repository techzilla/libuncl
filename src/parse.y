/*
** 2011 May 26
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains grammar for XJD1 query language
** using the lemon parser generator to generate C code that runs
** the parser.  Lemon will also generate a header file containing
** numeric codes for all of the tokens.
*/

// All token codes are small integers with #defines that begin with "TK_"
%token_prefix TK_

// The type of the data attached to each token is Token.  This is also the
// default type for non-terminals.
//
%token_type {Token}
%default_type {Token}

// The generated parser function takes a 4th argument as follows:
%extra_argument {void *pParse}

// The name of the generated procedure that implements the parser
// is as follows:
%name xjd1Parser

// The following text is included near the beginning of the C source
// code file that implements the parser.
//
%include {

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() is not used
*/
#define yytestcase(X)

} // end %include

// Input is a single XJD1 command
input ::= cmd.

/////////////////////////// Expression Processing /////////////////////////////
//

%left OR.
%left AND.
%right NOT.
%left IS LIKEOP BETWEEN IN NE EQ.
%left GT LE LT GE.
%right ESCAPE.
%left BITAND BITOR LSHIFT RSHIFT.
%left PLUS MINUS.
%left STAR SLASH REM.
%left CONCAT.
%left COLLATE.
%right BITNOT.

jvalue ::= NUMBER.
jvalue ::= STRING.
jvalue ::= TRUE.
jvalue ::= FALSE.
jvalue ::= NULL.
jvalue ::= JVALUE.

jexpr ::= JVALUE.
jexpr ::= expr.


lvalue ::= ID.
lvalue ::= lvalue DOT ID.
lvalue ::= lvalue LB expr RB.

expr ::= lvalue.
expr ::= NUMBER.
expr ::= STRING.
expr ::= TRUE.
expr ::= FALSE.
expr ::= NULL.
expr ::= ID LP exprlist RP.
expr ::= expr AND expr.
expr ::= expr OR expr.
expr ::= expr LT|GT|GE|LE expr.
expr ::= expr EQ|NE expr.
expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr.
expr ::= expr PLUS|MINUS expr.
expr ::= expr STAR|SLASH|REM expr.
expr ::= expr CONCAT expr.
likeop ::= LIKEOP.
likeop ::= NOT LIKEOP.
expr ::= expr likeop expr.  [LIKEOP]
expr ::= expr IS expr.
expr ::= expr IS NOT expr.
expr ::= NOT expr.
expr ::= BITNOT expr.
expr ::= MINUS expr. [BITNOT]
expr ::= PLUS expr. [BITNOT]
expr ::= LP select RP.
expr ::= LP expr RP.

exprlist ::= nexprlist.
exprlist ::= .
nexprlist ::= expr.
nexprlist ::= nexprlist COMMA expr.

///////////////////// TRANSACTIONS ////////////////////////////
//
cmd ::= BEGIN ID.
cmd ::= ROLLBACK ID.
cmd ::= COMMIT ID.


///////////////////// The CREATE TABLE statement ////////////////////////////
//
cmd ::= CREATE TABLE ifnotexists tabname.
ifnotexists ::= .
ifnotexists ::= IF NOT EXISTS.
tabname ::= ID.
tabname ::= ID DOT ID.

////////////////////////// The DROP TABLE /////////////////////////////////////
//
cmd ::= DROP TABLE ifexists tabname.
ifexists ::= IF EXISTS.
ifexists ::= .

//////////////////////// The SELECT statement /////////////////////////////////
//
cmd ::= select.

%left UNION EXCEPT.
%left INTERSECT.


select ::= oneselect.
select ::= eselect UNION eselect.
select ::= eselect UNION ALL eselect.
select ::= eselect EXCEPT eselect.
select ::= eselect INTERSECT eselect.
eselect ::= select.
eselect ::= expr.       // must be (SELECT...)

oneselect::= SELECT selcollist_opt from where_opt
                    groupby_opt orderby_opt limit_opt.


// selcollist is a list of expressions that are to become the return
// values of the SELECT statement.
//
selcollist_opt ::= .
selcollist_opt ::= selcollist.
selcollist ::= selcol.
selcollist ::= selcollist COMMA selcol.
selcol ::= expr.
selcol ::= expr AS ID.

// A complete FROM clause.
//
from ::= .
from ::= FROM fromlist.
fromlist ::= fromitem.
fromlist ::= fromlist COMMA fromitem.
fromitem ::= ID.
fromitem ::= LP select RP.
fromitem ::= fromitem FLATTENOP LP eachexpr_list RP.

eachexpr ::= expr.
eachexpr ::= expr AS ID.
eachexpr_list ::= eachexpr.
eachexpr_list ::= eachexpr_list COMMA eachexpr.

groupby_opt ::= .
groupby_opt ::= GROUP BY exprlist.
groupby_opt ::= GROUP BY exprlist HAVING expr.

orderby_opt ::= .
orderby_opt ::= ORDER BY sortlist.
sortlist ::= sortlist COMMA sortitem sortorder.
sortlist ::= sortitem sortorder.
sortitem ::= expr.
sortorder ::= ASCENDING.
sortorder ::= DESCENDING.
sortorder ::= .


limit_opt ::= .
limit_opt ::= LIMIT expr.
limit_opt ::= LIMIT expr OFFSET expr. 

where_opt ::= .
where_opt ::= WHERE expr.

/////////////////////////// The DELETE statement /////////////////////////////
//
cmd ::= DELETE FROM tabname where_opt.

////////////////////////// The UPDATE command ////////////////////////////////
//
cmd ::= UPDATE tabname SET setlist where_opt.

setlist ::= setlist COMMA setitem.
setlist ::= setitem.
setitem ::= lvalue EQ jexpr.

////////////////////////// The INSERT command /////////////////////////////////
//
cmd ::= INSERT INTO tabname VALUE jvalue.
cmd ::= INSERT INTO tabname select.
