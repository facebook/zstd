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
RANDOM       0.161907       8.786957        
LEGACY       0.960128       8.989482        
COVER       69.031037       10.641263        8          1298
COVER       7.017782       10.641263        8          1298
FAST15       24.710713       10.547583        8          1874
FAST15       0.271657       10.547583        8          1874
FAST16       23.906902       10.690723        8          1106
FAST16       0.315039       10.690723        8          1106
FAST17       25.384572       10.642322        8          1106
FAST17       0.319237       10.642322        8          1106
FAST18       21.935494       10.491283        8          1826
FAST18       0.255488       10.491283        8          1826
FAST19       21.349385       10.522182        8          1826
FAST19       0.311369       10.522182        8          1826
FAST20       23.124955       10.487431        8          1826
FAST20       0.317411       10.487431        8          1826
FAST21       27.311387       10.491047        8          1778
FAST21       0.398483       10.491047        8          1778
FAST22       23.993620       10.502191        8          1826
FAST22       0.329767       10.502191        8          1826
FAST23       27.793381       10.502191        8          1826
FAST23       0.359659       10.502191        8          1826
FAST24       29.281399       10.509461        8          1826
FAST24       0.398369       10.509461        8          1826

hg-commands:
NODICT       0.000007       2.425291        
RANDOM       0.083477       3.489515        
LEGACY       0.941867       3.911896        
COVER       67.314295       4.131136        8          386
COVER       2.757895       4.131136        8          386
FAST15       13.466983       3.920128        6          1106
FAST15       0.162656       3.920128        6          1106
FAST16       12.618110       4.032422        8          674
FAST16       0.159073       4.032422        8          674
FAST17       12.883772       4.063581        8          1490
FAST17       0.183131       4.063581        8          1490
FAST18       13.904432       4.085034        8          290
FAST18       0.161078       4.085034        8          290
FAST19       13.762269       4.097054        8          578
FAST19       0.179906       4.097054        8          578
FAST20       15.303927       4.101575        8          434
FAST20       0.213146       4.101575        8          434
FAST21       19.619482       4.104879        8          530
FAST21       0.289158       4.104879        8          530
FAST22       23.187937       4.102448        8          530
FAST22       0.335220       4.102448        8          530
FAST23       24.946655       4.095162        8          914
FAST23       0.396927       4.095162        8          914
FAST24       27.634065       4.114624        8          722
FAST24       0.434278       4.114624        8          722

hg-changelog:
NODICT       0.000027       1.377613        
RANDOM       0.676272       2.096785        
LEGACY       2.871887       2.058273        
COVER       226.371004       2.188654        8          98
COVER       5.359820       2.188654        8          98
FAST15       66.776425       2.130548        6          386
FAST15       0.796836       2.130548        6          386
FAST16       64.405113       2.144136        8          194
FAST16       0.778969       2.144136        8          194
FAST17       65.062292       2.155745        8          98
FAST17       0.822089       2.155745        8          98
FAST18       65.819104       2.172062        6          98
FAST18       0.804247       2.172062        6          98
FAST19       66.184016       2.179446        6          98
FAST19       0.883526       2.179446        6          98
FAST20       72.900924       2.187017        6          98
FAST20       0.908220       2.187017        6          98
FAST21       77.869945       2.183583        6          146
FAST21       0.932666       2.183583        6          146
FAST22       84.041413       2.182030        6          98
FAST22       1.092310       2.182030        6          98
FAST23       89.539265       2.185291        8          98
FAST23       1.294779       2.185291        8          98
FAST24       97.193482       2.184939        6          98
FAST24       1.270493       2.184939        6          98

hg-manifest:
NODICT       0.000004       1.866385        
RANDOM       0.969045       2.309485        
LEGACY       8.849052       2.506775        
COVER       905.855524       2.582597        8          434
COVER       34.951973       2.582597        8          434
FAST15       154.816926       2.391764        6          1826
FAST15       1.932845       2.391764        6          1826
FAST16       142.197120       2.480738        6          1922
FAST16       1.759330       2.480738        6          1922
FAST17       147.276099       2.548313        6          1682
FAST17       1.819175       2.548313        6          1682
FAST18       164.543366       2.567448        6          386
FAST18       2.728845       2.567448        6          386
FAST19       195.670852       2.581170        8          338
FAST19       2.439487       2.581170        8          338
FAST20       195.716408       2.587062        6          194
FAST20       2.056303       2.587062        6          194
FAST21       211.483191       2.590136        6          242
FAST21       2.983587       2.590136        6          242
FAST22       239.562966       2.591033        6          194
FAST22       3.355746       2.591033        6          194
FAST23       264.547195       2.590403        8          434
FAST23       3.667851       2.590403        8          434
FAST24       296.258379       2.591723        6          290
FAST24       3.858688       2.591723        6          290
