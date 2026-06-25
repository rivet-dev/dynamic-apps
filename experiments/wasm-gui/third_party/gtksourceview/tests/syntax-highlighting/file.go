// gtk-source-lang: Go
/* comment! */
package main
import ("fmt")
var s string = "A string with \x60 a horse \140 \u2014 \U0001F40E %[1]O\n"
var i = 0b0100_0101i
func main() { fmt.Printf(s, 0o_7_7_7); }
type my_struct struct { I int; o string }
func bar( a int32, b string )(c float32) { c = imag(987i) + 1.3 + 0x1.fp-2 * float32(a - int32(len(b))); return }