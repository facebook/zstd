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
NODICT       0.000025       2.999642        
RANDOM       0.030101       8.791189        
LEGACY       0.913108       8.173529        
COVER       59.234160       10.652243        8          1298
COVER       6.258459       10.652243        8          1298
FAST15       9.959246       10.555630        8          1874
FAST15       0.077719       10.555630        8          1874
FAST16       10.028343       10.701698        8          1106
FAST16       0.078117       10.701698        8          1106
FAST17       10.567355       10.650652        8          1106
FAST17       0.124833       10.650652        8          1106
FAST18       11.795287       10.499142        8          1826
FAST18       0.086992       10.499142        8          1826
FAST19       13.132451       10.527140        8          1826
FAST19       0.134716       10.527140        8          1826
FAST20       14.366314       10.494710        8          1826
FAST20       0.128844       10.494710        8          1826
FAST21       14.941238       10.503488        8          1778
FAST21       0.134975       10.503488        8          1778
FAST22       15.146226       10.509284        8          1826
FAST22       0.146918       10.509284        8          1826
FAST23       16.260552       10.509284        8          1826
FAST23       0.158494       10.509284        8          1826
FAST24       16.806037       10.512369        8          1826
FAST24       0.190464       10.512369        8          1826

hg-commands:
NODICT       0.000026       2.425291        
RANDOM       0.046270       3.490331        
LEGACY       0.847904       3.911682        
COVER       71.691804       4.132653        8          386
COVER       3.187085       4.132653        8          386
FAST15       11.593687       3.920720        6          1106
FAST15       0.082431       3.920720        6          1106
FAST16       11.775958       4.033306        8          674
FAST16       0.092587       4.033306        8          674
FAST17       11.965064       4.064132        8          1490
FAST17       0.106382       4.064132        8          1490
FAST18       11.438197       4.086714        8          290
FAST18       0.097293       4.086714        8          290
FAST19       12.292512       4.097947        8          578
FAST19       0.104406       4.097947        8          578
FAST20       13.857857       4.102851        8          434
FAST20       0.139467       4.102851        8          434
FAST21       14.599613       4.105350        8          530
FAST21       0.189416       4.105350        8          530
FAST22       15.966109       4.104100        8          530
FAST22       0.183817       4.104100        8          530
FAST23       18.033645       4.098110        8          914
FAST23       0.246641       4.098110        8          914
FAST24       22.992891       4.117367        8          722
FAST24       0.285994       4.117367        8          722

hg-changelog:
NODICT       0.000007       1.377613        
RANDOM       0.297345       2.097487        
LEGACY       2.633992       2.058907        
COVER       219.179786       2.189685        8          98
COVER       6.620852       2.189685        8          98
FAST15       47.635082       2.130794        6          386
FAST15       0.321297       2.130794        6          386
FAST16       43.837676       2.144845        8          194
FAST16       0.312640       2.144845        8          194
FAST17       49.349017       2.156099        8          242
FAST17       0.348459       2.156099        8          242
FAST18       51.153784       2.172439        6          98
FAST18       0.353106       2.172439        6          98
FAST19       52.627045       2.180321        6          98
FAST19       0.390612       2.180321        6          98
FAST20       63.748782       2.187431        6          98
FAST20       0.489544       2.187431        6          98
FAST21       68.709198       2.184185        6          146
FAST21       0.530852       2.184185        6          146
FAST22       68.491639       2.182830        6          98
FAST22       0.645699       2.182830        6          98
FAST23       72.558688       2.186399        8          98
FAST23       0.593539       2.186399        8          98
FAST24       76.137195       2.185608        6          98
FAST24       0.680132       2.185608        6          98

hg-manifest:
NODICT       0.000026       1.866385        
RANDOM       0.784554       2.309436        
LEGACY       10.193714       2.506977        
COVER       988.206583       2.582528        8          434
COVER       39.726199       2.582528        8          434
FAST15       168.388819       2.392920        6          1826
FAST15       1.272178       2.392920        6          1826
FAST16       161.822607       2.480762        6          1922
FAST16       1.164908       2.480762        6          1922
FAST17       157.688544       2.548285        6          1682
FAST17       1.222439       2.548285        6          1682
FAST18       154.529585       2.567634        6          386
FAST18       1.217596       2.567634        6          386
FAST19       160.244979       2.581653        8          338
FAST19       1.282450       2.581653        8          338
FAST20       191.503297       2.586881        8          194
FAST20       2.009748       2.586881        8          194
FAST21       226.389709       2.590051        6          242
FAST21       2.494543       2.590051        6          242
FAST22       217.859055       2.591376        6          194
FAST22       2.295693       2.591376        6          194
FAST23       236.819791       2.591131        8          434
FAST23       2.744711       2.591131        8          434
FAST24       269.187800       2.591548        6          290
FAST24       2.923671       2.591548        6          290
