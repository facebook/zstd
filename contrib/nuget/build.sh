#!/bin/bash
#

#
# Create redistributable NuGet package using win32 and win64 release
# artifacts from GitHub.
#
# Requires docker.
#
# Usage: ./build.sh <zstd-version>
#

set -e

PKGNAME=libzstd.redist
NUGETDIR=${PWD}
TOPDIR=${PWD}/../..

IN_DOCKER=n
if [[ $1 == --in-docker ]]; then
    # Added by this script on the host when called inside the docker container.
    IN_DOCKER=y
    shift
fi

VERSION="$1"
if [[ -z $VERSION ]]; then
    echo "Usage: $0 <zstd-version>"
    exit 1
fi

# The NuGet version should not have the 'v' prefix
NUVERSION=${VERSION#v}

# Staging directory name
STAGENAME="stage-${VERSION}"


if [[ $IN_DOCKER == y ]]; then
    # The script calls itself from docker to do the actual nuget packaging.
    pushd /v
    echo "# On-docker: running nuget pack"
    nuget pack $STAGENAME/pkg/${PKGNAME}.nuspec -BasePath $STAGENAME/pkg -NonInteractive
    popd # /v

    exit 0
fi

# Base URL for zstd releases on github, the arch dependent suffix is appended.
BASE_URL="https://github.com/facebook/zstd/releases/download/${VERSION}/zstd-${VERSION}-"

echo "# Creating stage directory $STAGENAME for downloads and package content"

[[ -d $STAGENAME ]] || mkdir $STAGENAME
pushd $STAGENAME

# Remove previous package build attempt.
rm -rf pkg
mkdir pkg

pushd pkg
PKGDIR=$PWD

echo "# Populating package content"

sed -e "s/__VERSION__/${NUVERSION}/g" "$NUGETDIR/${PKGNAME}.nuspec" > ${PKGNAME}.nuspec

mkdir -p build/native/include
mkdir -p build/native/lib/win/x86
mkdir -p build/native/lib/win/x64
mkdir -p runtimes/win-x86/native
mkdir -p runtimes/win-x64/native

cp -v $TOPDIR/LICENSE LICENSE.txt  # NuGet.org needs a file extension
cp -v $NUGETDIR/${PKGNAME}.props build/
cp -v $NUGETDIR/${PKGNAME}.targets build/native/

popd # pkg


# Download directory (not cleaned)
mkdir -p dl
pushd dl
DLDIR="$PWD"

echo "# Downloading prebuilt zips from ${BASE_URL}.."
[[ -f x86.zip ]] || curl -Ls -o x86.zip "${BASE_URL}win32.zip"
[[ -f x64.zip ]] || curl -Ls -o x64.zip "${BASE_URL}win64.zip"

for arch in x86 x64 ; do
    echo "# Populating arch $arch content"
    rm -rf $arch
    mkdir $arch

    unzip ${arch}.zip -d $arch

    # libzstd_static.lib is built with mingw and not compatible with MSVC
    #cp -v ${arch}/static/libzstd_static.lib "${PKGDIR}/build/native/lib/win/${arch}/libzstd.lib"

    cp -v ${arch}/dll/libzstd.{lib,def} "${PKGDIR}/build/native/lib/win/${arch}/"
    cp -v ${arch}/dll/libzstd.dll "${PKGDIR}/runtimes/win-${arch}/native/"
    cp -v ${arch}/example/zstd_errors.h "${PKGDIR}/build/native/include/"
    cp -v ${arch}/include/*.h "${PKGDIR}/build/native/include/"
done

popd # dl
popd # $STAGENAME


echo "# Running nuget pack in docker"
if ! docker run -v "${PWD}:/v" mono:latest /v/$(basename $0) --in-docker "$VERSION" ; then
    echo "# Packaging of $VERSION failed"
    exit 1
else
    echo "# Packaging of $VERSION succeeded"
fi
