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

Tests are performed using a subset of the data from the
[Squash Compression Benchmark]. Times were recorded using [hyperfine] with the
`--warmup 64` flag. All tests were performed using mmc 0.2.1.

| Name | Source | Description | Size | `md` | `gzip` | `mi` | `gunzip` | `mlc` | `lz4` | `mld` | `unlz4` |
|------|--------|-------------|------|------|--------|------|----------|-------|-------|-------|---------|
| alice29.txt | [Canterbury Corpus] | English text | 148.524 KiB | 6.453 ms | 6.931 ms | 847.147 us | 2.128 ms | 448.110 us | 648.072 us | 490.525 us | 288.897 us |
| asyoulik.txt | [Canterbury Corpus] | Shakespeare | 122.245 KiB | 6.325 ms | 6.406 ms | 1.006 ms | 2.230 ms | 905.642 us | 1.007 ms | 474.814 us | 646.272 us |
| cp.html | [Canterbury Corpus] | HTML source | 24.026 KiB | 704.935 us | 1.259 ms | 588.746 us | 1.912 ms | 423.007 us | 895.572 us | 354.137 us | 264.199 us |
| dickens | [Silesia Corpus] | Collected works of Charles Dickens | 9.720 MiB | 543.897 ms | 557.885 ms | 39.082 ms | 54.370 ms | 31.348 ms | 33.072 ms | 13.847 ms | 13.304 ms |
| enwik8 | [Large Text Compression Benchmark] | The first 10⁸ bytes of the English Wikipedia dump on Mar. 3, 2006 | 95.367 MiB | 4.062 s | 4.046 s | 442.839 ms | 596.586 ms | 290.687 ms | 310.804 ms | 350.351 ms | 147.741 ms |
| fields.c | [Canterbury Corpus] | C source | 10.888 KiB | 333.006 us | 798.120 us | 386.961 us | 1.657 ms | 312.148 us | 478.251 us | 638.734 us | 402.548 us |
| grammar.lsp | [Canterbury Corpus] | LISP source | 3.633 KiB | 500.878 us | 734.004 us | 336.822 us | 1.507 ms | 370.056 us | 428.229 us | 586.001 us | 584.324 us |
| kennedy.xls | [Canterbury Corpus] | Excel Spreadsheet | 1005.699 KiB | 25.059 ms | 26.698 ms | 2.937 ms | 4.813 ms | 1.942 ms | 2.396 ms | 1.648 ms | 1.227 ms |
| lcet10.txt | [Canterbury Corpus] | Technical writing | 416.751 KiB | 17.720 ms | 18.187 ms | 1.913 ms | 3.666 ms | 1.768 ms | 1.669 ms | 1.049 ms | 927.552 us |
| mozilla | [Silesia Corpus] | Tarred executables of Mozilla 1.0 (Tru64 UNIX edition) | 48.847 MiB | 1.799 s | 1.945 s | 205.346 ms | 301.032 ms | 110.514 ms | 116.601 ms | 70.182 ms | 67.226 ms |
| mr | [Silesia Corpus] | Medical magnetic resonance image | 9.508 MiB | 466.554 ms | 472.509 ms | 35.404 ms | 54.820 ms | 19.898 ms | 21.945 ms | 13.083 ms | 12.706 ms |
| nci | [Silesia Corpus] | Chemical database of structures | 31.999 MiB | 375.273 ms | 399.504 ms | 67.993 ms | 117.718 ms | 35.992 ms | 40.555 ms | 43.922 ms | 38.034 ms |
| ooffice | [Silesia Corpus] | A dll from Open Office.org 1.01 | 5.867 MiB | 273.866 ms | 283.677 ms | 29.408 ms | 41.312 ms | 14.754 ms | 16.724 ms | 7.766 ms | 8.552 ms |
| osdb | [Silesia Corpus] | Sample database in MySQL format from Open Source Database Benchmark | 9.618 MiB | 255.551 ms | 285.503 ms | 34.630 ms | 56.580 ms | 22.277 ms | 24.552 ms | 13.353 ms | 13.006 ms |
| plrabn12.txt | [Canterbury Corpus] | Poetry | 470.567 KiB | 27.353 ms | 27.557 ms | 2.408 ms | 4.093 ms | 1.913 ms | 2.008 ms | 1.019 ms | 1.139 ms |
| ptt5 | [Canterbury Corpus] | CCITT test set | 501.187 KiB | 7.995 ms | 8.994 ms | 2.000 ms | 3.199 ms | 774.048 us | 1.180 ms | 1.192 ms | 906.565 us |
| reymont | [Silesia Corpus] | Text of the book Chłopi by Władysław Reymont | 6.320 MiB | 322.109 ms | 343.591 ms | 22.094 ms | 32.332 ms | 18.125 ms | 20.289 ms | 9.641 ms | 8.818 ms |
| samba | [Silesia Corpus] | Tarred source code of Samba 2-2.3 | 20.605 MiB | 464.090 ms | 481.947 ms | 65.377 ms | 144.307 ms | 36.811 ms | 41.754 ms | 27.656 ms | 26.020 ms |
| sao | [Silesia Corpus] | The SAO star catalog | 6.915 MiB | 374.567 ms | 383.410 ms | 30.913 ms | 45.747 ms | 19.090 ms | 21.077 ms | 8.771 ms | 9.386 ms |
| sum | [Canterbury Corpus] | SPARC Executable | 37.343 KiB | 1.643 ms | 1.965 ms | 754.093 us | 1.833 ms | 402.242 us | 579.432 us | 823.601 us | 265.861 us |
| webster | [Silesia Corpus] | The 1913 Webster Unabridged Dictionary | 39.538 MiB | 1.411 s | 1.435 s | 156.747 ms | 218.375 ms | 115.067 ms | 119.018 ms | 55.828 ms | 53.036 ms |
| x-ray | [Silesia Corpus] | X-ray medical picture | 8.081 MiB | 300.146 ms | 317.132 ms | 44.073 ms | 69.712 ms | 12.715 ms | 13.617 ms | 8.292 ms | 10.244 ms |
| xargs.1 | [Canterbury Corpus] | GNU manual page | 4.127 KiB | 296.136 us | 752.579 us | 544.641 us | 1.496 ms | 208.976 us | 256.277 us | 530.425 us | 278.854 us |
| xml | [Silesia Corpus] | Collected XML files | 5.097 MiB | 82.004 ms | 85.821 ms | 12.166 ms | 19.433 ms | 7.699 ms | 8.888 ms | 7.571 ms | 6.558 ms |


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
[Squash Compression Benchmark]: https://quixdb.github.io/squash-benchmark/
[hyperfine]: https://github.com/sharkdp/hyperfine
[Canterbury Corpus]: http://corpus.canterbury.ac.nz/descriptions/#cantrbry
[Silesia Corpus]: http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia
[Large Text Compression Benchmark]: http://www.mattmahoney.net/dc/textdata.html
