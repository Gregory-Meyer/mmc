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

#include <common/error.h>

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

const char *executable_name;

Error eformat(const char *format, ...) {
  va_list first_args;
  va_start(first_args, format);

  va_list second_args;
  va_copy(second_args, first_args);

  const int minimum_buffer_size = vsnprintf(NULL, 0, format, first_args);
  va_end(first_args);
  assert(minimum_buffer_size >= 0);

  const size_t buffer_size = (size_t)minimum_buffer_size + 1;

  Error error = {
      .what = malloc(buffer_size), .size = buffer_size, .allocated = true};

  if (!error.what) {
    va_end(second_args);

    return ERROR_OUT_OF_MEMORY;
  }

  const int result = vsnprintf(error.what, error.size, format, second_args);
  assert(result == minimum_buffer_size);
  va_end(second_args);

  return error;
}

int print_error(Error error) {
  assert(error.what);

  const int result = fprintf(stderr, "%s: error: %.*s\n", executable_name,
                             (int)error.size, error.what);

  if (error.allocated) {
    free(error.what);
  }

  return result;
}

int print_warning(Error error) {
  assert(error.what);

  const int result = fprintf(stderr, "%s: warning: %.*s\n", executable_name,
                             (int)error.size, error.what);

  if (error.allocated) {
    free(error.what);
  }

  return result;
}
