Benchmarking Dictionary Builder

### Permitted Argument:
Input File/Directory (in=fileName): required; file/directory used to build dictionary; if directory, will operate recursively for files inside directory; can include multiple files/directories, each following "in="

###Running Test:
make test

###Usage:
Benchmark given input files: make ARG= followed by permitted arguments

### Examples:
make ARG="in=../../lib/dictBuilder in=../../lib/compress"

###Benchmarking Result:

github:
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| random        | 0.182254      |  8.786957          |
| cover         | 34.821007     |  10.430999         |
| legacy        | 1.125494      |  8.989482          |

hg-commands
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| random        | 0.089231      |  3.489515          |
| cover         | 32.342462     |  4.030274          |
| legacy        | 1.066594      |  3.911896          |

hg-manifest
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| random        | 1.095083      |  2.309485          |
| cover         | 517.999132    |  2.575331          |
| legacy        | 10.789509     |  2.506775          |

hg-changelog
| Algorithm     | Speed(sec)    | Compression Ratio  |
| ------------- |:-------------:| ------------------:|
| random        | 0.639630      |  2.096785          |
| cover         | 121.398023    |  2.175706          |
| legacy        | 3.050893      |  2.058273          |
