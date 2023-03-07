#!/bin/sh

# use installed versions

pwd=`pwd`

cd build

ALL=`ls`;

cd $pwd
for ver in $ALL; do
    major=`echo $ver | cut -d. -f1`;
    minor=`echo $ver | cut -d. -f2`;
    SPRINTER=./sprinter
    if test "$major" = 7 && test "$minor" -lt 66; then
        SPRINTER=./sprinter-old
    fi
    if ! test -x $SPRINTER; then
        echo "$SPRINTER is missing, forget to build?"
        exit
    fi
    LD_LIBRARY_PATH=$pwd/build/$ver/lib $SPRINTER localhost/4KB 100000 10 2>/dev/null
done
