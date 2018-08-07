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
RANDOM       0.036401       8.791189        
LEGACY       1.170745       8.173529        
COVER       60.841038       10.652243        8          1298
COVER       7.026512       10.652243        8          1298
FAST15       15.913632       10.555630        8          1874
FAST15       0.105652       10.555630        8          1874
FAST16       15.256371       10.701698        8          1106
FAST16       0.108564       10.701698        8          1106
FAST17       16.099808       10.650652        8          1106
FAST17       0.113188       10.650652        8          1106
FAST18       16.381432       10.499142        8          1826
FAST18       0.113615       10.499142        8          1826
FAST19       16.384702       10.527140        8          1826
FAST19       0.134103       10.527140        8          1826
FAST20       17.467039       10.494710        8          1826
FAST20       0.130395       10.494710        8          1826
FAST21       19.155774       10.503488        8          1778
FAST21       0.151328       10.503488        8          1778
FAST22       18.853681       10.509284        8          1826
FAST22       0.172543       10.509284        8          1826
FAST23       22.464652       10.509284        8          1826
FAST23       0.173671       10.509284        8          1826
FAST24       19.726143       10.512369        8          1826
FAST24       0.1876       10.512369        8          1826

hg-commands:
NODICT       0.000004       2.425291        
RANDOM       0.051193       3.490331        
LEGACY       0.902193       3.911682        
COVER       69.621755       4.132653        8          386
COVER       2.658960       4.132653        8          386
FAST15       17.436497       3.920720        6          1106
FAST15       0.14162       3.920720        6          1106
FAST16       16.735218       4.033306        8          674
FAST16       0.158825       4.033306        8          674
FAST17       16.861536       4.064132        8          1490
FAST17       0.159329       4.064132        8          1490
FAST18       16.502174       4.086714        8          290
FAST18       0.155131       4.086714        8          290
FAST19       17.118259       4.097947        8          578
FAST19       0.157701       4.097947        8          578
FAST20       20.822489       4.102851        8          434
FAST20       0.288611       4.102851        8          434
FAST21       23.115237       4.105350        8          530
FAST21       0.271089       4.105350        8          530
FAST22       26.786486       4.104100        8          530
FAST22       0.285098       4.104100        8          530
FAST23       27.285890       4.098110        8          914
FAST23       0.347279       4.098110        8          914
FAST24       28.612800       4.117367        8          722
FAST24       0.340522       4.117367        8          722

hg-changelog:
NODICT       0.000005       1.377613        
RANDOM       0.249451       2.097487        
LEGACY       2.606791       2.058907        
COVER       211.170746       2.189685        8          98
COVER       6.645103       2.189685        8          98
FAST15       63.987210       2.130794        6          386
FAST15       0.533923       2.130794        6          386
FAST16       62.935990       2.144845        8          194
FAST16       0.536322       2.144845        8          194
FAST17       62.304269       2.156099        8          242
FAST17       0.593555       2.156099        8          242
FAST18       65.585513       2.172439        6          98
FAST18       0.537827       2.172439        6          98
FAST19       64.517924       2.180321        6          98
FAST19       0.562431       2.180321        6          98
FAST20       69.756976       2.187431        6          98
FAST20       0.63555       2.187431        6          98
FAST21       76.074263       2.184185        6          146
FAST21       0.695889       2.184185        6          146
FAST22       80.143248       2.182830        6          98
FAST22       0.764593       2.182830        6          98
FAST23       80.930676       2.186399        8          98
FAST23       0.893793       2.186399        8          98
FAST24       91.931931       2.185608        6          98
FAST24       1.007653       2.185608        6          98

hg-manifest:
NODICT       0.000006       1.866385        
RANDOM       0.858201       2.309436        
LEGACY       10.560592       2.506977        
COVER       1007.639448       2.582528        8          434
COVER       37.979395       2.582528        8          434
FAST15       241.842290       2.392920        6          1826
FAST15       2.275914       2.392920        6          1826
FAST16       233.110305       2.480762        6          1922
FAST16       2.305798       2.480762        6          1922
FAST17       224.815156       2.548285        6          1682
FAST17       2.207005       2.548285        6          1682
FAST18       232.779565       2.567634        6          386
FAST18       1.934296       2.567634        6          386
FAST19       206.410639       2.581653        8          338
FAST19       1.843753       2.581653        8          338
FAST20       207.172472       2.586881        8          194
FAST20       1.931438       2.586881        8          194
FAST21       226.641577       2.590051        6          242
FAST21       2.476095       2.590051        6          242
FAST22       269.014639       2.591376        6          194
FAST22       2.756387       2.591376        6          194
FAST23       284.853617       2.591131        8          434
FAST23       3.401799       2.591131        8          434
FAST24       287.700141       2.591548        6          290
FAST24       3.260258       2.591548        6          290
