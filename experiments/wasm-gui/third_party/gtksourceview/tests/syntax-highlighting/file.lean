-- line comment

/-
  block comment
  /- nested block comment -/
-/

def main : IO Unit :=
  let a := 1
  let b := 0b1
  let c := 0x1
  let d := '1'
  IO.println "Hello World"

#eval main
