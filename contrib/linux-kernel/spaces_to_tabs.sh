#!/bin/sh
set -e

# Constants
INCLUDE='include/'
LIB='lib/'
SPACES='    '
TAB=$'\t'
TMP="replacements.tmp"

echo "Files: " $INCLUDE* $LIB*

# Check files for existing tabs
grep "$TAB" $INCLUDE* $LIB* && exit 1 || true
# Replace the first tab on every line
sed -i '' "s/^$SPACES/$TAB/" $INCLUDE* $LIB*

# Execute once and then execute as long as replacements are happening
more_work="yes"
while [ ! -z "$more_work" ]
do
  rm -f $TMP
  # Replaces $SPACES that directly follow a $TAB with a $TAB.
  # $TMP will be non-empty if any replacements took place.
  sed -i '' "s/$TAB$SPACES/$TAB$TAB/w $TMP" $INCLUDE* $LIB*
  more_work=$(cat "$TMP")
done
rm -f $TMP
