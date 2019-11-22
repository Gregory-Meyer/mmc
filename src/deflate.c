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
#include <stdbool.h>
#include <stddef.h>

#include <zlib.h>

#define MIN(X, Y) (((Y) < (X)) ? (Y) : (X))

typedef struct State {
  IntegerArgumentParser level_parser;
  KeywordArgument level;

  StringArgumentParser strategy_parser;
  KeywordArgument strategy;

  z_stream stream;
} State;

size_t size(size_t input_file_size, void *state_v);
Error init(AppIOState *io_state, void *state_v);
Error run(AppIOState *io_state, bool *finished, void *state_v);
void cleanup(AppIOState *io_state, void *state_v);

size_t max_compressed_size(size_t uncompressed_size);

static const char *const STRATEGY_VALUES[] = {"default", "filtered",
                                              "huffman-only", "rle", "fixed"};
static const int STRATEGY_MAPPING[] = {Z_DEFAULT_STRATEGY, Z_FILTERED,
                                       Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED};

int main(int argc, const char *const argv[]) {
  State state = {
      .level_parser = make_integer_parser("-l, --level", "LEVEL",
                                          Z_NO_COMPRESSION, Z_BEST_COMPRESSION),
      .level =
          {.short_name = 'l',
           .long_name = "level",
           .help_text =
               "Compression level to use. An integer in the range [" STRINGIFY(
                   Z_NO_COMPRESSION) ", " STRINGIFY(Z_BEST_COMPRESSION) "].",
           .parser = &state.level_parser.argument_parser},

      .strategy_parser = make_string_parser("-s, --strategy", "STRATEGY",
                                            sizeof(STRATEGY_VALUES) /
                                                sizeof(STRATEGY_VALUES[0]),
                                            STRATEGY_VALUES),
      .strategy =
          {.short_name = 's',
           .long_name = "strategy",
           .help_text =
               "Compression strategy to use. One of 'default', 'filtered', "
               "'huffman-only', 'rle', or 'fixed', corresponding to the "
               "zlib compression strategies.",
           .parser = &state.strategy_parser.argument_parser},
  };

  KeywordArgument *keyword_args[] = {&state.level, &state.strategy};

  return run_compression_app(
      argc, argv,
      &(AppParams){
          .executable_name = "mmap-deflate",
          .version = MMC_VERSION,
          .author = MMC_AUTHOR,
          .description = "mmap-deflate (md) compresses a file using the "
                         "DEFLATE compression "
                         "algorithm. zlib is used for compression and "
                         "memory-mapped files are "
                         "used to read and write data to disk.",

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
  (void)state_v;

  return max_compressed_size(input_file_size);
}

Error init(AppIOState *io_state, void *state_v) {
  assert(io_state);
  assert(state_v);

  State *const state = (State *)state_v;

  int strategy_value = Z_DEFAULT_STRATEGY;
  if (state->strategy.was_found) {
    strategy_value = STRATEGY_MAPPING[state->strategy_parser.value_index];
  }

  int level_value = Z_DEFAULT_COMPRESSION;
  if (state->level.was_found) {
    level_value = (int)state->level_parser.value;
  }

  state->stream =
      (z_stream){.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};

  const int init_errc = deflateInit2(&state->stream, level_value, Z_DEFLATED,
                                     15, 8, strategy_value);

  if (init_errc != Z_OK) {
    assert(init_errc != Z_STREAM_ERROR);

    const char *what;
    switch (init_errc) {
    case Z_MEM_ERROR:
      what = "out of memory";

      break;

    case Z_VERSION_ERROR:
      what = "zlib library version mismatch";

      break;
    default:
      assert(false);
    }

    if (state->stream.msg) {
      return eformat("couldn't initialize deflate stream: %s (%d): %s", what,
                     init_errc, state->stream.msg);
    } else {
      return eformat("couldn't initialize deflate stream: %s (%d)", what,
                     init_errc);
    }
  }

  return NULL_ERROR;
}

Error run(AppIOState *io_state, bool *finished, void *state_v) {
  assert(io_state);
  assert(finished);
  assert(state_v);

  State *const state = (State *)state_v;

  z_stream *const stream = &state->stream;

  stream->next_in = (z_const Bytef *)io_state->input_file.mapping +
                    io_state->input_mapping_first_unused_offset;
  stream->avail_in = (uInt)MIN(io_state->input_file.mapping_size -
                                   io_state->input_mapping_first_unused_offset,
                               (size_t)UINT_MAX);
  stream->total_in = 0;

  stream->next_out = (Bytef *)io_state->output_file.mapping +
                     io_state->output_mapping_first_unused_offset;
  stream->avail_out =
      (uInt)MIN(io_state->output_file.mapping_size -
                    io_state->output_mapping_first_unused_offset,
                (size_t)UINT_MAX);
  stream->total_out = 0;

  int flag;

  if ((size_t)stream->avail_out >=
      max_compressed_size((size_t)stream->avail_in)) {
    flag = Z_FINISH;
  } else {
    flag = Z_NO_FLUSH;
  }

  const int errc = deflate(&state->stream, flag);

  if (errc == Z_OK || errc == Z_STREAM_END) {
    io_state->input_mapping_first_unused_offset += (size_t)stream->total_in;
    io_state->output_mapping_first_unused_offset += (size_t)stream->total_out;
    io_state->output_bytes_written += (size_t)stream->total_out;
  }

  if (errc != Z_OK) {
    assert(errc != Z_STREAM_ERROR);
    assert(errc != Z_BUF_ERROR);

    const char *what;
    switch (errc) {
    case Z_STREAM_END: {
      *finished = true;

      return NULL_ERROR;
    }
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
      assert(false);
    }

    if (state->stream.msg) {
      return eformat("couldn't inflate stream: %s (%d): %s", what, errc,
                     state->stream.msg);
    }

    return eformat("couldn't inflate stream: %s (%d)", what, errc);
  }

  *finished = false;

  return NULL_ERROR;
}

void cleanup(AppIOState *io_state, void *state_v) {
  assert(io_state);
  assert(state_v);

  (void)io_state;

  State *const state = (State *)state_v;
  deflateEnd(&state->stream);
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
