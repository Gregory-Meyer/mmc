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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

#include <lz4frame.h>

int main(int argc, const char *const argv[]) {
  PassthroughArgumentParser input_filename_parser =
      make_passthrough_parser("INPUT_FILE", NULL);
  PositionalArgument input_filename = {
      .name = "INPUT_FILE",
      .help_text = "Compressed file to read from. The current user must have "
                   "the correct permissions to read from this file.",
      .parser = &input_filename_parser.argument_parser};

  PassthroughArgumentParser output_filename_parser =
      make_passthrough_parser("OUTUPT_FILE", NULL);
  PositionalArgument output_filename = {
      .name = "OUTPUT_FILE",
      .help_text =
          "Filename of the uncompressed file to create. If this file already "
          "exists, it is truncated to length 0 before being written to. Should "
          "mmap-lz4-decompress exit with an error after truncating this file, "
          "it will be deleted. The current user must have write permissions in "
          "this file's parent directory and, if the file already exists, write "
          "permissions on this file.",
      .parser = &output_filename_parser.argument_parser};

  PositionalArgument *positional_args[] = {&input_filename, &output_filename};

  Arguments arguments = {
      .executable_name = "mmap-lz4-decompress",
      .version = "0.1.2",
      .author = "Gregory Meyer <me@gregjm.dev>",
      .description =
          "mmap-lz4-decompress (mld) uncompresses a file using the LZ4 "
          "compression algorithm. liblz4 is used for decompression and "
          "memory-mapped files are used to read and write data to disk.",

      .positional_args = positional_args,
      .num_positional_args =
          sizeof(positional_args) / sizeof(positional_args[0]),

      .keyword_args = NULL,
      .num_keyword_args = 0};

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

  FileAndMapping input_file;
  error = open_and_map_file(input_filename_parser.value, &input_file);
  int return_code = EXIT_SUCCESS;

  if (error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  FileAndMapping output_file;
  error = create_and_map_file(output_filename_parser.value, input_file.size,
                              &output_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;

    goto cleanup_input_only;
  }

  LZ4F_dctx *decompression_context;
  const LZ4F_errorCode_t create_errc =
      LZ4F_createDecompressionContext(&decompression_context, LZ4F_VERSION);

  if (LZ4F_isError(create_errc)) {
    const char *const what = LZ4F_getErrorName(create_errc);

    print_error(eformat("couldn't initialize decompression context: %s (%zu)",
                        what, create_errc));
    return_code = EXIT_FAILURE;

    goto cleanup;
  }

  posix_madvise(input_file.contents, input_file.size, POSIX_MADV_SEQUENTIAL);
  posix_madvise(output_file.contents, output_file.size, POSIX_MADV_SEQUENTIAL);

  size_t input_first_unused = 0;
  size_t output_first_unused = 0;
  size_t output_current_length = output_file.size;
  size_t output_final_length = 0;

  while (input_first_unused < input_file.size) {
    size_t input_unused_length_or_bytes_consumed =
        input_file.size - input_first_unused;
    size_t output_unused_length_or_bytes_consumed =
        output_file.size - output_first_unused;
    const size_t maybe_decompress_errc =
        LZ4F_decompress(decompression_context,
                        (char *)output_file.contents + output_first_unused,
                        &output_unused_length_or_bytes_consumed,
                        (const char *)input_file.contents + output_first_unused,
                        &input_unused_length_or_bytes_consumed, NULL);

    if (LZ4F_isError(maybe_decompress_errc)) {
      const char *const what = LZ4F_getErrorName(create_errc);

      print_error(eformat("couldn't decompress stream: %s (%zu)", what,
                          maybe_decompress_errc));
      return_code = EXIT_FAILURE;

      break;
    }

    input_first_unused += input_unused_length_or_bytes_consumed;
    output_first_unused += output_unused_length_or_bytes_consumed;
    output_final_length += output_unused_length_or_bytes_consumed;

    error = unmap_unused_pages(&input_file, &input_first_unused);

    if (error.what) {
      print_error(error);
      return_code = EXIT_FAILURE;

      break;
    }

    error = unmap_unused_pages(&output_file, &output_first_unused);

    if (error.what) {
      print_error(error);
      return_code = EXIT_FAILURE;

      break;
    }

    const size_t previous_length = output_current_length;
    error = expand_output_mapping(&output_file, &output_current_length,
                                  output_first_unused);

    if (error.what) {
      print_error(error);
      return_code = EXIT_FAILURE;

      break;
    }

    if (output_current_length > previous_length) {
      posix_madvise(output_file.contents, output_file.size,
                    POSIX_MADV_SEQUENTIAL);
    }
  }

  const LZ4F_errorCode_t free_errc =
      LZ4F_freeDecompressionContext(decompression_context);
  assert(free_errc == 0);

  if (return_code == EXIT_SUCCESS) {
    if (ftruncate(output_file.fd, (off_t)output_final_length) == -1) {
      print_error(ERRNO_EFORMAT("couldn't resize output file '%s'",
                                output_filename_parser.value));
      return_code = EXIT_FAILURE;
    }
  }

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
