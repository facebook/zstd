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
NODICT       0.000006       2.999642        
RANDOM       0.038199       8.791189        
LEGACY       1.067144       8.173529        
COVER       62.686485       10.652243        8          1298
COVER       6.110430       10.652243        8          1298
FAST15       10.604295       10.555630        8          1874
FAST15       0.082868       10.555630        8          1874
FAST16       10.984899       10.701698        8          1106
FAST16       0.076512       10.701698        8          1106
FAST17       9.825107       10.650652        8          1106
FAST17       0.062119       10.650652        8          1106
FAST18       11.500016       10.499142        8          1826
FAST18       0.073187       10.499142        8          1826
FAST19       11.722691       10.527140        8          1826
FAST19       0.079957       10.527140        8          1826
FAST20       11.388957       10.494710        8          1826
FAST20       0.092578       10.494710        8          1826
FAST21       12.562260       10.503488        8          1778
FAST21       0.105245       10.503488        8          1778
FAST22       13.559575       10.509284        8          1826
FAST22       0.152513       10.509284        8          1826
FAST23       13.564919       10.509284        8          1826
FAST23       0.144657       10.509284        8          1826
FAST24       14.980042       10.512369        8          1826
FAST24       0.153242       10.512369        8          1826

hg-commands:
NODICT       0.000010       2.425291        
RANDOM       0.055585       3.490331        
LEGACY       0.982606       3.911682        
COVER       65.797717       4.132653        8          386
COVER       2.871716       4.132653        8          386
FAST15       10.917014       3.920720        6          1106
FAST15       0.088387       3.920720        6          1106
FAST16       12.123582       4.033306        8          674
FAST16       0.096843       4.033306        8          674
FAST17       11.664658       4.064132        8          1490
FAST17       0.096531       4.064132        8          1490
FAST18       12.681117       4.086714        8          290
FAST18       0.091455       4.086714        8          290
FAST19       14.494563       4.097947        8          578
FAST19       0.102687       4.097947        8          578
FAST20       16.096538       4.102851        8          434
FAST20       0.148272       4.102851        8          434
FAST21       17.203949       4.105350        8          530
FAST21       0.195910       4.105350        8          530
FAST22       19.776252       4.104100        8          530
FAST22       0.222126       4.104100        8          530
FAST23       20.954823       4.098110        8          914
FAST23       0.249742       4.098110        8          914
FAST24       23.153524       4.117367        8          722
FAST24       0.288504       4.117367        8          722

hg-changelog:
NODICT       0.000017       1.377613        
RANDOM       0.241059       2.097487        
LEGACY       2.225770       2.058907        
COVER       200.827791       2.189685        8          98
COVER       5.970501       2.189685        8          98
FAST15       45.477762       2.130794        6          386
FAST15       0.294055       2.130794        6          386
FAST16       44.421192       2.144845        8          194
FAST16       0.295201       2.144845        8          194
FAST17       52.067324       2.156099        8          242
FAST17       0.370465       2.156099        8          242
FAST18       51.808365       2.172439        6          98
FAST18       0.374856       2.172439        6          98
FAST19       51.016189       2.180321        6          98
FAST19       0.394039       2.180321        6          98
FAST20       63.398856       2.187431        6          98
FAST20       0.628999       2.187431        6          98
FAST21       68.821998       2.184185        6          146
FAST21       0.604191       2.184185        6          146
FAST22       71.742843       2.182830        6          98
FAST22       0.587611       2.182830        6          98
FAST23       76.527495       2.186399        8          98
FAST23       0.684086       2.186399        8          98
FAST24       80.295936       2.185608        6          98
FAST24       0.727660       2.185608        6          98

hg-manifest:
NODICT       0.000010       1.866385        
RANDOM       0.820717       2.309436        
LEGACY       9.166893       2.506977        
COVER       1033.915846       2.582528        8          434
COVER       41.877607       2.582528        8          434
FAST15       150.624670       2.392920        6          1826
FAST15       1.115526       2.392920        6          1826
FAST16       144.426716       2.480762        6          1922
FAST16       1.103791       2.480762        6          1922
FAST17       162.035023       2.548285        6          1682
FAST17       1.586538       2.548285        6          1682
FAST18       164.238613       2.567634        6          386
FAST18       1.057999       2.567634        6          386
FAST19       162.456920       2.581653        8          338
FAST19       1.167826       2.581653        8          338
FAST20       198.143583       2.586881        8          194
FAST20       1.541616       2.586881        8          194
FAST21       239.829447       2.590051        6          242
FAST21       2.807402       2.590051        6          242
FAST22       232.878796       2.591376        6          194
FAST22       2.494001       2.591376        6          194
FAST23       250.232526       2.591131        8          434
FAST23       2.922075       2.591131        8          434
FAST24       315.514957       2.591548        6          290
FAST24       2.845419       2.591548        6          290
