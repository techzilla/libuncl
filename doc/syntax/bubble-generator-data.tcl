
# This file contains the data used by the three syntax diagram rendering
# programs:
#
#   bubble-generator.tcl
#   bubble-generator-bnf.tcl
#   bubble-generator-bnf.tcl
#

# Graphs:
#

set all_graphs {
  unql-stmt {
    line {opt EXPLAIN}
      {or
         select-stmt
         create-collection-stmt
         drop-collection-stmt
         create-index-stmt
         drop-index-stmt
         insert-stmt
         update-stmt
         delete-stmt
         begin-stmt
         commit-stmt
         rollback-stmt
         pragma-stmt
      }
  }
  
  select-stmt {
    stack
       {loop {line select-core nil} {nil compound-operator nil}}
       {optx ORDER BY sorting-list}
       {optx LIMIT expr {optx OFFSET expr}}
  }

  select-core {
     stack
       {line SELECT {or {} DISTINCT ALL} {opt expr} {opt {line AS /name}}} 
       {optx FROM {loop { line data-source {opt AS /name} } ,}}
       {optx WHERE expr}
       {optx GROUP BY {loop expr ,} {optx HAVING expr}}
        
  }

  data-source {
    stack {
      line {or
        {line /collection-name}
        {line ( select ) }
      }
    } {
      optx {
        loop {
          line {or FLATTEN EACH} ( {
              loop {line expr {opt {line AS /name}}} ,
          } ) 
        } ""
      }
    }
  }

  compound-operator {
     or {line UNION {optx ALL}} INTERSECT EXCEPT
  }

  sorting-list {
     loop {line expr {or {} ASC DESC}} ,
  }

  create-collection-stmt {
    line CREATE COLLECTION /collection-name
         {opt {line OPTIONS expr}}
  }

  drop-collection-stmt {
    line DROP COLLECTION /collection-name
  }

  create-index-stmt {
    line CREATE INDEX /index-name ON ( sorting-list )
         {opt {line OPTIONS expr}}
  }

  drop-index-stmt {
    line DROP INDEX /index-name
  }

  insert-stmt {
       line
          {or {} SYNC ASYNC}
          INSERT INTO /collection-name 
            {or 
                 {line VALUE expr} 
                 {line select}
            }
  }

  update-stmt {
     stack {
       line 
         {or {} SYNC ASYNC}
         UPDATE /collection-name SET {loop {line property = expr} ,}
     } {
       line
              {optx WHERE expr}
              {optx ELSE INSERT expr}
     }
  }

  delete-stmt {
    line  {or {} SYNC ASYNC} DELETE FROM /collection-name {optx WHERE expr}
  }

  property {
    line /name {
        loop { or {} {line . /name} {line [ expr ]} } {}
    }
  }

  expr {
    line {or
      {line property}
      {line /integer-literal}
      {line /real-literal}
      {line /string-literal}
      {line /true}
      {line /false}
      {line "\xA0\x7B\xA0" {loop {line /name : expr} , } "\xA0\x7D\xA0" }
      {line "\xA0[\xA0" {loop {line expr} , } "\xA0]\xA0" }
      {line /function-name ( {toploop expr ,} )}
      {line expr binary-operator expr}
      {line expr ? expr : expr}
      {line unary-operator expr}
      {line ( expr )}
      {line ( select )}
    }
  }

  begin-stmt    { line BEGIN /transaction-name
     {or {} SERIALIZABLE SNAPSHOT 
         {line REPEATABLE READS}
         {line READ COMMITTED}
         {line READ UNCOMMITTED}}
  }
  commit-stmt   { line COMMIT /transaction-name }
  rollback-stmt { line ROLLBACK /transaction-name }

  pragma-stmt {
     line PRAGMA /pragma-name
          {or
              nil
              {line = expr}
              {line ( expr )}
          }
  }
}
