Builds a NuGet package using the Windows release artifacts
from https://github.com/facebook/zstd/releases

Usage:

    # 1. Build the package (requires docker)
    $ ./build.sh v1.3.8

    # 2. Check that package looks okay
    $ unzip -l libzstd.redist.1.3.8.nupkg

    # 3. Upload to https://NuGet.org (via web UI or with 'nuget push')
