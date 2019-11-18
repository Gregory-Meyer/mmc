// Copyright (c) 2019 Gregory Meyer
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <argparse.h>
#include <common.h>
#include <zlib_common.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <zlib.h>

size_t max_compressed_size(size_t uncompressed_size);
Error do_compress(z_stream *stream, bool *finished);

int main(int argc, const char *const argv[]) {
  PassthroughArgumentParser input_filename_parser =
      make_passthrough_parser("INPUT_FILE", NULL);
  PositionalArgument input_filename = {
      .name = "INPUT_FILE",
      .help_text = "Uncompressed file to read from. The current user must have "
                   "the correct permissions to read from this file.",
      .parser = &input_filename_parser.argument_parser};

  PassthroughArgumentParser output_filename_parser =
      make_passthrough_parser("OUTUPT_FILE", NULL);
  PositionalArgument output_filename = {
      .name = "OUTPUT_FILE",
      .help_text =
          "Filename of the compressed file to create. If this file already "
          "exists, it is truncated to length 0 before being written to. Should "
          "mmap-deflate exit with an error after truncating this file, it will "
          "be deleted. The current user must have write permissions in this "
          "file's parent directory and, if the file already exists, write "
          "permissions on this file.",
      .parser = &output_filename_parser.argument_parser};

  PositionalArgument *positional_args[] = {&input_filename, &output_filename};

  IntegerArgumentParser level_parser = make_integer_parser(
      "-l, --level", "LEVEL", Z_NO_COMPRESSION, Z_BEST_COMPRESSION);
  KeywordArgument level = {
      .short_name = 'l',
      .long_name = "level",
      .help_text =
          "Compression level to use. An integer in the range [" STRINGIFY(
              Z_NO_COMPRESSION) ", " STRINGIFY(Z_BEST_COMPRESSION) "].",
      .parser = &level_parser.argument_parser};

  const char *strategy_values[] = {"default", "filtered", "huffman-only", "rle",
                                   "fixed"};
  const int strategy_mapping[] = {Z_DEFAULT_STRATEGY, Z_FILTERED,
                                  Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED};
  StringArgumentParser strategy_parser =
      make_string_parser("-s, --strategy", "STRATEGY", strategy_values,
                         sizeof(strategy_values) / sizeof(strategy_values[0]));
  KeywordArgument strategy = {
      .short_name = 's',
      .long_name = "strategy",
      .help_text = "Compression strategy to use. One of 'default', 'filtered', "
                   "'huffman-only', 'rle', or 'fixed', corresponding to the "
                   "zlib compression strategies.",
      .parser = &strategy_parser.argument_parser};

  KeywordArgument *keyword_args[] = {&strategy, &level};

  Arguments arguments = {
      .executable_name = "mmap-deflate",
      .version = "0.2.0",
      .author = "Gregory Meyer <me@gregjm.dev>",
      .description =
          "mmap-deflate (md) compresses a file using the DEFLATE compression "
          "algorithm. zlib is used for compression and memory-mapped files are "
          "used to read and write data to disk.",

      .positional_args = positional_args,
      .num_positional_args =
          sizeof(positional_args) / sizeof(positional_args[0]),

      .keyword_args = keyword_args,
      .num_keyword_args = sizeof(keyword_args) / sizeof(keyword_args[0])};

  Error error = parse_arguments(&arguments, argc, argv);

  if (error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  if (arguments.has_help) {
    print_help(&arguments);

    return EXIT_SUCCESS;
  } else if (arguments.has_version) {
    print_version(&arguments);

    return EXIT_SUCCESS;
  }

  int strategy_value = Z_DEFAULT_STRATEGY;
  if (strategy.was_found) {
    strategy_value = strategy_mapping[strategy_parser.value_index];
  }

  int level_value = Z_DEFAULT_COMPRESSION;
  if (level.was_found) {
    level_value = (int)level_parser.value;
  }

  FileAndMapping input_file;
  error = open_and_map_file(input_filename_parser.value, &input_file);
  int return_code = EXIT_SUCCESS;

  if (error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  FileAndMapping output_file;
  error =
      create_and_map_file(output_filename_parser.value,
                          max_compressed_size(input_file.size), &output_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;

    goto cleanup_input_only;
  }

  z_stream stream = {.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};

  const int init_errc =
      deflateInit2(&stream, level_value, Z_DEFLATED, 15, 8, strategy_value);

  if (init_errc != Z_OK) {
    assert(init_errc != Z_STREAM_ERROR);

    return_code = EXIT_FAILURE;

    const char *what;
    switch (init_errc) {
    case Z_MEM_ERROR:
      what = "out of memory";

      break;

    case Z_VERSION_ERROR:
      what = "zlib library version mismatch";

      break;
    default:
      abort();
    }

    if (stream.msg) {
      error = eformat("couldn't initialize deflate stream: %s (%d): %s", what,
                      init_errc, stream.msg);
    } else {
      error = eformat("couldn't initialize deflate stream: %s (%d)", what,
                      init_errc);
    }

    print_error(error);

    goto cleanup;
  }

  error =
      transform_mapped_file(&input_file, &output_file, do_compress, &stream);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

  const int deflate_end_errc = deflateEnd(&stream);
  assert(deflate_end_errc == Z_OK);

cleanup:
  error = free_file(output_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

  if (return_code != EXIT_SUCCESS) {
    if (unlink(output_filename_parser.value) == -1) {
      print_error(ERRNO_EFORMAT("couldn't remove file '%s'",
                                output_filename_parser.value));
    }
  }

cleanup_input_only:
  error = free_file(input_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

  return return_code;
}

Error do_compress(z_stream *stream, bool *finished) {
  assert(stream);
  assert(finished);

  int flag;

  if ((size_t)stream->avail_out >=
      max_compressed_size((size_t)stream->avail_in)) {
    flag = Z_FINISH;
  } else {
    flag = Z_NO_FLUSH;
  }

  const int errc = deflate(stream, flag);

  if (errc != Z_OK) {
    assert(errc != Z_STREAM_ERROR);
    assert(errc != Z_BUF_ERROR);

    const char *what;
    switch (errc) {
    case Z_STREAM_END:
      *finished = true;

      return NULL_ERROR;

    case Z_NEED_DICT:
      what = "dictionary needed";

      break;

    case Z_DATA_ERROR:
      what = "input data corrupted";

      break;

    case Z_MEM_ERROR:
      what = "out of memory";

      break;

    default:
      abort();
    }

    if (stream->msg) {
      return eformat("couldn't inflate stream: %s (%d): %s", what, errc,
                     stream->msg);
    }

    return eformat("couldn't inflate stream: %s (%d)", what, errc);
  }

  *finished = false;

  return NULL_ERROR;
}

size_t max_compressed_size(size_t uncompressed_size) {
  static const size_t BLOCK_SIZE = 16000;
  static const size_t BYTES_PER_BLOCK = 5;
  static const size_t OVERHEAD_PER_STREAM;

  size_t num_blocks = uncompressed_size / BLOCK_SIZE; // 16 KB

  if (uncompressed_size % BLOCK_SIZE == 0) {
    ++num_blocks;
  }

  return uncompressed_size + num_blocks * BYTES_PER_BLOCK + OVERHEAD_PER_STREAM;
}
