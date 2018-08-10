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
- First Cover is optimize cover, second Cover uses optimized d and k from first one.
- For every f value of fastCover, the first one is optimize fastCover and the second one uses optimized d and k from first one.
- Fourth column is chosen d and fifth column is chosen k

github:
NODICT       0.000005       2.999642        
RANDOM       0.032216       8.791189        
LEGACY       0.997206       8.173529        
COVER       57.197030       10.652243        8          1298
COVER       6.037890       10.652243        8          1298
FAST15       9.279041       10.551970        8          1874
FAST15       0.072544       10.551970        8          1874
FAST16       11.068854       10.698853        8          1106
FAST16       0.071155       10.698853        8          1106
FAST17       9.958838       10.701698        8          1106
FAST17       0.077205       10.701698        8          1106
FAST18       10.265772       10.540483        8          1874
FAST18       0.08573       10.540483        8          1874
FAST19       12.189778       10.534978        8          1874
FAST19       0.101346       10.534978        8          1874
FAST20       11.956127       10.517584        8          1874
FAST20       0.106872       10.517584        8          1874
FAST21       13.685038       10.545279        8          1874
FAST21       0.130381       10.545279        8          1874
FAST22       13.466346       10.571150        6          1010
FAST22       0.124652       10.571150        6          1010
FAST23       14.557159       10.609556        6          1010
FAST23       0.133205       10.609556        6          1010
FAST24       15.390833       10.578307        6          1010
FAST24       0.154657       10.578307        6          1010

hg-commands:
NODICT       0.000004       2.425291        
RANDOM       0.055173       3.490331        
LEGACY       1.119033       3.911682        
COVER       66.068677       4.132653        8          386
COVER       2.647083       4.132653        8          386
FAST15       10.686139       3.918340        6          818
FAST15       0.088147       3.918340        6          818
FAST16       10.009709       4.028033        8          674
FAST16       0.086777       4.028033        8          674
FAST17       10.151141       4.063987        8          1490
FAST17       0.09192       4.063987        8          1490
FAST18       10.322779       4.081631        8          290
FAST18       0.090482       4.081631        8          290
FAST19       11.467831       4.094209        8          578
FAST19       0.10344       4.094209        8          578
FAST20       13.917384       4.101742        8          434
FAST20       0.148773       4.101742        8          434
FAST21       18.547107       4.102190        8          386
FAST21       0.176412       4.102190        8          386
FAST22       20.065170       4.094027        8          530
FAST22       0.22252       4.094027        8          530
FAST23       20.968485       4.101985        8          914
FAST23       0.260389       4.101985        8          914
FAST24       24.197238       4.107716        8          722
FAST24       0.28544       4.107716        8          722

hg-changelog:
NODICT       0.000010       1.377613        
RANDOM       0.232717       2.097487        
LEGACY       2.547946       2.058907        
COVER       188.113327       2.189685        8          98
COVER       4.996037       2.189685        8          98
FAST15       36.517460       2.130503        6          386
FAST15       0.251703       2.130503        6          386
FAST16       37.607959       2.144324        8          194
FAST16       0.252138       2.144324        8          194
FAST17       37.587524       2.157976        8          242
FAST17       0.250349       2.157976        8          242
FAST18       37.677389       2.170132        8          98
FAST18       0.249257       2.170132        8          98
FAST19       36.482961       2.183067        6          98
FAST19       0.254592       2.183067        6          98
FAST20       37.486135       2.186917        6          98
FAST20       0.280784       2.186917        6          98
FAST21       48.893514       2.187126        6          98
FAST21       0.427223       2.187126        6          98
FAST22       56.268097       2.184939        6          98
FAST22       0.532105       2.184939        6          98
FAST23       61.918036       2.185458        8          98
FAST23       0.655179       2.185458        8          98
FAST24       70.974912       2.185945        6          98
FAST24       0.654632       2.185945        6          98

hg-manifest:
NODICT       0.000005       1.866385        
RANDOM       0.770976       2.309436        
LEGACY       9.184757       2.506977        
COVER       932.145045       2.582528        8          434
COVER       34.878821       2.582528        8          434
FAST15       124.488208       2.393516        6          1826
FAST15       0.965171       2.393516        6          1826
FAST16       117.697444       2.481369        6          1922
FAST16       0.951346       2.481369        6          1922
FAST17       122.024225       2.545715        6          1682
FAST17       0.933205       2.545715        6          1682
FAST18       120.169086       2.566880        6          386
FAST18       0.93231       2.566880        6          386
FAST19       124.911027       2.582884        8          338
FAST19       0.934654       2.582884        8          338
FAST20       143.476208       2.585657        8          338
FAST20       1.094095       2.585657        8          338
FAST21       193.747366       2.586104        6          530
FAST21       2.510192       2.586104        6          530
FAST22       245.151162       2.589798        6          194
FAST22       2.577126       2.589798        6          194
FAST23       246.225152       2.587877        8          386
FAST23       2.787521       2.587877        8          386
FAST24       269.382864       2.588025        6          290
FAST24       2.731522       2.588025        6          290
