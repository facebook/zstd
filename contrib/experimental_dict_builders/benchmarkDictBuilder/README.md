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

d=8
f=23
freq[i] = 0 when dmer added to best segment

github:
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- | ------------- | ------------------ |
| nodict            | 0.000007      |  2.999642          |
| random            | 0.150258      |  8.786957          |
| cover             | 60.388853     |  10.641263         |
| legacy            | 0.965050      |  8.989482          |
| fastCover(opt)    | 84.968131     |  10.614747         |
| fastCover(k=200)  | 6.465490      |  9.484150          |

hg-commands
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- | ------------- | ------------------ |
| nodict            | 0.000005      |  2.425291          |
| random            | 0.084348      |  3.489515          |
| cover             | 60.144894     |  4.131136          |
| legacy            | 0.831981      |  3.911896          |
| fastCover(opt)    | 59.030437     |  4.157595          |
| fastCover(k=200)  | 3.702932      |  4.134222          |

hg-changelog
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- | ------------- | ------------------ |
| nodict            | 0.000004      |  1.377613          |
| random            | 0.555964      |  2.096785          |
| cover             | 214.423753    |  2.188654          |
| legacy            | 2.180249      |  2.058273          |
| fastCover(opt)    | 102.261452    |  2.180347          |
| fastCover(k=200)  | 11.81039      |  2.170673          |

hg-manifest
| Algorithm         | Speed(sec)    | Compression Ratio  |
| ----------------- | ------------- | ------------------ |
| nodict            | 0.000006      |  1.866385          |
| random            | 1.063974      |  2.309485          |
| cover             | 909.101849    |  2.582597          |
| legacy            | 8.706580      |  2.506775          |
| fastCover(opt)    | 188.598079    |  2.596761          |
| fastCover(k=200)  | 13.392734     |  2.592985          |
