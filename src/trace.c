/*tab
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
  { TK_INTEGER,          "TK_INTEGER"         },
  { TK_FLOAT,            "TK_FLOAT"           },
  { TK_STRING,           "TK_STRING"          },
  { TK_TRUE,             "TK_TRUE"            },
  { TK_FALSE,            "TK_FALSE"           },
  { TK_NULL,             "TK_NULL"            },
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
  { TK_ID,               "TK_ID"              },
  { TK_DOT,              "TK_DOT"             },
  { TK_LB,               "TK_LB"              },
  { TK_RB,               "TK_RB"              },
  { TK_LC,               "TK_LC"              },
  { TK_RC,               "TK_RC"              },
  { TK_COLON,            "TK_COLON"           },
  { TK_COMMA,            "TK_COMMA"           },
  { TK_LP,               "TK_LP"              },
  { TK_RP,               "TK_RP"              },
  { TK_UNION,            "TK_UNION"           },
  { TK_EXCEPT,           "TK_EXCEPT"          },
  { TK_INTERSECT,        "TK_INTERSECT"       },
  { TK_ALL,              "TK_ALL"             },
  { TK_SELECT,           "TK_SELECT"          },
  { TK_FROM,             "TK_FROM"            },
  { TK_AS,               "TK_AS"              },
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
  { TK_DATASET,          "TK_DATASET"         },
  { TK_IF,               "TK_IF"              },
  { TK_EXISTS,           "TK_EXISTS"          },
  { TK_DROP,             "TK_DROP"            },
  { TK_DELETE,           "TK_DELETE"          },
  { TK_UPDATE,           "TK_UPDATE"          },
  { TK_SET,              "TK_SET"             },
  { TK_INSERT,           "TK_INSERT"          },
  { TK_INTO,             "TK_INTO"            },
  { TK_VALUE,            "TK_VALUE"           },
  { TK_PRAGMA,           "TK_PRAGMA"          },
  /* End paste of parse_txt.h */
  { TK_NOT_LIKEOP,       "TK_NOT_LIKEOP"      },
  { TK_NOT_IS,           "TK_NOT_IS"          },
  { TK_FUNCTION,         "TK_FUNCTION"        },
  { TK_SPACE,            "TK_SPACE"           },
  { TK_ILLEGAL,          "TK_ILLEGAL"         },
  { TK_CREATEDATASET,    "TK_CREATEDATASET"   },
  { TK_DROPDATASET,      "TK_DROPDATASET"     },
};

/*
** Return the name of a token.
*/
const char *xjd1TokenName(int code){
  unsigned int i;
  for(i=0; i<sizeof(aToken)/sizeof(aToken[0]); i++){
    if( aToken[i].code==code ) return aToken[i].zName+3;
  }
  return "<unknown>";
}

/*
** Output the content of a Command structure.
*/
void xjd1TraceCommand(String *pOut, int indent, const Command *pCmd){
  if( pCmd==0 ) return;
  switch( pCmd->eCmdType ){
    case TK_SELECT: {
      xjd1StringAppendF(pOut, "%*sQuery:\n", indent, "");
      xjd1TraceQuery(pOut, indent+3, pCmd->u.q.pQuery);
      break;
    }
    case TK_CREATEDATASET: {
      xjd1StringAppendF(pOut, "%*sCreate-Dataset: \"%.*s\" if-not-exists=%d\n",
         indent, "", pCmd->u.crtab.name.n, pCmd->u.crtab.name.z,
         pCmd->u.crtab.ifExists);
      break;
    }
    case TK_DROPDATASET: {
      xjd1StringAppendF(pOut, "%*sDrop-Dataset: \"%.*s\" if-exists=%d\n",
         indent, "", pCmd->u.crtab.name.n, pCmd->u.crtab.name.z,
         pCmd->u.crtab.ifExists);
      break;
    }
    case TK_INSERT: {
      xjd1StringAppendF(pOut, "%*sInsert: %.*s\n",
         indent, "", pCmd->u.ins.name.n, pCmd->u.ins.name.z);
      if( pCmd->u.ins.pValue ){
         xjd1StringAppendF(pOut, "%*s value: ");
         xjd1TraceExpr(pOut, pCmd->u.ins.pValue);
         xjd1StringAppend(pOut, "\n", 1);
      }else{
         xjd1TraceQuery(pOut, indent+3, pCmd->u.ins.pQuery);
      }
      break; 
    }
    case TK_DELETE: {
      xjd1StringAppendF(pOut, "%*sDELETE: %.*s\n",
         indent, "", pCmd->u.del.name.n, pCmd->u.del.name.z);
      if( pCmd->u.del.pWhere ){
         xjd1StringAppendF(pOut, "%*s WHERE ", indent, "");
         xjd1TraceExpr(pOut, pCmd->u.del.pWhere);
         xjd1StringAppend(pOut, "\n", 1);
      }
      break; 
    }
    case TK_UPDATE: {
      xjd1StringAppendF(pOut, "%*sUPDATE: %.*s\n",
         indent, "", pCmd->u.update.name.n, pCmd->u.update.name.z);
      xjd1TraceExprList(pOut, indent+3, pCmd->u.update.pChng);
      if( pCmd->u.update.pWhere ){
         xjd1StringAppendF(pOut, "%*s WHERE ", indent, "");
         xjd1TraceExpr(pOut, pCmd->u.update.pWhere);
         xjd1StringAppend(pOut, "\n", 1);
      }
      break; 
    }
    case TK_PRAGMA: {
      xjd1StringAppendF(pOut, "%*sPRAGMA: %.*s",
         indent, "", pCmd->u.prag.name.n, pCmd->u.prag.name.z);
      if( pCmd->u.prag.pValue ){
        xjd1StringAppend(pOut, "(", 1);
        xjd1TraceExpr(pOut, pCmd->u.ins.pValue);
        xjd1StringAppend(pOut, ")\n", 2);
      }else{
        xjd1StringAppend(pOut, "\n", 1);
      }
      break; 
    }
    default: {
      xjd1StringAppendF(pOut, "%*seCmdType = %s (%d)\n",
          indent, "", xjd1TokenName(pCmd->eCmdType), pCmd->eCmdType);
      break;
    }
  }
}


/*
** Output the content of an query.
*/
void xjd1TraceQuery(String *pOut, int indent, const Query *p){
  if( p==0 ) return;
  xjd1StringAppendF(pOut, "%*seQType = %s\n",
          indent, "", xjd1TokenName(p->eQType));
  switch( p->eQType ){
    case TK_SELECT: {
      if( p->u.simple.pRes ){
        xjd1StringAppendF(pOut, "%*sResult: ", indent, "");
        xjd1TraceExpr(pOut, p->u.simple.pRes);
        xjd1StringAppend(pOut, "\n", 1);
      }
      if( p->u.simple.pFrom ){
        xjd1StringAppendF(pOut, "%*sFROM:\n", indent, "");
        xjd1TraceDataSrc(pOut, indent+3, p->u.simple.pFrom);
      }
      if( p->u.simple.pWhere ){
        xjd1StringAppendF(pOut, "%*sWHERE: ", indent, "");
        xjd1TraceExpr(pOut, p->u.simple.pWhere);
        xjd1StringAppendF(pOut, "\n");
      }
      if( p->u.simple.pGroupBy ){
        xjd1StringAppendF(pOut, "%*sGROUP-BY:\n", indent, "");
        xjd1TraceExprList(pOut, indent+3, p->u.simple.pGroupBy);
      }
      if( p->u.simple.pHaving ){
        xjd1StringAppendF(pOut, "%*sHAVING: ", indent, "");
        xjd1TraceExpr(pOut, p->u.simple.pWhere);
        xjd1StringAppendF(pOut, "\n");
      }
      if( p->u.simple.pOrderBy ){
        xjd1StringAppendF(pOut, "%*sORDER-BY:\n", indent, "");
        xjd1TraceExprList(pOut, indent+3, p->u.simple.pOrderBy);
      }
      if( p->u.simple.pLimit ){
        xjd1StringAppendF(pOut, "%*sLIMIT: ", indent, "");
        xjd1TraceExpr(pOut, p->u.simple.pLimit);
        xjd1StringAppendF(pOut, "\n");
      }
      if( p->u.simple.pOffset ){
        xjd1StringAppendF(pOut, "%*sOFFSET: ", indent, "");
        xjd1TraceExpr(pOut, p->u.simple.pOffset);
        xjd1StringAppendF(pOut, "\n");
      }
      break;
    }
    default: {
      if( p->u.compound.pLeft ){
        xjd1StringAppendF(pOut, "%*sLeft:\n", indent, "");
        xjd1TraceQuery(pOut, indent+3, p->u.compound.pLeft);
      }
      if( p->u.compound.pRight ){
        xjd1StringAppendF(pOut, "%*sLeft:\n", indent, "");
        xjd1TraceQuery(pOut, indent+3, p->u.compound.pRight);
      }
      break;
    }
  }
}

/*
** Output the content of a data source.
*/
void xjd1TraceDataSrc(String *pOut, int indent, const DataSrc *p){
  if( p==0 ) return;
  /*xjd1StringAppendF(pOut, "%*s%s\n", indent, "", xjd1TokenName(p->eDSType));*/
  switch( p->eDSType ){
    case TK_ID: {
      xjd1StringAppendF(pOut, "%*sID.%.*s", indent, "",
               p->u.tab.name.n, p->u.tab.name.z);
      if( p->asId.n ){
        xjd1StringAppendF(pOut, " AS %s", p->asId.n, p->asId.z);
      }
      xjd1StringAppend(pOut, "\n", 1);
      break;
    }
    case TK_COMMA: {
      xjd1StringAppendF(pOut, "%*sJoin:\n", indent, "");
      xjd1TraceDataSrc(pOut, indent+3, p->u.join.pLeft);
      xjd1TraceDataSrc(pOut, indent+3, p->u.join.pRight);
      break;
    }
    case TK_SELECT: {
      xjd1StringAppendF(pOut, "%*sSubquery:\n", indent, "");
      xjd1TraceQuery(pOut, indent+3, p->u.subq.q);
      break;
    }
    case TK_FLATTENOP: {
      xjd1TraceDataSrc(pOut, indent, p->u.flatten.pNext);
      xjd1StringAppendF(pOut, "%*s%.*s:\n", indent, "",
            p->u.flatten.opName.n, p->u.flatten.opName.z);
      xjd1TraceExprList(pOut, indent+3, p->u.flatten.pList);
      break;
    }
  }
}

/*
** Output the content of an expression.
*/
void xjd1TraceExpr(String *pOut, const Expr *p){
  if( p==0 ) return;
  switch( p->eType ){
    case TK_JVALUE: {
      xjd1JsonRender(pOut, p->u.json.p);
      break;
    }
    case TK_STRUCT: {
      int i;
      ExprList *pList = p->u.st;
      xjd1StringAppend(pOut, "{", 1);
      for(i=0; i<pList->nEItem; i++){
        ExprItem *pItem = &pList->apEItem[i];
        if( i>0 ) xjd1StringAppend(pOut, ",", 1);
        xjd1StringAppendF(pOut, "%.*s:", pItem->tkAs.n, pItem->tkAs.z);
        xjd1TraceExpr(pOut, pItem->pExpr);
      }
      xjd1StringAppend(pOut, "}", 1);
      break;
    }
    case TK_ARRAY: {
      int i;
      ExprList *pList = p->u.st;
      xjd1StringAppend(pOut, "[", 1);
      for(i=0; i<pList->nEItem; i++){
        ExprItem *pItem = &pList->apEItem[i];
        if( i>0 ) xjd1StringAppend(pOut, ",", 1);
        xjd1TraceExpr(pOut, pItem->pExpr);
      }
      xjd1StringAppend(pOut, "]", 1);
      break;
    }
    case TK_DOT:
    case TK_AND:
    case TK_OR:
    case TK_LT:
    case TK_LE:
    case TK_GT:
    case TK_GE:
    case TK_EQ:
    case TK_NE:
    case TK_BITAND:
    case TK_BITOR:
    case TK_BITNOT:
    case TK_LSHIFT:
    case TK_RSHIFT:
    case TK_PLUS:
    case TK_MINUS:
    case TK_STAR:
    case TK_SLASH:
    case TK_REM:
    case TK_CONCAT:
    case TK_IS:
    case TK_NOT:
    case TK_NOT_IS: {
      if( p->u.bi.pRight ){
        xjd1StringAppend(pOut, "(", 1);
        xjd1TraceExpr(pOut, p->u.bi.pLeft);
        xjd1StringAppendF(pOut, ") %s (", xjd1TokenName(p->eType));
        xjd1TraceExpr(pOut, p->u.bi.pRight);
        xjd1StringAppend(pOut, ")", 1);
      }else{
        xjd1StringAppendF(pOut, "%s (", xjd1TokenName(p->eType));
        xjd1TraceExpr(pOut, p->u.bi.pLeft);
        xjd1StringAppend(pOut, ")", 1);
      }
      break;
    }
    default: {
      xjd1StringAppend(pOut, xjd1TokenName(p->eType), -1);
      break;
    }
  }
}

/*
** Output the content of an expression list.
*/
void xjd1TraceExprList(String *pOut, int indent, const ExprList *p){
  int i;
  if( p==0 || p->nEItem==0 ) return;
  for(i=0; i<p->nEItem; i++){
    xjd1StringAppendF(pOut, "%*s%d: ", indent, "", i);
    xjd1TraceExpr(pOut, p->apEItem[i].pExpr);
    if( p->apEItem[i].tkAs.n ){
      xjd1StringAppendF(pOut, " AS %.*s\n",
             p->apEItem[i].tkAs.n, p->apEItem[i].tkAs.z);
    }else{
      xjd1StringAppend(pOut, "\n", 1);
    }
  }
}
