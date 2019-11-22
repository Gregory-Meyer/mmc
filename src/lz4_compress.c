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

#include <common/app.h>
#include <common/argparse.h>
#include <common/error.h>
#include <common/mmc.h>

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#include <lz4frame.h>
#include <lz4hc.h>

typedef struct State {
  StringArgumentParser block_mode_parser;
  KeywordArgument block_mode;

  StringArgumentParser block_size_parser;
  KeywordArgument block_size;

  KeywordArgument favor_decompression_speed;

  IntegerArgumentParser level_parser;
  KeywordArgument level;

  LZ4F_preferences_t preferences;
} State;

size_t size(size_t input_file_size, void *state_v);
Error run(AppIOState *io_state, bool *finished, void *state_v);

static const char *const BLOCK_MODE_VALUES[] = {"linked", "independent"};
static const LZ4F_blockMode_t BLOCK_MODE_MAPPING[] = {LZ4F_blockLinked,
                                                      LZ4F_blockIndependent};

static const char *const BLOCK_SIZE_VALUES[] = {"default", "64KB", "256KB",
                                                "1MB", "4MB"};
static const LZ4F_blockSizeID_t BLOCK_SIZE_MAPPING[] = {
    LZ4F_default, LZ4F_max64KB, LZ4F_max256KB, LZ4F_max1MB, LZ4F_max4MB};

int main(int argc, const char *const argv[]) {
  char level_help_text[512];
  sprintf(
      level_help_text,
      "Compression level to use. An integer in the range [%d, " STRINGIFY(
          LZ4HC_CLEVEL_MAX) "]. "
                            "Negative values trigger \"fast acceleration.\"",
      INT_MIN);

  State state = {
      .block_mode_parser = make_string_parser("-m, --block-mode", "MODE",
                                              sizeof(BLOCK_MODE_VALUES) /
                                                  sizeof(BLOCK_MODE_VALUES[0]),
                                              BLOCK_MODE_VALUES),
      .block_mode =
          {.short_name = 'm',
           .long_name = "block-mode",
           .help_text =
               "Block mode. One of {'linked', 'independent'}. Linked "
               "blocks compress small blocks better, but some LZ4 decoders "
               "are only compatible with independent blocks.",
           .parser = &state.block_mode_parser.argument_parser},

      .block_size_parser = make_string_parser("-s, --block-size", "SIZE",
                                              sizeof(BLOCK_SIZE_VALUES) /
                                                  sizeof(BLOCK_SIZE_VALUES[0]),
                                              BLOCK_SIZE_VALUES),
      .block_size =
          {.short_name = 's',
           .long_name = "block-size",
           .help_text =
               "Maximum block size. One of {'default', '64KB', '256KB', "
               "'1MB', '4MB'}. The larger the block size, the better the "
               "compression ratio, but at the cost of increased memory "
               "usage when compressing and decompressing.",
           .parser = &state.block_size_parser.argument_parser},

      .favor_decompression_speed = {.short_name = 'd',
                                    .long_name = "favor-decompression-speed",
                                    .help_text =
                                        "If set, the parser will favor "
                                        "decompression speed over compression "
                                        "ratio. Only works for compression "
                                        "levels of at least " STRINGIFY(
                                            LZ4HC_CLEVEL_OPT_MIN) ".",
                                    .parser = NULL},

      .level_parser = make_integer_parser("-l, --level", "LEVEL",
                                          (long long)INT_MIN, LZ4HC_CLEVEL_MAX),
      .level = {.short_name = 'l',
                .long_name = "level",
                .help_text = level_help_text,
                .parser = &state.level_parser.argument_parser},

      .preferences = LZ4F_INIT_PREFERENCES,
  };

  KeywordArgument *keyword_args[] = {&state.block_mode, &state.block_size,
                                     &state.favor_decompression_speed,
                                     &state.level};

  return run_compression_app(
      argc, argv,
      &(AppParams){
          .executable_name = "mmap-lz4-compress",
          .version = MMC_VERSION,
          .author = MMC_AUTHOR,
          .description =
              "mmap-lz4-compress (mlc) compresses a file using the LZ4 "
              "compression algorithm. liblz4 is used for compression and "
              "memory-mapped files are used to read and write data to disk.",

          .keyword_args = keyword_args,
          .num_keyword_args = sizeof(keyword_args) / sizeof(keyword_args[0]),

          .size = size,
          .run = run,
          .arg = &state,
      });
}

size_t size(size_t input_file_size, void *state_v) {
  assert(state_v);

  State *const state = (State *)state_v;

  if (state->favor_decompression_speed.was_found) {
    state->preferences.favorDecSpeed = 1;
  }

  if (state->level.was_found) {
    state->preferences.compressionLevel = (int)state->level_parser.value;
  }

  if (state->block_mode.was_found) {
    state->preferences.frameInfo.blockMode =
        BLOCK_MODE_MAPPING[state->block_mode_parser.value_index];
  }

  if (state->block_size.was_found) {
    state->preferences.frameInfo.blockSizeID =
        BLOCK_SIZE_MAPPING[state->block_size_parser.value_index];
  }

  state->preferences.frameInfo.contentSize =
      (unsigned long long)input_file_size;

  return LZ4F_compressFrameBound(input_file_size, &state->preferences);
}

Error run(AppIOState *io_state, bool *finished, void *state_v) {
  assert(io_state);
  assert(finished);
  assert(state_v);

  State *const state = (State *)state_v;

  const size_t output_final_size_or_error = LZ4F_compressFrame(
      io_state->output_file.mapping, io_state->output_file.mapping_size,
      io_state->input_file.mapping, io_state->input_file.mapping_size,
      &state->preferences);

  if (LZ4F_isError(output_final_size_or_error)) {
    const char *const what = LZ4F_getErrorName(output_final_size_or_error);

    return eformat("couldn't compress input file '%s': %s (%zu)",
                   io_state->input_file.filename, what,
                   output_final_size_or_error);
  }

  io_state->input_mapping_first_unused_offset =
      io_state->input_file.mapping_size;
  io_state->output_mapping_first_unused_offset = output_final_size_or_error;
  io_state->output_bytes_written = output_final_size_or_error;

  *finished = true;

  return NULL_ERROR;
}
