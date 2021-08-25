#!/bin/sh

# libcurl versions to build
ALL="7.78.0 7.77.0 7.76.1 7.70.0 7.65.0 7.60.0 7.58.0 7.50.0"

if ! test -d curl; then
    echo "no git clone found, cloning..."
    git clone https://github.com/curl/curl.git
fi

pwd=`pwd`

# iterate over all versions and build+install what's not already present
for ver in $ALL; do
    if ! test -d "build/$ver"; then
        echo "======================"
        echo "==== Build $ver ===="
        echo "======================"
        tag=`echo "curl-$ver" | tr . _`
        # change cwd into curl source tree
        cd curl
        # get our version
        git checkout -q $tag
        # generate the configure script
        ./buildconf
        # create a build directory
        mkdir $ver && cd $ver
        # configure it
        ../configure --prefix=$pwd/build/$ver --with-ssl
        # build and install
        make -sj7 && make install
        # remove the build directory again
        cd .. && rm -rf $ver
        # go back to the original cwd
        cd $pwd
    fi
done
