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
#include <limits.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <lz4frame.h>
#include <lz4hc.h>

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
          "mmap-lz4-compress exit with an error after truncating this file, it "
          "will be deleted. The current user must have write permissions in "
          "this file's parent directory and, if the file already exists, write "
          "permissions on this file.",
      .parser = &output_filename_parser.argument_parser};

  PositionalArgument *positional_args[] = {&input_filename, &output_filename};

  static const LZ4F_blockMode_t block_mode_mapping[] = {LZ4F_blockLinked,
                                                        LZ4F_blockIndependent};
  StringArgumentParser block_mode_parser = make_string_parser(
      "-m, --block-mode", "MODE", (const char *[]){"linked", "independent"}, 2);
  KeywordArgument block_mode = {
      .short_name = 'm',
      .long_name = "block-mode",
      .help_text = "Block mode. One of {'linked', 'independent'}. Linked "
                   "blocks compress small blocks better, but some LZ4 decoders "
                   "are only compatible with independent blocks.",
      .parser = &block_mode_parser.argument_parser};

  static const LZ4F_blockSizeID_t block_size_mapping[] = {
      LZ4F_default, LZ4F_max64KB, LZ4F_max256KB, LZ4F_max1MB, LZ4F_max4MB};
  StringArgumentParser block_size_parser = make_string_parser(
      "-s, --block-size", "SIZE",
      (const char *[]){"default", "64KB", "256KB", "1MB", "4MB"}, 5);
  KeywordArgument block_size = {
      .short_name = 's',
      .long_name = "block-size",
      .help_text = "Maximum block size. One of {'default', '64KB', '256KB', "
                   "'1MB', '4MB'}. The larger the block size, the better the "
                   "compression ratio, but at the cost of increased memory "
                   "usage when compressing and decompressing.",
      .parser = &block_size_parser.argument_parser,
  };

  KeywordArgument favor_decompression_speed = {
      .short_name = 'd',
      .long_name = "favor-decompression-speed",
      .help_text =
          "If set, the parser will favor decompression speed over compression "
          "ratio. Only works for compression levels of at least " STRINGIFY(
              LZ4HC_CLEVEL_OPT_MIN) ".",
      .parser = NULL};

  char level_help_text[512];
  sprintf(
      level_help_text,
      "Compression level to use. An integer in the range [%d, " STRINGIFY(
          LZ4HC_CLEVEL_MAX) "]. "
                            "Negative values trigger \"fast acceleration.\"",
      INT_MIN);

  IntegerArgumentParser level_parser = make_integer_parser(
      "-l, --level", "LEVEL", (long long)INT_MIN, LZ4HC_CLEVEL_MAX);
  KeywordArgument level = {.short_name = 'l',
                           .long_name = "level",
                           .help_text = level_help_text,
                           .parser = &level_parser.argument_parser};

  KeywordArgument *keyword_args[] = {&block_mode, &block_size,
                                     &favor_decompression_speed, &level};

  Arguments arguments = {
      .executable_name = "mmap-lz4-compress",
      .version = "0.1.2",
      .author = "Gregory Meyer <me@gregjm.dev>",
      .description =
          "mmap-lz4-compress (mlc) compresses a file using the LZ4 compression "
          "algorithm. liblz4 is used for compression and memory-mapped files "
          "are used to read and write data to disk.",

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

  LZ4F_preferences_t preferences = LZ4F_INIT_PREFERENCES;

  if (favor_decompression_speed.was_found) {
    preferences.favorDecSpeed = 1;
  }

  if (level.was_found) {
    preferences.compressionLevel = (int)level_parser.value;
  }

  if (block_mode.was_found) {
    preferences.frameInfo.blockMode =
        block_mode_mapping[block_mode_parser.value_index];
  }

  if (block_size.was_found) {
    preferences.frameInfo.blockSizeID =
        block_size_mapping[block_size_parser.value_index];
  }

  FileAndMapping input_file;
  error = open_and_map_file(input_filename_parser.value, &input_file);

  if (error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  preferences.frameInfo.contentSize = (unsigned long long)input_file.size;

  const size_t output_file_size =
      LZ4F_compressFrameBound(input_file.size, &preferences);

  int return_code = EXIT_SUCCESS;
  FileAndMapping output_file;
  error = create_and_map_file(output_filename_parser.value, output_file_size,
                              &output_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;

    goto cleanup_input_only;
  }

  posix_madvise(input_file.contents, input_file.size, POSIX_MADV_SEQUENTIAL);
  posix_madvise(output_file.contents, output_file.size, POSIX_MADV_SEQUENTIAL);

  const size_t output_final_size_or_error =
      LZ4F_compressFrame(output_file.contents, output_file.size,
                         input_file.contents, input_file.size, &preferences);

  if (LZ4F_isError(output_final_size_or_error)) {
    const char *const what = LZ4F_getErrorName(output_final_size_or_error);

    print_error(eformat("couldn't compress input file '%s': %s (%zu)",
                        input_filename_parser.value, what,
                        output_final_size_or_error));
    return_code = EXIT_FAILURE;
  } else if (ftruncate(output_file.fd, (off_t)output_final_size_or_error) ==
             -1) {
    print_error(ERRNO_EFORMAT("couldn't resize output file '%s'",
                              output_filename_parser.value));
    return_code = EXIT_FAILURE;
  }

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
