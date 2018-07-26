Benchmarking Dictionary Builder

### Permitted Argument:
Input File/Directory (in=fileName): required; file/directory used to build dictionary; if directory, will operate recursively for files inside directory; can include multiple files/directories, each following "in="

###Running Test:
make test

###Usage:
Benchmark given input files: make ARG= followed by permitted arguments

### Examples:
make ARG="in=../../../lib/dictBuilder in=../../../lib/compress"

###Benchmarking Result:

github:
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ------------------|:-------------:| ------------------:|
| nodict            | 0.000004      |  2.999642          |
| random            | 0.148247      |  8.786957          |
| cover             | 56.331553     |  10.641263         |
| legacy            | 0.917595      |  8.989482          |
| fastCover(opt)    | 13.169979     |  10.215174         |
| fastCover(k=200)  | 2.692406      |  8.657219          |

hg-commands
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- |:-------------:| ------------------:|
| nodict            | 0.000007      |  2.425291          |
| random            | 0.093990      |  3.489515          |
| cover             | 58.602385     |  4.131136          |
| legacy            | 0.865683      |  3.911896          |
| fastCover(opt)    | 9.404134      |  3.977229          |
| fastCover(k=200)  | 1.037434      |  3.810326          |

hg-changelog
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- |:-------------:| ------------------:|
| nodict            | 0.000022      |  1.377613          |
| random            | 0.551539      |  2.096785          |
| cover             | 221.370056    |  2.188654          |
| legacy            | 2.405923      |  2.058273          |
| fastCover(opt)    | 49.526246     |  2.124185          |
| fastCover(k=200)  | 9.746872      |  2.114674          |

hg-manifest
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- |:-------------:| ------------------:|
| nodict            | 0.000019      |  1.866385          |
| random            | 1.083536      |  2.309485          |
| cover             | 928.894887    |  2.582597          |
| legacy            | 9.110371      |  2.506775          |
| fastCover(opt)    | 116.508270    |  2.525689          |
| fastCover(k=200)  | 12.176555     |  2.472221          |
