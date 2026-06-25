#[=[
    This is a bracket comment.
    TODO markers are recognized here.
]=]

# This is a line comment.
# TODO markers are recognized here, too.

set(words
    These are unquoted arguments.
    They can include variable references: ${var}, $ENV{var},
    and escape sequences: \n\".)

message("This is an unquoted argument.
    It can also include variable references: ${APPLE}, $ENV{CC},
    and escape sequences: \t\ \\.
    In addition, line continuations: \
    are allowed.")

message("Variable references can nest: ${x$ENV{y${z}w}v}")

message([==[
    This is a bracket argument.
    Variable references (${x}) or escape sequences (\n)
    are not processed here.
]==])

# These are examples of legacy unquoted argument syntax
# from the cmake-language manual page.
set(arg_examples
    -Da="b c" -Da=$(v) a" "b"c"d # legacy
    "-Da=\"b c\"" "-Da=$(v)" "a\" \"b\"c\"d" # quoted equivalents
)
unset(arg_examples)

# Control construct example.
if(a EQUAL 5 AND (s STREQUAL "${x}" OR s MATCHES [[\*]]))
endif()

