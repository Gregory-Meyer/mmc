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
#include <stddef.h>
#include <stdlib.h>

#include <zstd.h>

typedef struct State {
  IntegerArgumentParser level_parser;
  KeywordArgument level;

  StringArgumentParser strategy_parser;
  KeywordArgument strategy;

  ZSTD_CCtx *compression_context;
} State;

size_t size(size_t input_file_size, void *state_v);
Error init(AppIOState *io_state, void *state_v);
Error run(AppIOState *io_state, bool *finished, void *state_v);
void cleanup(AppIOState *io_state, void *state_v);

static const char *const STRATEGY_VALUES[] = {"fast",  "dfast",   "greedy",
                                              "lazy",  "lazy2",   "btlazy2",
                                              "btopt", "btultra", "btultra2"};
static const ZSTD_strategy STRATEGY_MAPPING[] = {
    ZSTD_fast,    ZSTD_dfast, ZSTD_greedy,  ZSTD_lazy,    ZSTD_lazy2,
    ZSTD_btlazy2, ZSTD_btopt, ZSTD_btultra, ZSTD_btultra2};

int main(int argc, const char *const argv[]) {
  const int min_level = ZSTD_minCLevel();
  const int max_level = ZSTD_maxCLevel();

  char level_help_text[512];
  sprintf(level_help_text,
          "Compression level to use. An integer in the range [%d, %d].",
          min_level, max_level);

  State state = {
      .level_parser = make_integer_parser(
          "-l, --level", "LEVEL", (long long)min_level, (long long)max_level),
      .level =
          {
              .short_name = 'l',
              .long_name = "level",
              .help_text = level_help_text,
              .parser = &state.level_parser.argument_parser,
          },

      .strategy_parser = make_string_parser("-s, --strategy", "STRATEGY",
                                            sizeof(STRATEGY_VALUES) /
                                                sizeof(STRATEGY_VALUES[0]),
                                            STRATEGY_VALUES),
      .strategy =
          {
              .short_name = 's',
              .long_name = "strategy",
              .help_text = "Compression strategy to use. One of 'fast', "
                           "'dfast', 'greedy', 'lazy', 'lazy2', 'btlazy2', "
                           "'btopt', 'btultra', or 'btultra2', corresponding "
                           "to the zstd compression strategies in increasing "
                           "order of compression ratio and time.",
              .parser = &state.strategy_parser.argument_parser,
          },
  };

  KeywordArgument *keyword_args[] = {&state.level, &state.strategy};

  return run_compression_app(
      argc, argv,
      &(AppParams){
          .executable_name = "mmap-zstd-compress",
          .version = MMC_VERSION,
          .author = MMC_AUTHOR,
          .description =
              "mmap-zstd-compress (mzc) compresses a file using the Zstandard "
              "compression algorithm. zstd is used for compression and "
              "memory-mapped files are used to read and write data to disk.",

          .keyword_args = keyword_args,
          .num_keyword_args = sizeof(keyword_args) / sizeof(keyword_args[0]),

          .size = size,
          .init = init,
          .run = run,
          .cleanup = cleanup,
          .arg = &state,
      });
}

size_t size(size_t input_file_size, void *state_v) {
  assert(state_v);

  (void)state_v;

  return ZSTD_compressBound(input_file_size);
}

Error init(AppIOState *io_state, void *state_v) {
  assert(io_state);
  assert(state_v);

  State *const state = state_v;
  ZSTD_CCtx *const compression_context = ZSTD_createCCtx();

  if (!compression_context) {
    return ERROR_OUT_OF_MEMORY;
  }

  if (state->level.was_found) {
    const size_t result =
        ZSTD_CCtx_setParameter(compression_context, ZSTD_c_compressionLevel,
                               (int)state->level_parser.value);
    assert(!ZSTD_isError(result));
    (void)result;
  }

  if (state->strategy.was_found) {
    const size_t result = ZSTD_CCtx_setParameter(
        compression_context, ZSTD_c_strategy,
        (int)STRATEGY_MAPPING[state->strategy_parser.value_index]);
    assert(!ZSTD_isError(result));
    (void)result;
  }

  // no need to call ZSTD_CCtx_setPledgedSize, as ZSTD_compress2 overwrites it

  state->compression_context = compression_context;

  return NULL_ERROR;
}

Error run(AppIOState *io_state, bool *finished, void *state_v) {
  assert(io_state);
  assert(finished);
  assert(state_v);

  State *const state = state_v;

  const size_t output_final_size_or_error = ZSTD_compress2(
      state->compression_context, io_state->output_file.mapping,
      io_state->output_file.mapping_size, io_state->input_file.mapping,
      io_state->input_file.mapping_size);

  if (ZSTD_isError(output_final_size_or_error)) {
    const char *const what = ZSTD_getErrorName(output_final_size_or_error);

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

void cleanup(AppIOState *io_state, void *state_v) {
  assert(io_state);
  assert(state_v);

  (void)io_state;

  State *const state = state_v;

  assert(state->compression_context);

  const size_t result = ZSTD_freeCCtx(state->compression_context);
  assert(!ZSTD_isError(result));
  (void)result;
}
