import structs/[ArrayList, LinkedList], io/FileReader
include stdarg, memory, string
use sdl, gtk
pi := const 3.14
Int: cover from int {
    toString: func -> String { "%d" format() }
}
Dog: class extends Animal {
    barf: func { ("woof! " * 2) println() }
}
