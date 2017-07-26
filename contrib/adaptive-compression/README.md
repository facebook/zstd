###Summary

`adapt` is a new compression tool targeted at optimizing performance across network connections. The tool aims at sensing network speeds and adapting compression level based on network or pipe speeds.
In situations where the compression level does not appropriately match the network/pipe speed, the compression may be bottlenecking the entire pipeline or the files may not be compressed as much as they potentially could be, therefore losing efficiency. It also becomes quite impractical to manually measure and set compression level, therefore the tool does it for you.

###Using `adapt`

In order to build and use the tool, you can simply run `make adapt` in the `adaptive-compression` directory under `contrib`. This will generate an executable available for use.

###Options
`-oFILE` : write output to `FILE`

`-i#`    : provide initial compression level

`-h`     : display help/information

`-f`     : force the compression level to stay constant

`-c`     : force write to `stdout`

`-p`     : hide progress bar

`-q`     : quiet mode -- do not show progress bar or other information

###Benchmarking / Test results
