#!/bin/sh
# "./check-style.sh files..." will validate files given on command line.
# "./check-style.sh" without arguments will validate all style files
# in the source directory

files=""

if [ $1 ]; then
  files=$@
else
  if [ "$srcdir" ]; then
    cd $srcdir
  fi

  files=*.xml
fi

for file in $files; do
  xmllint --relaxng styles.rng --noout $file || exit 1
done
