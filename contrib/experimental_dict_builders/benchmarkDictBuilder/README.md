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
NODICT       0.000004       2.999642        
RANDOM       0.033395       8.791189        
LEGACY       0.933930       8.173529        
COVER       54.284688       10.652243        8          1298
COVER       5.748026       10.652243        8          1298
FAST15       27.530489       10.749143        8          1634
FAST15       5.388103       10.749143        8          1634
FAST16       25.839250       10.438346        8          1730
FAST16       4.788438       10.438346        8          1730
FAST17       25.782240       10.704193        8          1106
FAST17       5.348653       10.704193        8          1106
FAST18       25.613290       10.584171        8          1682
FAST18       5.415339       10.584171        8          1682
FAST19       26.089359       10.576229        8          1682
FAST19       4.805235       10.576229        8          1682
FAST20       28.432791       10.593114        8          1682
FAST20       5.447654       10.593114        8          1682
FAST21       29.196669       10.601757        8          1682
FAST21       5.455557       10.601757        8          1682
FAST22       29.914479       10.509284        8          1826
FAST22       5.458887       10.509284        8          1826
FAST23       28.993762       10.509284        8          1826
FAST23       5.488758       10.509284        8          1826
FAST24       31.673093       10.512369        8          1826
FAST24       5.455455       10.512369        8          1826

hg-commands:
NODICT       0.000008       2.425291        
RANDOM       0.053563       3.490331        
LEGACY       0.958894       3.911682        
COVER       62.825191       4.132653        8          386
COVER       2.481977       4.132653        8          386
FAST15       19.408943       3.927124        6          1922
FAST15       2.201625       3.927124        6          1922
FAST16       19.256022       4.003796        8          1346
FAST16       1.739605       4.003796        8          1346
FAST17       19.528058       4.063358        8          626
FAST17       1.911837       4.063358        8          626
FAST18       20.066206       4.087558        8          770
FAST18       1.925552       4.087558        8          770
FAST19       21.924301       4.094840        8          1922
FAST19       1.978983       4.094840        8          1922
FAST20       22.916487       4.108142        8          434
FAST20       1.833808       4.108142        8          434
FAST21       24.216695       4.108481        8          1010
FAST21       2.064244       4.108481        8          1010
FAST22       27.838891       4.104100        8          530
FAST22       2.035397       4.104100        8          530
FAST23       28.988792       4.104385        8          434
FAST23       2.097038       4.104385        8          434
FAST24       30.583537       4.117367        8          722
FAST24       2.100727       4.117367        8          722

hg-changelog:
NODICT       0.000004       1.377613        
RANDOM       0.216047       2.097487        
LEGACY       2.061015       2.058907        
COVER       197.100429       2.189685        8          98
COVER       6.097570       2.189685        8          98
FAST15       68.519100       2.125422        6          674
FAST15       4.773113       2.125422        6          674
FAST16       68.122491       2.141879        8          194
FAST16       4.552212       2.141879        8          194
FAST17       67.996498       2.157691        6          242
FAST17       4.514087       2.157691        6          242
FAST18       69.549023       2.173130        8          98
FAST18       4.564223       2.173130        8          98
FAST19       73.302230       2.180188        6          98
FAST19       5.172977       2.180188        6          98
FAST20       81.305310       2.187431        6          98
FAST20       3.824888       2.187431        6          98
FAST21       66.536173       2.184185        6          146
FAST21       3.605864       2.184185        6          146
FAST22       74.635575       2.182830        6          98
FAST22       3.725359       2.182830        6          98
FAST23       80.262936       2.186399        8          98
FAST23       3.724049       2.186399        8          98
FAST24       83.743478       2.185608        6          98
FAST24       4.077438       2.185608        6          98

hg-manifest:
NODICT       0.000024       1.866385        
RANDOM       0.703984       2.309436        
LEGACY       7.816456       2.506977        
COVER       852.800963       2.582528        8          434
COVER       32.613303       2.582528        8          434
FAST15       243.352076       2.394883        8          1778
FAST15       21.780946       2.394883        8          1778
FAST16       227.875094       2.479973        8          1874
FAST16       23.826866       2.479973        8          1874
FAST17       231.673469       2.537074        6          1634
FAST17       25.173464       2.537074        6          1634
FAST18       231.401979       2.571808        6          530
FAST18       25.433425       2.571808        6          530
FAST19       233.619103       2.588829        8          338
FAST19       21.549183       2.588829        8          338
FAST20       352.232030       2.586909        6          194
FAST20       24.032556       2.586909        6          194
FAST21       259.886179       2.589535        6          242
FAST21       24.958938       2.589535        6          242
FAST22       317.003494       2.591376        6          194
FAST22       26.193992       2.591376        6          194
FAST23       341.971793       2.591131        8          434
FAST23       25.375493       2.591131        8          434
FAST24       425.051849       2.591400        6          386
FAST24       34.07906       2.591400        6          386
