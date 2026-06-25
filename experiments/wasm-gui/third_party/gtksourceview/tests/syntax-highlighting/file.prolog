conc([],X,X).
conc([Car|Cdr], X, [Car|ConcatCdr]):-
  conc(Cdr, X, ConcatCdr).
