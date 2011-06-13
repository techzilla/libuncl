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
** This file contains code used to render the parse tree.
*/
#include "xjd1Int.h"

/*
** Names of tokens.
*/
static const struct {
  int code;
  const char *zName;
} aToken[] = {
  /* Begin paste of parse_txt.h */
  { TK_SEMI,             "TK_SEMI"            },
  { TK_OR,               "TK_OR"              },
  { TK_AND,              "TK_AND"             },
  { TK_NOT,              "TK_NOT"             },
  { TK_IS,               "TK_IS"              },
  { TK_LIKEOP,           "TK_LIKEOP"          },
  { TK_BETWEEN,          "TK_BETWEEN"         },
  { TK_IN,               "TK_IN"              },
  { TK_NE,               "TK_NE"              },
  { TK_EQ,               "TK_EQ"              },
  { TK_GT,               "TK_GT"              },
  { TK_LE,               "TK_LE"              },
  { TK_LT,               "TK_LT"              },
  { TK_GE,               "TK_GE"              },
  { TK_ESCAPE,           "TK_ESCAPE"          },
  { TK_BITAND,           "TK_BITAND"          },
  { TK_BITOR,            "TK_BITOR"           },
  { TK_LSHIFT,           "TK_LSHIFT"          },
  { TK_RSHIFT,           "TK_RSHIFT"          },
  { TK_PLUS,             "TK_PLUS"            },
  { TK_MINUS,            "TK_MINUS"           },
  { TK_STAR,             "TK_STAR"            },
  { TK_SLASH,            "TK_SLASH"           },
  { TK_REM,              "TK_REM"             },
  { TK_CONCAT,           "TK_CONCAT"          },
  { TK_COLLATE,          "TK_COLLATE"         },
  { TK_BITNOT,           "TK_BITNOT"          },
  { TK_INTEGER,          "TK_INTEGER"         },
  { TK_FLOAT,            "TK_FLOAT"           },
  { TK_STRING,           "TK_STRING"          },
  { TK_TRUE,             "TK_TRUE"            },
  { TK_FALSE,            "TK_FALSE"           },
  { TK_NULL,             "TK_NULL"            },
  { TK_JVALUE,           "TK_JVALUE"          },
  { TK_ID,               "TK_ID"              },
  { TK_DOT,              "TK_DOT"             },
  { TK_LB,               "TK_LB"              },
  { TK_RB,               "TK_RB"              },
  { TK_LP,               "TK_LP"              },
  { TK_RP,               "TK_RP"              },
  { TK_COMMA,            "TK_COMMA"           },
  { TK_UNION,            "TK_UNION"           },
  { TK_EXCEPT,           "TK_EXCEPT"          },
  { TK_INTERSECT,        "TK_INTERSECT"       },
  { TK_ALL,              "TK_ALL"             },
  { TK_SELECT,           "TK_SELECT"          },
  { TK_AS,               "TK_AS"              },
  { TK_FROM,             "TK_FROM"            },
  { TK_FLATTENOP,        "TK_FLATTENOP"       },
  { TK_GROUP,            "TK_GROUP"           },
  { TK_BY,               "TK_BY"              },
  { TK_HAVING,           "TK_HAVING"          },
  { TK_ORDER,            "TK_ORDER"           },
  { TK_ASCENDING,        "TK_ASCENDING"       },
  { TK_DESCENDING,       "TK_DESCENDING"      },
  { TK_LIMIT,            "TK_LIMIT"           },
  { TK_OFFSET,           "TK_OFFSET"          },
  { TK_WHERE,            "TK_WHERE"           },
  { TK_BEGIN,            "TK_BEGIN"           },
  { TK_ROLLBACK,         "TK_ROLLBACK"        },
  { TK_COMMIT,           "TK_COMMIT"          },
  { TK_CREATE,           "TK_CREATE"          },
  { TK_TABLE,            "TK_TABLE"           },
  { TK_IF,               "TK_IF"              },
  { TK_EXISTS,           "TK_EXISTS"          },
  { TK_DROP,             "TK_DROP"            },
  { TK_DELETE,           "TK_DELETE"          },
  { TK_UPDATE,           "TK_UPDATE"          },
  { TK_SET,              "TK_SET"             },
  { TK_INSERT,           "TK_INSERT"          },
  { TK_INTO,             "TK_INTO"            },
  { TK_VALUE,            "TK_VALUE"           },
  /* End paste of parse_txt.h */
  { TK_NOT_LIKEOP,       "TK_NOT_LIKEOP"      },
  { TK_FUNCTION,         "TK_FUNCTION"        },
  { TK_SPACE,            "TK_SPACE"           },
  { TK_ILLEGAL,          "TK_ILLEGAL"         },
};

/*
** Return the name of a token.
*/
const char *xjd1TokenName(int code){
  unsigned int i;
  for(i=0; i<sizeof(aToken)/sizeof(aToken[0]); i++){
    if( aToken[i].code==code ) return aToken[i].zName;
  }
  return "<unknown>";
}

/*
** Output the content of a Command structure.
*/
void xjd1TraceCommand(String *pOut, int indent, const Command *pCmd){
  if( pCmd==0 ) return;
  xjd1StringAppendF(pOut, "%*seCmdType = %s (%d)\n",
          indent, "", xjd1TokenName(pCmd->eCmdType), pCmd->eCmdType);
  switch( pCmd->eCmdType ){
    case TK_SELECT: {
      xjd1TraceQuery(pOut, indent+3, pCmd->u.q.pQuery);
      break;
    }
    default: {
      break;
    }
  }
}


/*
** Output the content of an query.
*/
void xjd1TraceQuery(String *pOut, int indent, const Query *p){
  
}

/*
** Output the content of a data source.
*/
void xjd1TraceDataSrc(String *pOut, int indent, const DataSrc *p){
  
}

/*
** Output the content of an expression.
*/
void xjd1TraceExpr(String *pOut, int indent, const Expr *p){
  
}

/*
** Output the content of an expression list.
*/
void xjd1TraceExprList(String *pOut, int indent, const ExprList *p){
  
}
