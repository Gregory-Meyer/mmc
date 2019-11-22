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
#include <common/error.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <zlib.h>

#define MIN(X, Y) (((Y) < (X)) ? (Y) : (X))

size_t size(size_t input_file_size, void *stream_v);
Error init(AppIOState *io_state, void *stream_v);
Error run(AppIOState *io_state, bool *finished, void *stream_v);
void cleanup(AppIOState *io_state, void *stream_v);

int main(int argc, const char *const argv[]) {
  z_stream stream;

  return run_decompression_app(
      argc, argv,
      &(AppParams){
          .executable_name = "mmap-inflate",
          .version = "0.2.1",
          .author = "Gregory Meyer <me@gregjm.dev>",
          .description =
              "mmap-inflate (mi) decompresses a file that was compressed by "
              "mmap-deflate (md) using the DEFLATE compression algorithm. zlib "
              "is "
              "used for decompression and memory-mapped files are used to read "
              "and "
              "write data to disk.",

          .size = size,
          .init = init,
          .run = run,
          .cleanup = cleanup,
          .arg = &stream,
      });
}

size_t size(size_t input_file_size, void *stream_v) {
  assert(stream_v);

  (void)stream_v;

  return input_file_size;
}

Error init(AppIOState *io_state, void *stream_v) {
  assert(io_state);
  assert(stream_v);

  z_stream *const stream = (z_stream *)stream_v;

  *stream = (z_stream){.next_in = NULL,
                       .avail_in = 0,
                       .zalloc = Z_NULL,
                       .zfree = Z_NULL,
                       .opaque = Z_NULL};

  const int init_errc = inflateInit(stream);

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

    if (stream->msg) {
      return eformat("couldn't initialize inflate stream: %s (%d): %s", what,
                     init_errc, stream->msg);
    } else {
      return eformat("couldn't initialize inflate stream: %s (%d)", what,
                     init_errc);
    }
  }

  return NULL_ERROR;
}

Error run(AppIOState *io_state, bool *finished, void *stream_v) {
  assert(io_state);
  assert(finished);
  assert(stream_v);

  z_stream *const stream = (z_stream *)stream_v;

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

  if ((size_t)stream->avail_out / 1032 > (size_t)stream->avail_in) {
    flag = Z_FINISH;
  } else {
    flag = Z_NO_FLUSH;
  }

  const int errc = inflate(stream, flag);

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
      assert(false);
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

void cleanup(AppIOState *io_state, void *stream_v) {
  assert(io_state);
  assert(stream_v);

  (void)io_state;

  z_stream *const stream = (z_stream *)stream_v;
  inflateEnd(stream);
}
