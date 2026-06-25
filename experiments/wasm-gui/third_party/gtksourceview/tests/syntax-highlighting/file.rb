#!/usr/bin/env ruby
#
puts "\141" # Shows 'a'

str = 'lorem'
puts str[1..3] # 'ore'

# Escape sequences
"\a \b \t \n \v \f \r \e \s" # control characters
"\\" # backslash
"\1 \11 \111" # octal
"\xa \xA1" # hexadecimal
"\u9aBC \u{1} \u{10FFFF} \u{123 abc DEF}" # unicode
"\ca     \c"     \c\1     \c\11     \c\111     \c\xa     \c\xA1"     # control characters
"\C-a    \C-"    \C-\1    \C-\11    \C-\111    \C-\xa    \C-\xA1"    # control characters
"\M-a    \M-"    \M-\1    \M-\11    \M-\111    \M-\xa    \M-\xA1"    # meta characters
"\M-\ca  \M-\c"  \M-\c\1  \M-\c\11  \M-\c\111  \M-\c\xa  \M-\c\xA1"  # meta control characters
"\c\M-a  \c\M-"  \c\M-\1  \c\M-\11  \c\M-\111  \c\M-\xa  \c\M-\xA1"  # meta control characters
"\M-\C-a \M-\C-" \M-\C-\1 \M-\C-\11 \M-\C-\111 \M-\C-\xa \M-\C-\xA1" # meta control characters
"\C-\M-a \C-\M-" \C-\M-\1 \C-\M-\11 \C-\M-\111 \C-\M-\xa \C-\M-\xA1" # meta control characters

# Invalid escape sequences
"\xg"
"\ug \u{1234567}"
"\C \c\C-a \C-\ca \c\M-\ca \c\M-\C-a \C-\M-\ca"
"\M \M-\M-a \M-\c\M-a \M-\C-\M-a"

# Number methods
puts -11.to_s + ' ' + 0x11.to_s  + ' ' + 1.1.to_s + ' ' + ?a.to_s + ' ' + 1.x
11.1aa # not a valid method name

# Ranges
11..11, 0x11..0x11, 01..07, ?a..?f, 1.1..2.2 # range incl. the last value
11...11, 0x11...0x11, 01...07, ?\x40...?\101, 1.1...2.2 # range excl. the last value
11....111, 0x11......0x11, 1.1.....1.2 # 4 (and more) dots are not a valid range

# Character literals
str = ?\x41 + ?\101 # == 'AA'
puts ?\M-\C-x


	# xml == '\tlorem\n\tipsum'
	xml = <<-XML
	lorem
	ipsum
	XML

# xml == 'lorem\nipsum'
xml = <<XML
lorem
ipsum
XML

	# strip the leading tabs; xml == 'lorem\nipsum'
	xml = <<~XML
	lorem
	ipsum
	XML
