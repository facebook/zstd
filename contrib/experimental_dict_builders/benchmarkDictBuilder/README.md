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
RANDOM       0.031365       8.791189        
LEGACY       0.918818       8.173529        
COVER       59.111864       10.652243        8          1298
COVER       5.528302       10.652243        8          1298
FAST15       8.608277       10.556256        8          1874
FAST15       0.076267       10.555630        8          1874
FAST16       9.229086       10.700948        8          1106
FAST16       0.080302       10.701698        8          1106
FAST17       9.900222       10.649151        8          1106
FAST17       0.086606       10.650652        8          1106
FAST18       10.197612       10.502707        8          1826
FAST18       0.085989       10.499142        8          1826
FAST19       10.710638       10.527288        8          1826
FAST19       0.097214       10.527140        8          1826
FAST20       11.209267       10.493033        8          1826
FAST20       0.105905       10.494710        8          1826
FAST21       12.329850       10.501705        8          1778
FAST21       0.131955       10.503488        8          1778
FAST22       12.951253       10.509063        8          1826
FAST22       0.144327       10.509284        8          1826
FAST23       13.945499       10.509063        8          1826
FAST23       0.158974       10.509284        8          1826
FAST24       15.257556       10.512989        8          1826
FAST24       0.183711       10.512369        8          1826

hg-commands:
NODICT       0.000004       2.425291        
RANDOM       0.053888       3.490331        
LEGACY       1.103126       3.911682        
COVER       65.805490       4.132653        8          386
COVER       2.846178       4.132653        8          386
FAST15       8.854412       3.917973        6          1106
FAST15       0.090181       3.920720        6          1106
FAST16       8.584094       4.028908        8          674
FAST16       0.089120       4.033306        8          674
FAST17       9.080643       4.060578        8          1490
FAST17       0.096433       4.064132        8          1490
FAST18       8.896033       4.084489        8          290
FAST18       0.097725       4.086714        8          290
FAST19       10.632111       4.093471        8          578
FAST19       0.130242       4.097947        8          578
FAST20       12.207356       4.097088        8          434
FAST20       0.140380       4.102851        8          434
FAST21       15.155902       4.100380        8          530
FAST21       0.210744       4.105350        8          530
FAST22       16.866879       4.097906        8          530
FAST22       0.233667       4.104100        8          530
FAST23       18.365883       4.092244        8          914
FAST23       0.245550       4.098110        8          914
FAST24       19.251445       4.112837        8          722
FAST24       0.261788       4.117367        8          722

hg-changelog:
NODICT       0.000005       1.377613        
RANDOM       0.248784       2.097487        
LEGACY       2.432781       2.058907        
COVER       211.364740       2.189685        8          98
COVER       6.700196       2.189685        8          98
FAST15       37.583555       2.130794        6          386
FAST15       0.294869       2.130794        6          386
FAST16       35.784461       2.144828        8          194
FAST16       0.282724       2.144845        8          194
FAST17       35.939065       2.156001        8          242
FAST17       0.315988       2.156099        8          242
FAST18       37.314221       2.172373        6          98
FAST18       0.328969       2.172439        6          98
FAST19       35.752416       2.180165        6          98
FAST19       0.312284       2.180321        6          98
FAST20       45.085289       2.187483        6          98
FAST20       0.409879       2.187431        6          98
FAST21       53.934688       2.184109        6          146
FAST21       0.580802       2.184185        6          146
FAST22       60.729265       2.182811        6          98
FAST22       0.519552       2.182830        6          98
FAST23       66.617360       2.186186        8          98
FAST23       0.674128       2.186399        8          98
FAST24       65.036263       2.185494        6          98
FAST24       0.677030       2.185608        6          98

hg-manifest:
NODICT       0.000008       1.866385        
RANDOM       1.041664       2.309436        
LEGACY       9.494490       2.506977        
COVER       998.757831       2.582528        8          434
COVER       41.675547       2.582528        8          434
FAST15       127.499380       2.392947        6          1826
FAST15       1.097875       2.392920        6          1826
FAST16       167.898543       2.480835        6          1922
FAST16       1.147568       2.480762        6          1922
FAST17       98.929065       2.548470        6          1682
FAST17       0.944189       2.548285        6          1682
FAST18       92.210661       2.567745        6          386
FAST18       0.918395       2.567634        6          386
FAST19       92.881858       2.581729        8          338
FAST19       0.903956       2.581653        8          338
FAST20       111.232710       2.586917        6          194
FAST20       1.018468       2.586861        6          194
FAST21       145.921077       2.590090        6          242
FAST21       1.581907       2.590051        6          242
FAST22       169.435018       2.591434        6          194
FAST22       2.023020       2.591376        6          194
FAST23       183.661025       2.591212        8          434
FAST23       2.313386       2.591131        8          434
FAST24       203.660068       2.591748        6          290
FAST24       2.259268       2.591548        6          290
