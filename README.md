# mmap-deflate (md) and mmap-inflate (mi)

mmap-deflate and mmap-inflate are file compression utilities implemented as
frontends for the zlib compression library. File I/O is accomplished using
[`mmap(2)`], with calls to [`ftruncate(2)`] and [`mremap(2)`] to increase the
size of the output file as appropriate.

## Usage

```bash
md $UNCOMPRESSED $COMPRESSED --level=$LEVEL --strategy=$STRATEGY
mi $COMPRESSED $UNCOMPRESSED
```

mmap-deflate and mmap-inflate operate on raw zlib formatted archives. The zlib
compression level and strategy used by mmap-deflate can be set using the `-l`,
`--level` and the `-s`, `--strategy` options.

Further usage information can be viewed using the `-h`, `--help` options for
both `md` and `mi`.

## Implementation

mmap-deflate unmaps pages in 64KiB chunks after it is done using them, which is
when it has finished reading from them (for the input file) or when it has
filled up that span with data (for the output file). Whenever the output file
buffer fills up, it is doubled in size and the file mapping is updated with the
new size.

## Build Requirements

mmap-deflate is written in standards-compliant C99 using the Linux extension
[`mremap(2)`] and the glibc [`getopt_long(3)`] extensions. [CMake] 3.11 or
higher is required, as the [`CMakeLists.txt`] makes use of the `c_std_99`
compile feature.

## Performance

Memory-mapped file I/O via [`mmap(2)`], [`ftruncate(2)`], and [`mremap(2)`] is
significantly more efficient than the equivalent I/O using [`read(2)`] and
[`write(2)`] for large (>2MiB) files.

Tests are performed in the two degenerate cases of compression -- all zeros, as
obtained from `/dev/zero`, and random data, as obtained from `/dev/urandom`. All
tests were performed using [hyperfine] 1.8.0. Compression tests were invoked
using the default compression level and strategy. `--warmup 512` was used for
the 4 KiB and 2 MiB tests, while `--warmup 2` was used for the 1 GiB tests.

### Compression
#### Zeros
| Size |  `md`   | `gzip`  |
|------|---------|---------|
| 4KiB | 0.7 ms  | 0.4 ms  |
| 2MiB | 6.6 ms  | 7.9 ms  |
| 1GiB | 2.990 s | 3.612 s |

#### Random Data
| Size |   `md`   |  `gzip`  |
|------|----------|----------|
| 4KiB |  0.6 ms  |  0.4 ms  |
| 2MiB | 43.2 ms  | 49.2 ms  |
| 1GiB | 22.810 s | 24.311 s |

### Decompression
#### Zeros
| Size |  `mi`   | `gunzip` |
|------|---------|----------|
| 4KiB | 1.4 ms  | 1.4 ms   |
| 2MiB | 5.0 ms  | 7.7 ms   |
| 1GiB | 2.732 s | 3.325 s  |

#### Random Data
| Size |   `mi`  | `gunzip` |
|------|---------|----------|
| 4KiB | 0.1 ms  | 0.7 ms   |
| 2MiB | 1.9 ms  | 9.4 ms   |
| 1GiB | 2.191 s | 4.404 s  |

## License

mmap-deflate is licensed under the MIT license.

[`mmap(2)`]: http://man7.org/linux/man-pages/man2/mmap.2.html
[`ftruncate(2)`]: http://man7.org/linux/man-pages/man2/ftruncate.2.html
[`mremap(2)`]: http://man7.org/linux/man-pages/man2/mremap.2.html
[`getopt_long(3)`]: http://man7.org/linux/man-pages/man3/getopt_long.3.html
[CMake]: https://cmake.org/
[`CMakeLists.txt`]: CMakeLists.txt
[`read(2)`]: http://man7.org/linux/man-pages/man2/read.2.html
[`write(2)`]: http://man7.org/linux/man-pages/man2/write.2.html
[hyperfine]: https://github.com/sharkdp/hyperfine
