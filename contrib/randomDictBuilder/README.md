Random Dictionary Builder

### Permitted Arguments:
Input Files (in=fileName): files used to build dictionary, can include multiple files, each following "in=", required
Output Dictionary (out=dictName): if not provided, default to defaultDict
Dictionary ID (dictID=#): positive number, if not provided, default to 0
Maximum Dictionary Size (maxdict=#): positive number, in bytes, if not provided, default to 110KB
Size of Randomly Selected Segment (k=#): positive number, in bytes, if not provided, default to 200
Compression Level (c=#): positive number, if not provided, default to 3

### Examples:
make run ARG="in=../../lib/dictBuilder out=dict100 dictID=520"
make run ARG="in=../../lib/dictBuilder in=../../lib/compress"
