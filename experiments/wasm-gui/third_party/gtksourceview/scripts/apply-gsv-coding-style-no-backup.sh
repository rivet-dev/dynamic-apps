#!/usr/bin/env bash

# Attention, this script modifies the input file directly without doing a
# backup first! Ensure that your changes are saved in a git commit before
# running this script.
#
# The script can modify only one file, to run the script on multiple files GNU
# Parallel is recommended:
# ls *.c | parallel ./apply-gsv-coding-style-no-backup.sh
#
# You need gcu-lineup-parameters from:
# https://github.com/swilmet/gnome-c-utils
#
# Aligning function prototypes is not working yet, so avoid headers, only *.c
# files can be processed.
#
# The uncrustify config file is not perfect, some options can be tuned over
# time when using this script and seeing a problem (or the source code can be
# changed to avoid the problem).
#
# This script has not (yet) been run on the GtkSourceView *.c files, it can
# serve more as a guideline, especially for new files.

input_file=$1

uncrustify --replace --no-backup -c gtksourceview-uncrustify.cfg $input_file
gcu-lineup-parameters --tabs $input_file
