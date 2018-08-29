largeNbDicts
=====================

`largeNbDicts` is a benchmark test tool
dedicated to the specific scenario of
dictionary decompression using a very large number of dictionaries,
which suffers from increased latency due to cache misses.
It's created in a bid to investigate performance for this scenario,
and experiment mitigation techniques.

Command line :
```
$ largeNbDicts filename [Options]
Options :
--clevel=#     : use compression level # (default: 3)
--blockSize=#  : cut input into blocks of size # (default: 4096)
--dictionary=# : use # as a dictionary (default: create one)
--nbDicts=#    : set nb of dictionaries to # (default: one per block)
```
