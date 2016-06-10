scripts for automated testing of zstd
================================

#### test-zstd-versions.py - script for testing zstd interoperability between versions

This script creates `versionsTest` directory to which zstd repository is cloned.
Then all taged (released) versions of zstd are compiled.
In the following step interoperability between zstd versions is checked.


#### test-zstd-speed.py - script for testing zstd speed difference between commits

This script creates `speedTest` directory to which zstd repository is cloned.
Then it compiles all branches of zstd and performs a speed benchmark for a given list of files (the `testFileNames` parameter).
After `sleepTime` (an optional parameter, default 300 seconds) seconds the script checks repository for new commits.
If a new commit is found it's compiled and a speed benchmark for this commit is performed.
The results of the speed benchmark are compared to the previous results.
If compression or decompression speed for one of zstd levels is lower than `lowerLimit` (an optional parameter, default 0.98) the speed benchmark is restarted.
If second results are lower than `lowerLimit` the warning e-mail is send to recipients from the list (the `emails` parameter).
The speed benchmark is not performed if computers' load average is higher than `maxLoadAvg` (an optional parameter, default 0.75).

The example usage:
```./test-zstd-speed.py "silesia.tar calgary.tar" "email@gmail.com" --message "tested on my laptop" --sleepTime 60``` 

The full list of parameters:
```
positional arguments:
  testFileNames         file names list for speed benchmark
  emails                list of e-mail addresses to send warnings

optional arguments:
  -h, --help            show this help message and exit
  --message MESSAGE     attach an additional message to e-mail
  --lowerLimit LOWERLIMIT
                        send email if speed is lower than given limit
  --maxLoadAvg MAXLOADAVG
                        maximum load average to start testing
  --lastCLevel LASTCLEVEL
                        last compression level for testing
  --sleepTime SLEEPTIME
                        frequency of repository checking in seconds
```
