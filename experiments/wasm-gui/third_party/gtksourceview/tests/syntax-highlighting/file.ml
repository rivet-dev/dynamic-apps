open List

(*comment
(** comment in comment *)
*)

let id = fun x -> x

(**) (*<- empty comment*)
(**ocamldoc (*and comment*) *)

let _ = print_string "ok\n"

