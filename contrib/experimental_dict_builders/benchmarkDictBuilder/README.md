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
NODICT       0.000007       2.999642        
RANDOM       0.032431       8.791189        
LEGACY       0.954963       8.173529        
COVER       61.835446       10.652243        8          1298
COVER       5.992098       10.652243        8          1298
FAST15       9.642805       10.555630        8          1874
FAST15       0.065641       10.555630        8          1874
FAST16       10.029785       10.573316        8          1970
FAST16       0.065992       10.573316        8          1970
FAST17       9.948889       10.504682        6          1010
FAST17       0.067814       10.504682        6          1010
FAST18       9.652888       10.511970        8          1826
FAST18       0.072837       10.511970        8          1826
FAST19       10.972048       10.516254        8          1826
FAST19       0.088646       10.516254        8          1826
FAST20       12.790908       10.504785        8          1826
FAST20       0.102836       10.504785        8          1826
FAST21       12.428766       10.503488        8          1778
FAST21       0.108829       10.503488        8          1778
FAST22       12.993776       10.509284        8          1826
FAST22       0.119060       10.509284        8          1826
FAST23       13.336864       10.509284        8          1826
FAST23       0.135937       10.509284        8          1826
FAST24       14.613413       10.512369        8          1826
FAST24       0.158576       10.512369        8          1826

hg-commands:
NODICT       0.000008       2.425291        
RANDOM       0.046974       3.490331        
LEGACY       0.927354       3.911682        
COVER       63.286326       4.132653        8          386
COVER       2.667725       4.132653        8          386
FAST15       10.029098       3.908185        6          1106
FAST15       0.081623       3.908185        6          1106
FAST16       10.128101       4.010290        8          914
FAST16       0.078019       4.010290        8          914
FAST17       11.686405       4.061024        8          290
FAST17       0.095453       4.061024        8          290
FAST18       12.436533       4.087554        8          770
FAST18       0.093102       4.087554        8          770
FAST19       12.486420       4.099508        8          1058
FAST19       0.110195       4.099508        8          1058
FAST20       16.681183       4.103519        8          434
FAST20       0.146332       4.103519        8          434
FAST21       17.976174       4.105373        8          530
FAST21       0.193156       4.105373        8          530
FAST22       20.203772       4.110622        8          434
FAST22       0.221232       4.110622        8          434
FAST23       21.612351       4.098356        8          578
FAST23       0.231875       4.098356        8          578
FAST24       25.100098       4.103929        8          722
FAST24       0.297470       4.103929        8          722

hg-changelog:
NODICT       0.000005       1.377613        
RANDOM       0.244289       2.097487        
LEGACY       2.404478       2.058907        
COVER       217.327828       2.189685        8          98
COVER       6.656336       2.189685        8          98
FAST15       46.879804       2.129426        6          482
FAST15       0.314572       2.129426        6          482
FAST16       49.211204       2.141731        8          242
FAST16       0.327231       2.141731        8          242
FAST17       47.177713       2.155765        6          242
FAST17       0.333980       2.155765        6          242
FAST18       48.518095       2.169202        6          146
FAST18       0.302470       2.169202        6          146
FAST19       46.871694       2.177108        6          194
FAST19       0.370482       2.177108        6          194
FAST20       57.205946       2.183945        6          146
FAST20       0.420422       2.183945        6          146
FAST21       65.629860       2.185633        6          98
FAST21       0.556744       2.185633        6          98
FAST22       74.113986       2.187456        6          98
FAST22       0.702334       2.187456        6          98
FAST23       78.799286       2.186028        6          98
FAST23       0.676309       2.186028        6          98
FAST24       70.068654       2.185028        6          98
FAST24       0.623832       2.185028        6          98

hg-manifest:
NODICT       0.000017       1.866385        
RANDOM       0.860279       2.309436        
LEGACY       9.894060       2.506977        
COVER       1032.794396       2.582528        8          434
COVER       42.499772       2.582528        8          434
FAST15       161.924950       2.397983        6          1826
FAST15       1.385221       2.397983        6          1826
FAST16       176.531842       2.487198        6          1922
FAST16       1.265498       2.487198        6          1922
FAST17       159.130813       2.548285        6          1682
FAST17       1.300055       2.548285        6          1682
FAST18       152.818509       2.572578        6          386
FAST18       1.076773       2.572578        6          386
FAST19       154.295759       2.583685        6          290
FAST19       1.384673       2.583685        6          290
FAST20       201.680347       2.590109        6          194
FAST20       1.789935       2.590109        6          194
FAST21       233.403457       2.587789        6          194
FAST21       2.331004       2.587789        6          194
FAST22       251.420077       2.592430        6          194
FAST22       2.316132       2.592430        6          194
FAST23       275.118703       2.593717        6          194
FAST23       2.612162       2.593717        6          194
FAST24       299.770406       2.589110        6          50
FAST24       3.151881       2.589110        6          50
