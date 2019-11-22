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
#include <common/mmc.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <lz4frame.h>

size_t size(size_t input_file_size, void *decompression_context_ptr_v);
Error init(AppIOState *io_state, void *decompression_context_ptr_v);
Error run(AppIOState *io_state, bool *finished,
          void *decompression_context_ptr_v);
void cleanup(AppIOState *io_state, void *decompression_context_ptr_v);

int main(int argc, const char *const argv[]) {
  LZ4F_dctx *decompression_context = NULL;

  return run_decompression_app(
      argc, argv,
      &(AppParams){
          .executable_name = "mmap-lz4-decompress",
          .version = MMC_VERSION,
          .author = MMC_AUTHOR,
          .description =
              "mmap-lz4-decompress (mld) uncompresses a file using the LZ4 "
              "compression algorithm. liblz4 is used for decompression and "
              "memory-mapped files are used to read and write data to disk.",

          .size = size,
          .init = init,
          .run = run,
          .cleanup = cleanup,
          .arg = &decompression_context,
      });
}

size_t size(size_t input_file_size, void *decompression_context_ptr_v) {
  assert(decompression_context_ptr_v);

  (void)decompression_context_ptr_v;

  return input_file_size;
}

Error init(AppIOState *io_state, void *decompression_context_ptr_v) {
  assert(io_state);
  assert(decompression_context_ptr_v);

  LZ4F_dctx **const decompression_context_ptr =
      (LZ4F_dctx **)decompression_context_ptr_v;

  const LZ4F_errorCode_t errc =
      LZ4F_createDecompressionContext(decompression_context_ptr, LZ4F_VERSION);

  if (LZ4F_isError(errc)) {
    const char *const what = LZ4F_getErrorName(errc);

    return eformat("couldn't initialize decompression context: %s (%zu)", what,
                   errc);
  }

  return NULL_ERROR;
}

Error run(AppIOState *io_state, bool *finished,
          void *decompression_context_ptr_v) {
  assert(io_state);
  assert(finished);
  assert(decompression_context_ptr_v);

  LZ4F_dctx **const decompression_context_ptr =
      (LZ4F_dctx **)decompression_context_ptr_v;

  size_t input_unused_length_or_bytes_consumed =
      io_state->input_file.mapping_size -
      io_state->input_mapping_first_unused_offset;
  size_t output_unused_length_or_bytes_consumed =
      io_state->output_file.mapping_size -
      io_state->output_mapping_first_unused_offset;
  const size_t maybe_decompress_errc =
      LZ4F_decompress(*decompression_context_ptr,
                      (char *)io_state->output_file.mapping +
                          io_state->output_mapping_first_unused_offset,
                      &output_unused_length_or_bytes_consumed,
                      (const char *)io_state->input_file.mapping +
                          io_state->input_mapping_first_unused_offset,
                      &input_unused_length_or_bytes_consumed, NULL);

  if (LZ4F_isError(maybe_decompress_errc)) {
    const char *const what = LZ4F_getErrorName(maybe_decompress_errc);

    return eformat("couldn't decompress stream: %s (%zu)", what,
                   maybe_decompress_errc);
  }

  io_state->input_mapping_first_unused_offset +=
      input_unused_length_or_bytes_consumed;
  io_state->output_mapping_first_unused_offset +=
      output_unused_length_or_bytes_consumed;
  io_state->output_bytes_written += output_unused_length_or_bytes_consumed;

  *finished = io_state->input_mapping_first_unused_offset ==
              io_state->input_file.mapping_size;

  return NULL_ERROR;
}

void cleanup(AppIOState *io_state, void *decompression_context_ptr_v) {
  assert(io_state);
  assert(decompression_context_ptr_v);

  LZ4F_dctx **const decompression_context_ptr =
      (LZ4F_dctx **)decompression_context_ptr_v;

  LZ4F_freeDecompressionContext(*decompression_context_ptr);
}
