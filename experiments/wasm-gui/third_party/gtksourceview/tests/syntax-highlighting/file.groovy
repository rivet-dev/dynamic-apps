#!/usr/bin/env groovy

// Single-line comment. TODO markers work.
/* Multi-line comment.
   TODO markers work. */

'line' \
    + 'continuation'

def tsq = '''Triple single-quoted string.

    Escapes work: \' \n \123 \u12ab \
    Interpolation doesn't work: $x ${x}
'''

def sq = 'Single-quoted string. Escapes work: \' \r \45 \u45CD \
    Interpolation does not: $x ${x}'

def tdq = """Triple double-quoted string.

    Escapes work: \" \f \6 \uABCD \
    Interpolation works: $x._y.z0 ${x + 5}
"""

def dq = "Double-quoted string. Escapes work: \" \b \7 \u5678 \
    So does interpolation: $_abc1 ${x + "abc"}"

def slashy = /Slashy string.

    There are only two escape sequences: \/ \
    Interpolation works: $X ${-> X}
    Dollars $ and backslashes \ on their own are interpreted literally./

def notSlashy = 1 /2/ 3 // not a slashy string; just two division operators

def dollarSlashy = $/Dollar slashy string.

    There are three escape sequences: $$ $/ \
    Interpolation works: $_ ${x.collect { it + '\n' }.join('')}
    Dollars $ and backslashes \ on their own are interpreted literally./$

0b10i + 0b0110_1011 // binary numbers
0 + 123L + 456_789 // decimal numbers
0xab + 0xcd_efG // hexadecimal numbers

01_23.45_67g + 1e-23f + 2d + 08.80 // floating-point numbers

trait Test {
    void foo(List list) {
        def sum = 0
        for (n in list) sum += n as int
        println sum
    }
}

