# relative
Tools to measure libcurl performance delta between versions

## build-many

This script iterates over all the curl versions listed in the top of the file
and for each version it will build and install it locally. It will clone the
git repository first if not present.

Each built libcurl version is installed in `build/$version`. The used build
directory will be removed after install.

## run-many

This script iterates over all the libcurl versions found installed in
`build/$version`. For each such version, it will set `LD_LIBRARY_PATH` and
then invoke `./sprinter` to have that specific version performance tested.

If the found libcurl version is older than 7.66.0, the `./sprinter-old` tool
will instead be used (to use a different API).

## sprinter

Downloads the same URL a given number of times, using a single thread and the
libcurl multi interface. It uses a specified number of concurrent transfers.

The data is simply dropped on receival. If any transfer returns an error, the
entire operation is canceled and an error message is shown.

Only use this tool against servers you have permission to torture.

### Example using 8GB downloads

~~~
$ ./sprinter localhost/8GB 6 2
curl: 7.79.0-DEV
URL: localhost/8GB
Transfers: 6 [2 in parallel]...
Time: 8110158 us, 1351693.00 us/transfer
Freq: 0.74 requests/second
Downloaded: 51539608924 bytes, 48.0 GB
Speed: 6354945110.1 bytes/sec 6060.5 MB/s (8589934820 bytes/transfer)
~~~

### Example using 4KB downloads

~~~
$ ./sprinter localhost/4KB 100000 100
curl: 7.79.0-DEV
URL: localhost/4KB
Transfers: 100000 [100 in parallel]...
Time: 4065449 us, 40.65 us/transfer
Freq: 24597.53 requests/second
Downloaded: 430255224 bytes, 0.4 GB
Speed: 105832153.8 bytes/sec 100.9 MB/s (4302 bytes/transfer)
~~~
