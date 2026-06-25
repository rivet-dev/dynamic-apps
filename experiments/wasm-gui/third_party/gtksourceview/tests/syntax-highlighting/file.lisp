; Define a simple package
(defpackage #:fu
  (:use #:alexandria #:cl)
  (:export
     #:bar))

(in-package :foo)

#|
  Block comment
  more text
  #| embedded block comment |#
|#

(defmacro bar (quux &rest wibble)
  "String with some an escape \""
  `(angle ,quux ,@wibble))

(bar 123 234. 745e7 126f4 #3r12 #xaa0 #O23 #b0101/11
     #< ; invalid sharpsign
     #\; #\Spac\e ; Some characters
     #10*01010101 ; bit vector
     'baz #'gronk)

(format t "~{~a~#[~;, and ~:;, ~]~}" '(a b c))
