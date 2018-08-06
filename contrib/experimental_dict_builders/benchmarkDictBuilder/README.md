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
NODICT       0.000012       2.999642        
RANDOM       0.042863       8.791189        
LEGACY       1.119005       8.173529        
COVER       59.807233       10.652243        8          1298
COVER       6.172291       10.652243        8          1298
FAST15       8.911868       10.556256        8          1874
FAST15       0.060186       10.556256        8          1874
FAST16       9.324293       10.700948        8          1106
FAST16       0.059857       10.700948        8          1106
FAST17       9.528887       10.649151        8          1106
FAST17       0.066559       10.649151        8          1106
FAST18       10.009846       10.502707        8          1826
FAST18       0.075172       10.502707        8          1826
FAST19       12.950599       10.527288        8          1826
FAST19       0.0939       10.527288        8          1826
FAST20       12.955029       10.493033        8          1826
FAST20       0.09353       10.493033        8          1826
FAST21       11.587041       10.501705        8          1778
FAST21       0.115014       10.501705        8          1778
FAST22       12.962984       10.509063        8          1826
FAST22       0.124754       10.509063        8          1826
FAST23       13.558212       10.509063        8          1826
FAST23       0.139194       10.509063        8          1826
FAST24       14.257633       10.512989        8          1826
FAST24       0.147998       10.512989        8          1826

hg-commands:
NODICT       0.000009       2.425291        
RANDOM       0.054504       3.490331        
LEGACY       1.038284       3.911682        
COVER       68.098013       4.132653        8          386
COVER       2.791586       4.132653        8          386
FAST15       9.177752       3.917973        6          1106
FAST15       0.064786       3.917973        6          1106
FAST16       8.747899       4.028908        8          674
FAST16       0.060907       4.028908        8          674
FAST17       9.437830       4.060578        8          1490
FAST17       0.06852       4.060578        8          1490
FAST18       9.448910       4.084489        8          290
FAST18       0.068418       4.084489        8          290
FAST19       11.025557       4.093471        8          578
FAST19       0.085576       4.093471        8          578
FAST20       13.334997       4.097088        8          434
FAST20       0.12563       4.097088        8          434
FAST21       16.637669       4.100380        8          530
FAST21       0.152556       4.100380        8          530
FAST22       16.704789       4.097906        8          530
FAST22       0.191533       4.097906        8          530
FAST23       18.995269       4.092244        8          914
FAST23       0.225833       4.092244        8          914
FAST24       21.247055       4.112837        8          722
FAST24       0.262141       4.112837        8          722

hg-changelog:
NODICT       0.000005       1.377613        
RANDOM       0.224497       2.097487        
LEGACY       2.274095       2.058907        
COVER       224.285032       2.189685        8          98
COVER       7.461153       2.189685        8          98
FAST15       40.860436       2.130794        6          386
FAST15       0.217781       2.130794        6          386
FAST16       42.267307       2.144828        8          194
FAST16       0.2117       2.144828        8          194
FAST17       42.961269       2.156001        8          242
FAST17       0.199534       2.156001        8          242
FAST18       39.794237       2.172373        6          98
FAST18       0.224185       2.172373        6          98
FAST19       40.787192       2.180165        6          98
FAST19       0.221888       2.180165        6          98
FAST20       47.382007       2.187483        6          98
FAST20       0.316777       2.187483        6          98
FAST21       51.402872       2.184109        6          146
FAST21       0.455072       2.184109        6          146
FAST22       55.301835       2.182811        6          98
FAST22       0.446912       2.182811        6          98
FAST23       61.672440       2.186186        8          98
FAST23       0.572235       2.186186        8          98
FAST24       66.411245       2.185494        6          98
FAST24       0.515929       2.185494        6          98

hg-manifest:
NODICT       0.000005       1.866385        
RANDOM       0.758387       2.309436        
LEGACY       9.536460       2.506977        
COVER       931.433001       2.582528        8          434
COVER       31.054383       2.582528        8          434
FAST15       118.305096       2.392947        6          1826
FAST15       0.590765       2.392947        6          1826
FAST16       161.717797       2.480835        6          1922
FAST16       1.265018       2.480835        6          1922
FAST17       192.813027       2.548470        6          1682
FAST17       1.304805       2.548470        6          1682
FAST18       188.237682       2.567745        6          386
FAST18       1.245209       2.567745        6          386
FAST19       188.225017       2.581729        8          338
FAST19       1.25796       2.581729        8          338
FAST20       379.944277       2.586917        6          194
FAST20       1.354535       2.586917        6          194
FAST21       193.168665       2.590090        6          242
FAST21       1.883419       2.590090        6          242
FAST22       199.795401       2.591434        6          194
FAST22       2.25046       2.591434        6          194
FAST23       215.450108       2.591212        8          434
FAST23       2.115187       2.591212        8          434
FAST24       218.475588       2.591748        6          290
FAST24       2.256616       2.591748        6          290
