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
RANDOM       0.034014       8.791189        
LEGACY       0.993954       8.173529        
COVER       59.243793       10.652243        8          1298
COVER       5.602981       10.652243        8          1298
FAST15       8.944343       10.551970        8          1874
FAST15       0.060415       10.551970        8          1874
FAST16       9.227988       10.698853        8          1106
FAST16       0.061976       10.698853        8          1106
FAST17       9.380495       10.701698        8          1106
FAST17       0.067343       10.701698        8          1106
FAST18       10.650935       10.540483        8          1874
FAST18       0.073262       10.540483        8          1874
FAST19       11.861344       10.534978        8          1874
FAST19       0.08592       10.534978        8          1874
FAST20       12.156516       10.517584        8          1874
FAST20       0.110863       10.517584        8          1874
FAST21       13.444900       10.545279        8          1874
FAST21       0.123729       10.545279        8          1874
FAST22       14.883325       10.559145        6          1010
FAST22       0.128846       10.559145        6          1010
FAST23       16.522820       10.570538        6          1010
FAST23       0.150492       10.570538        6          1010
FAST24       17.106452       10.611617        6          1010
FAST24       0.165137       10.611617        6          1010

hg-commands:
NODICT       0.000007       2.425291        
RANDOM       0.065889       3.490331        
LEGACY       1.207616       3.911682        
COVER       74.489403       4.132653        8          386
COVER       3.215184       4.132653        8          386
FAST15       10.043437       3.916461        6          818
FAST15       0.076316       3.916461        6          818
FAST16       9.703066       4.028033        8          674
FAST16       0.074792       4.028033        8          674
FAST17       11.208213       4.063987        8          1490
FAST17       0.088007       4.063987        8          1490
FAST18       11.350147       4.081631        8          290
FAST18       0.090308       4.081631        8          290
FAST19       12.246058       4.094209        8          578
FAST19       0.097509       4.094209        8          578
FAST20       14.159601       4.101742        8          434
FAST20       0.128217       4.101742        8          434
FAST21       15.531496       4.102190        8          386
FAST21       0.153074       4.102190        8          386
FAST22       18.395969       4.094027        8          530
FAST22       0.195774       4.094027        8          530
FAST23       19.092620       4.101985        8          914
FAST23       0.238042       4.101985        8          914
FAST24       23.311861       4.107716        8          722
FAST24       0.283651       4.107716        8          722

hg-changelog:
NODICT       0.000010       1.377613        
RANDOM       0.246226       2.097487        
LEGACY       2.699278       2.058907        
COVER       218.642638       2.189685        8          98
COVER       6.678614       2.189685        8          98
FAST15       45.163589       2.128453        8          578
FAST15       0.288409       2.128453        8          578
FAST16       47.352015       2.144324        8          194
FAST16       0.310171       2.144324        8          194
FAST17       47.511351       2.158889        6          290
FAST17       0.310214       2.158889        6          290
FAST18       44.558437       2.171179        6          98
FAST18       0.30314       2.171179        6          98
FAST19       46.450810       2.182175        6          98
FAST19       0.332944       2.182175        6          98
FAST20       53.273275       2.185875        6          98
FAST20       0.424863       2.185875        6          98
FAST21       60.597922       2.187343        6          146
FAST21       0.537917       2.187343        6          146
FAST22       72.853263       2.185483        6          98
FAST22       0.603151       2.185483        6          98
FAST23       68.473010       2.185458        8          98
FAST23       0.660587       2.185458        8          98
FAST24       79.241432       2.186826        6          98
FAST24       0.720539       2.186826        6          98

hg-manifest:
NODICT       0.000007       1.866385        
RANDOM       0.921900       2.309436        
LEGACY       10.478105       2.506977        
COVER       1042.749309       2.582528        8          434
COVER       40.641417       2.582528        8          434
FAST15       155.823562       2.406392        6          1586
FAST15       1.231394       2.406392        6          1586
FAST16       153.995200       2.481369        6          1922
FAST16       1.163162       2.481369        6          1922
FAST17       148.559612       2.540650        8          1538
FAST17       1.203054       2.540650        8          1538
FAST18       166.898125       2.566880        6          386
FAST18       1.078501       2.566880        6          386
FAST19       150.094006       2.582884        8          338
FAST19       1.223703       2.582884        8          338
FAST20       193.525339       2.585657        8          338
FAST20       1.793958       2.585657        8          338
FAST21       241.181768       2.586139        6          290
FAST21       2.17063       2.586139        6          290
FAST22       237.865216       2.589631        6          242
FAST22       2.76955       2.589631        6          242
FAST23       260.903480       2.588158        6          194
FAST23       2.629834       2.588158        6          194
FAST24       264.047022       2.587816        6          386
FAST24       2.791977       2.587816        6          386
