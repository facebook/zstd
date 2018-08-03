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
RANDOM       0.034775       8.791189        
LEGACY       0.951949       8.173529        
COVER       65.460598       10.652243        8          1298
COVER       6.408153       10.652243        8          1298
FAST15       10.716062       10.555630        8          1874
FAST15       0.081547       10.555630        8          1874
FAST16       11.277492       10.573316        8          1970
FAST16       0.067094       10.573316        8          1970
FAST17       11.743516       10.504682        6          1010
FAST17       0.091332       10.504682        6          1010
FAST18       12.435327       10.511970        8          1826
FAST18       0.126692       10.511970        8          1826
FAST19       13.205875       10.516254        8          1826
FAST19       0.120919       10.516254        8          1826
FAST20       13.524575       10.504785        8          1826
FAST20       0.121456       10.504785        8          1826
FAST21       14.491768       10.503488        8          1778
FAST21       0.129342       10.503488        8          1778
FAST22       14.720676       10.509284        8          1826
FAST22       0.155289       10.509284        8          1826
FAST23       15.616741       10.509284        8          1826
FAST23       0.165583       10.509284        8          1826
FAST24       17.217058       10.512369        8          1826
FAST24       0.188876       10.512369        8          1826

hg-commands:
NODICT       0.000017       2.425291        
RANDOM       0.048324       3.490331        
LEGACY       0.936428       3.911682        
COVER       68.933586       4.132653        8          386
COVER       2.928002       4.132653        8          386
FAST15       12.455327       3.908185        6          1106
FAST15       0.102423       3.908185        6          1106
FAST16       11.951092       4.010290        8          914
FAST16       0.085439       4.010290        8          914
FAST17       11.572172       4.061024        8          290
FAST17       0.097268       4.061024        8          290
FAST18       12.100551       4.087554        8          770
FAST18       0.097468       4.087554        8          770
FAST19       12.521838       4.099508        8          1058
FAST19       0.105444       4.099508        8          1058
FAST20       15.037724       4.103519        8          434
FAST20       0.134077       4.103519        8          434
FAST21       17.148518       4.105373        8          530
FAST21       0.188644       4.105373        8          530
FAST22       17.170767       4.110622        8          434
FAST22       0.227894       4.110622        8          434
FAST23       20.698886       4.098356        8          578
FAST23       0.257118       4.098356        8          578
FAST24       23.654759       4.103929        8          722
FAST24       0.297979       4.103929        8          722

hg-changelog:
NODICT       0.000009       1.377613        
RANDOM       0.231264       2.097487        
LEGACY       2.709644       2.058907        
COVER       223.954798       2.189685        8          98
COVER       6.749304       2.189685        8          98
FAST15       49.851760       2.129426        6          482
FAST15       0.416369       2.129426        6          482
FAST16       54.599154       2.141731        8          242
FAST16       0.288149       2.141731        8          242
FAST17       50.559812       2.155765        6          242
FAST17       0.349654       2.155765        6          242
FAST18       49.613182       2.169202        6          146
FAST18       0.443793       2.169202        6          146
FAST19       49.463356       2.177108        6          194
FAST19       0.361805       2.177108        6          194
FAST20       61.357828       2.183945        6          146
FAST20       0.408644       2.183945        6          146
FAST21       66.718019       2.185633        6          98
FAST21       0.589945       2.185633        6          98
FAST22       65.846702       2.187456        6          98
FAST22       0.633373       2.187456        6          98
FAST23       70.548251       2.186028        6          98
FAST23       0.785499       2.186028        6          98
FAST24       75.874001       2.185028        6          98
FAST24       0.724305       2.185028        6          98

hg-manifest:
NODICT       0.000021       1.866385        
RANDOM       0.812247       2.309436        
LEGACY       10.375994       2.506977        
COVER       957.674828       2.582528        8          434
COVER       35.199348       2.582528        8          434
FAST15       142.722051       2.397983        6          1826
FAST15       1.079451       2.397983        6          1826
FAST16       139.522516       2.487198        6          1922
FAST16       1.103527       2.487198        6          1922
FAST17       140.232079       2.548285        6          1682
FAST17       1.113939       2.548285        6          1682
FAST18       147.138949       2.572578        6          386
FAST18       1.101858       2.572578        6          386
FAST19       142.958995       2.583685        6          290
FAST19       1.144980       2.583685        6          290
FAST20       163.092079       2.590109        6          194
FAST20       1.054777       2.590109        6          194
FAST21       171.837207       2.587789        6          194
FAST21       1.733065       2.587789        6          194
FAST22       211.771507       2.592430        6          194
FAST22       2.359623       2.592430        6          194
FAST23       249.520542       2.593717        6          194
FAST23       2.557949       2.593717        6          194
FAST24       269.097876       2.589110        6          50
FAST24       2.841894       2.589110        6          50
