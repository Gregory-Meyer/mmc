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

#ifndef COMMON_APP_H
#define COMMON_APP_H

#include <common/argparse.h>
#include <common/file.h>

#include <stddef.h>

typedef struct AppIOState AppIOState;

typedef size_t(AppSizeFunc)(size_t input_file_size, void *arg);
typedef Error(AppInitFunc)(AppIOState *app_state, void *arg);
typedef Error(AppRunFunc)(AppIOState *app_state, bool *finished, void *arg);
typedef void(AppCleanupFunc)(AppIOState *app_state, void *arg);

typedef struct AppParams {
  const char *executable_name;
  const char *version;
  const char *author;
  const char *description;

  KeywordArgument **keyword_args;
  size_t num_keyword_args;

  AppSizeFunc *size;
  AppInitFunc *init;
  AppRunFunc *run;
  AppCleanupFunc *cleanup;

  void *arg;
} AppParams;

struct AppIOState {
  FileAndMapping input_file;
  FileAndMapping output_file;

  size_t input_mapping_first_unused_offset;
  size_t output_mapping_first_unused_offset;
  size_t output_bytes_written;
};

int run_compression_app(int argc, const char *const argv[argc],
                        const AppParams *params);
int run_decompression_app(int argc, const char *const argv[argc],
                          const AppParams *params);

#endif
