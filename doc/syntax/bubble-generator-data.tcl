
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
  sql-stmt {
    line
      {or
         select-stmt
         create-collection-stmt
         drop-collection-stmt
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
       {optx ORDER BY {loop expr ,}}
       {optx LIMIT expr {optx OFFSET expr}}
  }

  select-core {
     stack
       {line SELECT {opt expr}} 
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

  create-collection-stmt {
    line CREATE COLLECTION {optx IF NOT EXISTS} /collection-name
  }

  drop-collection-stmt {
    line DROP COLLECTION {optx IF EXISTS} /collection-name
  }

  insert-stmt {
       line
          INSERT INTO /collection-name 
            {or 
                 {line VALUE expr} 
                 {line select}
            }
  }

  update-stmt {
     stack {
       line 
         UPDATE /collection-name SET {loop {line property = expr} ,}
     } {
       line
              {optx WHERE expr}
              {optx ELSE INSERT expr}
     }
  }

  delete-stmt {
    line DELETE FROM /collection-name {optx WHERE expr}
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
      {line unary-operator expr}
      {line ( expr )}
      {line ( select )}
    }
  }

  begin-stmt    { line BEGIN /transaction-name }
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



