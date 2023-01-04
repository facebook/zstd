externalMatchfinder
=====================

`externalMatchfinder` is a test tool for the external matchfinder API.
It demonstrates how to use the API to perform a simple round-trip test.

A sample matchfinder is provided in matchfinder.c, but the user can swap
this out with a different one if desired. The sample matchfinder implements
LZ compression with a 1KB hashtable. Dictionary compression is not currently supported.

Command line :
```
externalMatchfinder filename
```
