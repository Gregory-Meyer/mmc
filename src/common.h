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

#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define DO_STRINGIFY(X) #X
#define STRINGIFY(X) DO_STRINGIFY(X)

#define STATIC_ERROR(MESSAGE)                                                  \
  ((Error){.what = (MESSAGE), .size = sizeof(MESSAGE), .allocated = false})
#define DO_ERRNO_EFORMAT(FORMAT, ...)                                          \
  eformat(FORMAT "%s: %s (%d)", __VA_ARGS__, strerror(errno), errno)
#define ERRNO_EFORMAT(...) DO_ERRNO_EFORMAT(__VA_ARGS__, "")
#define ERROR_OUT_OF_MEMORY STATIC_ERROR("out of memory")
#define NULL_ERROR ((Error){.what = NULL, .size = 0, .allocated = false})

typedef struct Error {
  char *what;
  size_t size;
  bool allocated;
} Error;

typedef struct FileAndMapping {
  const char *filename;
  int fd;
  void *contents;
  size_t size;
} FileAndMapping;

extern const char *executable_name;

Error open_and_map_file(const char *filename, FileAndMapping *file);
Error create_and_map_file(const char *filename, size_t length,
                          FileAndMapping *file);
Error unmap_unused_pages(FileAndMapping *file, size_t *first_unused_offset);
Error expand_output_mapping(FileAndMapping *file, size_t *current_size,
                            size_t num_bytes_in_use);
Error free_file(FileAndMapping file);

Error eformat(const char *format, ...);
int print_error(Error error);

#endif
