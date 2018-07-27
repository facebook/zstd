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
First Cover is optimize cover, second Cover uses optimized d and k from first one.
For every f value of fastCover, the first one is optimize fastCover and the second one uses optimized d and k from first one.

github:
NODICT       0.000004       2.999642
RANDOM       0.146096       8.786957
LEGACY       0.956888       8.989482
COVER       56.596152       10.641263
COVER       4.937047       10.641263
FAST15       17.722269       10.586461
FAST15       0.239135       10.586461
FAST16       18.276179       10.492503
FAST16       0.265285       10.492503
FAST17       18.077916       10.611737
FAST17       0.236573       10.611737
FAST18       19.510150       10.621586
FAST18       0.278683       10.621586
FAST19       18.794350       10.629626
FAST19       0.307943       10.629626
FAST20       19.671099       10.610308
FAST20       0.428814       10.610308
FAST21       36.527238       10.625733
FAST21       0.716384       10.625733
FAST22       83.803521       10.625281
FAST22       1.290246       10.625281
FAST23       158.287924       10.602342
FAST23       3.084848       10.602342
FAST24       283.630941       10.603379
FAST24       8.088933       10.603379

hg-commands
NODICT       0.000007       2.425291
RANDOM       0.084010       3.489515
LEGACY       0.926763       3.911896
COVER       62.036915       4.131136
COVER       2.194398       4.131136
FAST15       12.169025       3.903719
FAST15       0.156552       3.903719
FAST16       11.886255       4.005077
FAST16       0.155506       4.005077
FAST17       11.886955       4.097811
FAST17       0.176327       4.097811
FAST18       12.544698       4.136081
FAST18       0.171796       4.136081
FAST19       12.920868       4.166021
FAST19       0.207029       4.166021
FAST20       15.771429       4.163740
FAST20       0.258685       4.163740
FAST21       33.165829       4.157057
FAST21       0.663088       4.157057
FAST22       68.779201       4.158195
FAST22       1.568439       4.158195
FAST23       121.921931       4.161450
FAST23       2.498972       4.161450
FAST24       221.990451       4.159658
FAST24       5.793594       4.159658

hg-changelog
NODICT       0.000004       1.377613
RANDOM       0.549307       2.096785
LEGACY       2.273818       2.058273
COVER       219.640608       2.188654
COVER       6.055391       2.188654
FAST15       67.820700       2.127194
FAST15       0.824624       2.127194
FAST16       69.774209       2.145401
FAST16       0.889737       2.145401
FAST17       70.027355       2.157544
FAST17       0.869004       2.157544
FAST18       68.229652       2.173127
FAST18       0.930689       2.173127
FAST19       70.696241       2.179527
FAST19       1.385515       2.179527
FAST20       80.618172       2.183233
FAST20       1.699632       2.183233
FAST21       96.366254       2.180920
FAST21       2.606553       2.180920
FAST22       139.440758       2.184297
FAST22       5.962606       2.184297
FAST23       207.791930       2.187666
FAST23       14.823301       2.187666
FAST24       322.050385       2.189889
FAST24       29.294918       2.189889

hg-manifest
NODICT       0.000008       1.866385
RANDOM       1.075766       2.309485
LEGACY       8.688387       2.506775
COVER       926.024689       2.582597
COVER       33.630695       2.582597
FAST15       152.845945       2.377689
FAST15       2.206285       2.377689
FAST16       147.772371       2.464814
FAST16       1.937997       2.464814
FAST17       147.729498       2.539834
FAST17       1.966577       2.539834
FAST18       144.156821       2.576924
FAST18       1.954106       2.576924
FAST19       145.678760       2.592479
FAST19       2.096876       2.592479
FAST20       159.634674       2.594551
FAST20       2.568766       2.594551
FAST21       228.116552       2.597128
FAST21       4.634508       2.597128
FAST22       288.890644       2.596971
FAST22       6.618204       2.596971
FAST23       377.196211       2.601416
FAST23       13.497286       2.601416
FAST24       503.208577       2.602830
FAST24       29.538585       2.602830
