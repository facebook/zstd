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
| random        | 0.180238      |  8.786957          |
| cover         | 33.891987     |  10.430999         |
| legacy        | 1.077569      |  8.989482          |

hg-commands
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000006      |  2.425291          |
| random        | 0.088735      |  3.489515          |
| cover         | 35.447300     |  4.030274          |
| legacy        | 1.048509      |  3.911896          |

hg-manifest
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000005      |  1.866385          |
| random        | 1.148231      |  2.309485          |
| cover         | 509.685257    |  2.575331          |
| legacy        | 10.705866     |  2.506775          |

hg-changelog
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| nodict        | 0.000005      |  1.377613          |
| random        | 0.706434      |  2.096785          |
| cover         | 122.815783    |  2.175706          |
| legacy        | 3.010318      |  2.058273          |
