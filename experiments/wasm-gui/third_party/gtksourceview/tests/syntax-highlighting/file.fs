(* Simple F# sample *)
let rec map func lst =
    match lst with
       | [] -> []
       | head :: tail -> func head :: map func tail
 
let myList = [1;3;5]
let newList = map (fun x -> x + 1) myList
