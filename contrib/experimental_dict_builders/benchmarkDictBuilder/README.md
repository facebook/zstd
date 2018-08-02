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
RANDOM       0.029508       8.791189        
LEGACY       0.862424       8.173529        
COVER       53.564142       10.652243        8          1298
COVER       6.403745       10.652243        8          1298
FAST15       12.183161       10.428151        8          1682
FAST15       0.077605       10.428151        8          1682
FAST16       12.008940       10.381748        6          1106
FAST16       0.081883       10.381748        6          1106
FAST17       12.517070       10.414438        6          1202
FAST17       0.076787       10.414438        6          1202
FAST18       12.625680       10.414975        6          1202
FAST18       0.088505       10.414975        6          1202
FAST19       13.614024       10.362817        6          1106
FAST19       0.135561       10.362817        6          1106
FAST20       14.575482       10.365472        6          1106
FAST20       0.122024       10.365472        6          1106
FAST21       16.025613       10.501292        8          1202
FAST21       0.202391       10.501292        8          1202
FAST22       16.770895       10.463314        8          1202
FAST22       0.151647       10.463314        8          1202
FAST23       17.324357       10.463314        8          1202
FAST23       0.169822       10.463314        8          1202
FAST24       18.605195       10.463314        8          1202
FAST24       0.199490       10.463314        8          1202

hg-commands:
NODICT       0.000023       2.425291        
RANDOM       0.052923       3.490331        
LEGACY       0.989280       3.911682        
COVER       66.037644       4.132653        8          386
COVER       2.436799       4.132653        8          386
FAST15       10.991971       3.373688        6          1970
FAST15       0.087334       3.373688        6          1970
FAST16       10.975682       3.357885        6          1970
FAST16       0.086707       3.357885        6          1970
FAST17       10.872008       3.365458        6          1970
FAST17       0.095128       3.365458        6          1970
FAST18       11.265688       3.345901        6          1778
FAST18       0.099890       3.345901        6          1778
FAST19       12.219262       3.344692        6          1778
FAST19       0.112674       3.344692        6          1778
FAST20       13.945985       3.344834        6          1778
FAST20       0.141736       3.344834        6          1778
FAST21       16.456103       3.332099        6          1778
FAST21       0.165591       3.332099        6          1778
FAST22       17.849867       3.332099        6          1778
FAST22       0.190531       3.332099        6          1778
FAST23       20.121578       3.332099        6          1778
FAST23       0.219091       3.332099        6          1778
FAST24       21.525724       3.332282        6          1778
FAST24       0.247244       3.332282        6          1778

hg-changelog:
NODICT       0.000006       1.377613        
RANDOM       0.231405       2.097487        
LEGACY       2.250898       2.058907        
COVER       198.748340       2.189685        8          98
COVER       5.764088       2.189685        8          98
FAST15       49.313614       2.074851        6          1538
FAST15       0.350122       2.074851        6          1538
FAST16       49.586975       2.074380        6          1922
FAST16       0.361911       2.074380        6          1922
FAST17       51.615849       2.072383        6          1922
FAST17       0.323552       2.072383        6          1922
FAST18       45.985266       2.071508        6          1970
FAST18       0.334584       2.071508        6          1970
FAST19       48.574026       2.072184        6          1922
FAST19       0.410501       2.072184        6          1922
FAST20       54.599370       2.072745        6          1922
FAST20       0.444329       2.072745        6          1922
FAST21       60.192792       2.074229        6          1922
FAST21       0.598984       2.074229        6          1922
FAST22       69.194183       2.074870        6          1922
FAST22       0.589217       2.074870        6          1922
FAST23       72.943549       2.075175        6          1922
FAST23       0.754455       2.075175        6          1922
FAST24       75.069615       2.074168        6          1970
FAST24       0.727424       2.074168        6          1970

hg-manifest:
NODICT       0.000005       1.866385        
RANDOM       0.802322       2.309436        
LEGACY       8.881489       2.506977        
COVER       963.817531       2.582528        8          434
COVER       44.422698       2.582528        8          434
FAST15       192.735162       2.182093        6          1970
FAST15       1.690944       2.182093        6          1970
FAST16       187.770054       2.189460        6          1970
FAST16       1.346101       2.189460        6          1970
FAST17       174.168035       2.182944        6          1970
FAST17       1.549590       2.182944        6          1970
FAST18       176.782976       2.182217        6          1970
FAST18       1.294935       2.182217        6          1970
FAST19       205.344302       2.179650        6          1970
FAST19       1.782814       2.179650        6          1970
FAST20       222.806737       2.177064        6          1970
FAST20       2.057328       2.177064        6          1970
FAST21       277.655343       2.173649        6          1874
FAST21       2.592156       2.173649        6          1874
FAST22       289.195991       2.173027        6          1874
FAST22       2.994468       2.173027        6          1874
FAST23       280.640301       2.173811        6          1874
FAST23       2.887603       2.173811        6          1874
FAST24       304.420667       2.173853        6          1874
FAST24       3.617361       2.173853        6          1874
