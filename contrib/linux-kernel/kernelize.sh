#!/bin/sh
set -e

# Constants
INCLUDE='include/linux/'
LIB='lib/zstd/'
SPACES='    '
TAB=$'\t'
TMP="replacements.tmp"

function prompt() {
  while true; do
    read -p "$1 [Y/n]" yn
    case $yn in
        '' ) yes='yes'; break;;
        [Yy]* ) yes='yes'; break;;
        [Nn]* ) yes=''; break;;
        * ) echo "Please answer yes or no.";;
    esac
done
}

echo "Files: " $INCLUDE*.h $LIB*.{h,c}

prompt "Do you wish to replace 4 spaces with a tab?"
if [ ! -z "$yes" ]
then
  # Check files for existing tabs
  grep "$TAB" $INCLUDE*.h $LIB*.{h,c} && exit 1 || true
  # Replace the first tab on every line
  sed -i '' "s/^$SPACES/$TAB/" $INCLUDE*.h $LIB*.{h,c}

  # Execute once and then execute as long as replacements are happening
  more_work="yes"
  while [ ! -z "$more_work" ]
  do
    rm -f $TMP
    # Replaces $SPACES that directly follow a $TAB with a $TAB.
    # $TMP will be non-empty if any replacements took place.
    sed -i '' "s/$TAB$SPACES/$TAB$TAB/w $TMP" $INCLUDE*.h $LIB*.{h,c}
    more_work=$(cat "$TMP")
  done
  rm -f $TMP
fi

prompt "Do you wish to replace '{   ' with a tab?"
if [ ! -z "$yes" ]
then
  sed -i '' "s/$TAB{   /$TAB{$TAB/g" $INCLUDE*.h $LIB*.{h,c}
fi

prompt "Do you wish to replace 'current' with 'curr'?"
if [ ! -z "$yes" ]
then
  sed -i '' "s/current/curr/g" $LIB*.{h,c}
fi
