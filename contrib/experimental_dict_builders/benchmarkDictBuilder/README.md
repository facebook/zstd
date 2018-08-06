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
RANDOM       0.028963       8.791189        
LEGACY       0.878235       8.173529        
COVER       57.673337       10.652243        8          1298
COVER       5.755193       10.652243        8          1298
FAST15       8.809097       10.604731        8          1778
FAST15       0.061001       10.604731        8          1778
FAST16       8.866052       10.556598        8          1778
FAST16       0.062848       10.556598        8          1778
FAST17       9.459054       10.643246        8          1778
FAST17       0.069457       10.643246        8          1778
FAST18       10.835864       10.710841        8          1778
FAST18       0.083213       10.710841        8          1778
FAST19       11.241659       10.716254        8          1778
FAST19       0.087187       10.716254        8          1778
FAST20       11.841620       10.586072        8          1778
FAST20       0.1018       10.586072        8          1778
FAST21       13.445770       10.599955        8          1778
FAST21       0.111163       10.599955        8          1778
FAST22       14.420178       10.552371        8          1778
FAST22       0.122112       10.552371        8          1778
FAST23       14.607778       10.529569        8          1778
FAST23       0.136831       10.529569        8          1778
FAST24       18.483461       10.535824        8          1778
FAST24       0.154349       10.535824        8          1778

hg-commands:
NODICT       0.000003       2.425291        
RANDOM       0.046695       3.490331        
LEGACY       0.870179       3.911682        
COVER       68.946283       4.132653        8          386
COVER       2.807227       4.132653        8          386
FAST15       10.704073       3.904785        6          1106
FAST15       0.08255       3.904785        6          1106
FAST16       10.435822       3.997353        8          818
FAST16       0.07998       3.997353        8          818
FAST17       10.630261       4.072528        8          914
FAST17       0.077778       4.072528        8          914
FAST18       10.457510       4.087207        8          434
FAST18       0.10404       4.087207        8          434
FAST19       12.145264       4.103834        8          290
FAST19       0.078133       4.103834        8          290
FAST20       12.784966       4.107275        8          434
FAST20       0.118039       4.107275        8          434
FAST21       15.564391       4.108752        8          530
FAST21       0.155789       4.108752        8          530
FAST22       16.843655       4.119395        8          578
FAST22       0.186655       4.119395        8          578
FAST23       20.053119       4.111750        8          434
FAST23       0.188051       4.111750        8          434
FAST24       22.292822       4.108405        8          1922
FAST24       0.271772       4.108405        8          1922

hg-changelog:
NODICT       0.000006       1.377613        
RANDOM       0.242847       2.097487        
LEGACY       2.638518       2.058907        
COVER       205.499137       2.189685        8          98
COVER       6.345072       2.189685        8          98
FAST15       42.024298       2.128669        8          290
FAST15       0.281162       2.128669        8          290
FAST16       46.981837       2.142786        8          386
FAST16       0.274282       2.142786        8          386
FAST17       43.056813       2.159819        8          290
FAST17       0.287212       2.159819        8          290
FAST18       43.874697       2.172054        6          146
FAST18       0.294031       2.172054        6          146
FAST19       45.370900       2.180226        8          98
FAST19       0.316945       2.180226        8          98
FAST20       54.416542       2.186840        6          98
FAST20       0.450304       2.186840        6          98
FAST21       58.931976       2.186098        6          98
FAST21       0.497906       2.186098        6          98
FAST22       69.338393       2.183416        6          98
FAST22       0.520307       2.183416        6          98
FAST23       67.533743       2.182496        6          98
FAST23       0.530205       2.182496        6          98
FAST24       72.429191       2.187812        8          98
FAST24       0.617281       2.187812        8          98

hg-manifest:
NODICT       0.000008       1.866385        
RANDOM       0.768242       2.309436        
LEGACY       8.976183       2.506977        
COVER       985.814521       2.582528        8          434
COVER       35.872354       2.582528        8          434
FAST15       137.816482       2.395606        8          1586
FAST15       1.012195       2.395606        8          1586
FAST16       135.364390       2.479599        8          1826
FAST16       1.058008       2.479599        8          1826
FAST17       128.874611       2.544569        8          1490
FAST17       0.888344       2.544569        8          1490
FAST18       121.613683       2.565328        6          1442
FAST18       0.905692       2.565328        6          1442
FAST19       117.455997       2.582160        6          194
FAST19       0.895677       2.582160        6          194
FAST20       142.541017       2.585248        6          290
FAST20       1.06709       2.585248        6          290
FAST21       182.028623       2.588795        8          434
FAST21       1.725953       2.588795        8          434
FAST22       215.708168       2.588428        6          194
FAST22       1.97766       2.588428        6          194
FAST23       234.192868       2.591356        6          194
FAST23       2.195121       2.591356        6          194
FAST24       270.055113       2.593055        6          50
FAST24       2.386321       2.593055        6          50
