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
RANDOM       0.036114       8.791189        
LEGACY       1.111024       8.173529        
COVER       57.856477       10.652243        8          1298
COVER       5.769965       10.652243        8          1298
FAST15       9.965877       10.555630        8          1874
FAST15       0.140285       10.555630        8          1874
FAST16       10.337194       10.701698        8          1106
FAST16       0.114887       10.701698        8          1106
FAST17       10.207121       10.650652        8          1106
FAST17       0.135424       10.650652        8          1106
FAST18       11.463120       10.499142        8          1826
FAST18       0.154287       10.499142        8          1826
FAST19       12.143020       10.527140        8          1826
FAST19       0.158889       10.527140        8          1826
FAST20       12.510857       10.494710        8          1826
FAST20       0.171334       10.494710        8          1826
FAST21       13.201432       10.503488        8          1778
FAST21       0.192867       10.503488        8          1778
FAST22       13.754560       10.509284        8          1826
FAST22       0.206276       10.509284        8          1826
FAST23       14.708633       10.509284        8          1826
FAST23       0.221751       10.509284        8          1826
FAST24       15.134848       10.512369        8          1826
FAST24       0.234242       10.512369        8          1826

hg-commands:
NODICT       0.000004       2.425291        
RANDOM       0.055073       3.490331        
LEGACY       0.927414       3.911682        
COVER       72.749028       4.132653        8          386
COVER       3.391066       4.132653        8          386
FAST15       10.910989       3.920720        6          1106
FAST15       0.130480       3.920720        6          1106
FAST16       10.565224       4.033306        8          674
FAST16       0.146228       4.033306        8          674
FAST17       11.394137       4.064132        8          1490
FAST17       0.175567       4.064132        8          1490
FAST18       11.040248       4.086714        8          290
FAST18       0.132692       4.086714        8          290
FAST19       11.335856       4.097947        8          578
FAST19       0.181441       4.097947        8          578
FAST20       14.166272       4.102851        8          434
FAST20       0.203632       4.102851        8          434
FAST21       15.848896       4.105350        8          530
FAST21       0.269518       4.105350        8          530
FAST22       15.570995       4.104100        8          530
FAST22       0.238512       4.104100        8          530
FAST23       17.437566       4.098110        8          914
FAST23       0.270788       4.098110        8          914
FAST24       18.836604       4.117367        8          722
FAST24       0.323618       4.117367        8          722

hg-changelog:
NODICT       0.000006       1.377613        
RANDOM       0.253393       2.097487        
LEGACY       2.410568       2.058907        
COVER       203.550681       2.189685        8          98
COVER       7.381697       2.189685        8          98
FAST15       45.960609       2.130794        6          386
FAST15       0.512057       2.130794        6          386
FAST16       44.594817       2.144845        8          194
FAST16       0.601258       2.144845        8          194
FAST17       45.852992       2.156099        8          242
FAST17       0.500844       2.156099        8          242
FAST18       46.624930       2.172439        6          98
FAST18       0.680501       2.172439        6          98
FAST19       47.754905       2.180321        6          98
FAST19       0.606180       2.180321        6          98
FAST20       56.733632       2.187431        6          98
FAST20       0.710149       2.187431        6          98
FAST21       59.723173       2.184185        6          146
FAST21       0.875562       2.184185        6          146
FAST22       66.570788       2.182830        6          98
FAST22       1.061013       2.182830        6          98
FAST23       73.817645       2.186399        8          98
FAST23       0.838496       2.186399        8          98
FAST24       78.059933       2.185608        6          98
FAST24       0.843158       2.185608        6          98

hg-manifest:
NODICT       0.000005       1.866385        
RANDOM       0.735840       2.309436        
LEGACY       9.322081       2.506977        
COVER       885.961515       2.582528        8          434
COVER       32.678552       2.582528        8          434
FAST15       114.414413       2.392920        6          1826
FAST15       1.412690       2.392920        6          1826
FAST16       113.869718       2.480762        6          1922
FAST16       1.539424       2.480762        6          1922
FAST17       113.333636       2.548285        6          1682
FAST17       1.473196       2.548285        6          1682
FAST18       111.717871       2.567634        6          386
FAST18       1.421200       2.567634        6          386
FAST19       112.428344       2.581653        8          338
FAST19       1.412185       2.581653        8          338
FAST20       128.897480       2.586881        8          194
FAST20       1.586570       2.586881        8          194
FAST21       168.465684       2.590051        6          242
FAST21       2.190732       2.590051        6          242
FAST22       202.320435       2.591376        6          194
FAST22       2.667877       2.591376        6          194
FAST23       228.952201       2.591131        8          434
FAST23       3.315501       2.591131        8          434
FAST24       327.320020       2.591548        6          290
FAST24       5.048348       2.591548        6          290
