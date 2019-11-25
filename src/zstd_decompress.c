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

#include <zstd.h>

size_t size(size_t input_file_size, void *decompression_stream_ptr_v);
Error init(AppIOState *io_state, void *decompression_stream_ptr_v);
Error run(AppIOState *io_state, bool *finished,
          void *decompression_stream_ptr_v);
void cleanup(AppIOState *io_state, void *decompression_stream_ptr_v);

int main(int argc, const char *const argv[]) {
  ZSTD_DStream *decompression_stream;

  return run_decompression_app(
      argc, argv,
      &(AppParams){
          .executable_name = "mmap-zstd-decompress",
          .version = MMC_VERSION,
          .author = MMC_AUTHOR,
          .description = "mmap-zstd-decompress (mzd) decompresses a file using "
                         "the Zstandard compression algorithm. zstd is used "
                         "for decompression and memory-mapped files are used "
                         "to read and write data to disk.",

          .size = size,
          .init = init,
          .run = run,
          .cleanup = cleanup,
          .arg = &decompression_stream,
      });
}

size_t size(size_t input_file_size, void *decompression_stream_ptr_v) {
  assert(decompression_stream_ptr_v);

  (void)decompression_stream_ptr_v;

  return input_file_size;
}

Error init(AppIOState *io_state, void *decompression_stream_ptr_v) {
  assert(io_state);
  assert(decompression_stream_ptr_v);

  ZSTD_DStream **const decompression_stream_ptr = decompression_stream_ptr_v;
  ZSTD_DStream *const decompression_stream = ZSTD_createDStream();

  if (!decompression_stream) {
    return ERROR_OUT_OF_MEMORY;
  }

  *decompression_stream_ptr = decompression_stream;

  return NULL_ERROR;
}

Error run(AppIOState *io_state, bool *finished,
          void *decompression_stream_ptr_v) {
  assert(io_state);
  assert(finished);
  assert(decompression_stream_ptr_v);

  ZSTD_DStream *const decompression_stream =
      *(ZSTD_DStream *const *)decompression_stream_ptr_v;

  assert(decompression_stream);

  ZSTD_inBuffer in_buffer = {
      .src = io_state->input_file.mapping,
      .size = io_state->input_file.mapping_size,
      .pos = io_state->input_mapping_first_unused_offset,
  };

  ZSTD_outBuffer out_buffer = {
      .dst = io_state->output_file.mapping,
      .size = io_state->output_file.mapping_size,
      .pos = io_state->output_mapping_first_unused_offset,
  };

  const size_t output_bytes_written_or_error =
      ZSTD_decompressStream(decompression_stream, &out_buffer, &in_buffer);

  if (ZSTD_isError(output_bytes_written_or_error)) {
    const char *const what = ZSTD_getErrorName(output_bytes_written_or_error);

    return eformat("couldn't decompress input file '%s': %s (%zu)",
                   io_state->input_file.filename, what,
                   output_bytes_written_or_error);
  }

  const size_t input_bytes_read =
      in_buffer.pos - io_state->input_mapping_first_unused_offset;
  const size_t output_bytes_written =
      out_buffer.pos - io_state->output_mapping_first_unused_offset;

  io_state->input_mapping_first_unused_offset += input_bytes_read;
  io_state->output_mapping_first_unused_offset += output_bytes_written;
  io_state->output_bytes_written += output_bytes_written;

  *finished = (in_buffer.pos == in_buffer.size);

  return NULL_ERROR;
}

void cleanup(AppIOState *io_state, void *decompression_stream_ptr_v) {
  assert(io_state);
  assert(decompression_stream_ptr_v);

  (void)io_state;

  ZSTD_DStream *const decompression_stream =
      *(ZSTD_DStream *const *)decompression_stream_ptr_v;

  assert(decompression_stream);

  const size_t result = ZSTD_freeDStream(decompression_stream);
  assert(!ZSTD_isError(result));
  (void)result;
}
