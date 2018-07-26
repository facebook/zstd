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
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000004      |  2.999642          |
| random        | 0.135459      |  8.786957          |
| cover         | 50.341079     |  10.641263         |
| legacy        | 0.866283      |  8.989482          |
| fastCover     | 13.450947     |  10.215174         |

hg-commands
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000020      |  2.425291          |
| random        | 0.088828      |  3.489515          |
| cover         | 60.028672     |  4.131136          |
| legacy        | 0.852481      |  3.911896          |
| fastCover     | 9.524284      |  3.977229          |

hg-changelog
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000004      |  1.377613          |
| random        | 0.621812      |  2.096785          |
| cover         | 217.510962    |  2.188654          |
| legacy        | 2.559194      |  2.058273          |
| fastCover     | 51.132516     |  2.124185          |

hg-manifest
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000005      |  1.866385          |
| random        | 1.035220      |  2.309485          |
| cover         | 930.480173    |  2.582597          |
| legacy        | 8.916513      |  2.506775          |
| fastCover     | 116.871089    |  2.525689          |
