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

For every f value for fast, the first one is optimize and the second one has k=200

github:
NODICT       0.000023       2.999642
RANDOM       0.149020       8.786957
LEGACY       0.854277       8.989482
FAST15       8.764078       10.609015
FAST15       0.232610       9.135669
FAST16       9.597777       10.474574
FAST16       0.243698       9.346482
FAST17       9.385449       10.611737
FAST17       0.268376       9.605798
FAST18       9.988885       10.626382
FAST18       0.311769       9.130565
FAST19       10.737259       10.411729
FAST19       0.331885       9.271814
FAST20       10.479782       10.388895
FAST20       0.498416       9.194115
FAST21       21.189883       10.376394
FAST21       1.098532       9.244456
FAST22       39.849935       10.432555
FAST22       2.590561       9.410930
FAST23       75.832399       10.614747
FAST23       6.108487       9.484150
FAST24       139.782714       10.611753
FAST24       13.029406       9.379030
COVER       55.118542       10.641263

hg-commands
NODICT       0.000012       2.425291
RANDOM       0.083071       3.489515
LEGACY       0.835195       3.911896
FAST15       0.163980       3.808375
FAST16       6.373850       4.010783
FAST16       0.160299       3.966604
FAST17       6.668799       4.091602
FAST17       0.172480       4.062773
FAST18       6.266105       4.130824
FAST18       0.171554       4.094666
FAST19       6.869651       4.158180
FAST19       0.209468       4.111289
FAST20       8.267766       4.149707
FAST20       0.331680       4.119873
FAST21       18.824296       4.171784
FAST21       0.783961       4.120884
FAST22       33.321252       4.152035
FAST22       1.854215       4.126626
FAST23       60.775388       4.157595
FAST23       4.040395       4.134222
FAST24       110.910038       4.163091
FAST24       8.505828       4.143533
COVER       61.654796       4.131136

hg-changelog
NODICT       0.000004       1.377613
RANDOM       0.582067       2.096785
LEGACY       2.739515       2.058273
FAST15       35.682665       2.127596
FAST15       0.931621       2.115299
FAST16       36.557988       2.141787
FAST16       1.008155       2.136080
FAST17       36.272242       2.155332
FAST17       0.906803       2.154596
FAST18       35.542043       2.171997
FAST18       1.063101       2.167723
FAST19       37.756934       2.180893
FAST19       1.257291       2.173768
FAST20       40.273755       2.179442
FAST20       1.630522       2.170072
FAST21       54.606548       2.181400
FAST21       2.321266       2.171643
FAST22       72.454066       2.178774
FAST22       5.092888       2.168885
FAST23       106.753208       2.180347
FAST23       14.722222       2.170673
FAST24       171.083201       2.183426
FAST24       27.575575       2.170623
COVER       227.219660       2.188654

hg-manifest
NODICT       0.000007       1.866385
RANDOM       1.086571       2.309485
LEGACY       9.567507       2.506775
FAST15       77.811380       2.380461
FAST15       1.969718       2.317727
FAST16       75.789019       2.469144
FAST16       2.051283       2.375815
FAST17       79.659040       2.539069
FAST17       1.995394       2.501047
FAST18       76.281105       2.578095
FAST18       2.059272       2.564840
FAST19       79.395382       2.590433
FAST19       2.354158       2.591024
FAST20       87.937568       2.597813
FAST20       2.922189       2.597104
FAST21       121.760549       2.598408
FAST21       4.798981       2.600269
FAST22       155.878461       2.594560
FAST22       8.151807       2.601047
FAST23       194.238003       2.596761
FAST23       15.160578       2.592985
FAST24       267.425904       2.597657
FAST24       29.513286       2.600363
COVER       930.675322       2.582597
