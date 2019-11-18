# Memory-Mapped File Compression (mmc)

mmc is a set of file compression and decompression utilities built as frontends
to zlib and liblz4. File I/O is accomplished using [`mmap(2)`], with calls to
[`ftruncate(2)`] and [`mremap(2)`] to increase the size of the output file as
appropriate.

## Usage

```bash
# zlib frontends
md $UNCOMPRESSED $COMPRESSED --level=$LEVEL --strategy=$STRATEGY
mi $COMPRESSED $UNCOMPRESSED

# liblz4 frontends
mlc $UNCOMPRESSED $COMPRESSED --block-mode=$MODE --block-size=$SIZE \
    --favor-decompression-speed --compression-level=$LEVEL
mld $COMPRESSED $UNCOMPRESSED
```

mmap-deflate and mmap-inflate operate on raw zlib formatted archives. The zlib
compression level and strategy used by mmap-deflate can be set using the (`-l`,
`--level`) and the (`-s`, `--strategy`) options.

mmap-lz4-compress and mmap-lz4-decompress operate on LZ4 framed archives and are
interoperable with archives produced by lz4(1). The LZ4 parameters
used by mmap-lz4-compress can be tuned using the (`-m`, `--block-mode`),
(`-s`, `--block-size`), (`-d`, `--favor-decompression-speed`), and (`-l`,
`--level`) options.

Further usage information can be viewed by using the `-h`, `--help` option.

## Build Requirements

The executables provided by mmc are written in standards-compliant C99 using the
Linux extension [`mremap(2)`]. [CMake] 3.11 or higher is required, as the
[`CMakeLists.txt`] makes use of the `c_std_99` compile feature.

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

## Memory-Mapped File I/O Implementation Details

For all utilities, the entire input file is mapped into memory at once.
Compression utilities will create the output file and set its length to the
maximum theoretically possible compressed size, which is a little larger than
the size of the uncompressed file. Decompression utilities initially set the
length of the output file to the same length as the input file and double its
on-disk length as necessary. Pages that have already been completely read from
or written to are unmapped in 64KiB chunks.

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
