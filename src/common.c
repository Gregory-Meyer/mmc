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

#include <common.h>

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char *executable_name;

Error open_and_map_file(const char *filename, FileAndMapping *file) {
  assert(filename);
  assert(file);

  const int fd = open(filename, O_RDONLY);

  if (fd == -1) {
    return ERRNO_EFORMAT("couldn't open file '%s' for reading", filename);
  }

  struct stat statbuf;

  if (fstat(fd, &statbuf) == -1) {
    close(fd);

    return ERRNO_EFORMAT("couldn't stat file '%s'", filename);
  }

  void *const contents =
      mmap(NULL, (size_t)statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if (contents == MAP_FAILED) {
    close(fd);

    return ERRNO_EFORMAT("couldn't map file '%s' into memory", filename);
  }

  *file = (FileAndMapping){.filename = filename,
                           .fd = fd,
                           .contents = contents,
                           .size = (size_t)statbuf.st_size};

  return NULL_ERROR;
}

Error create_and_map_file(const char *filename, size_t length,
                          FileAndMapping *file) {
  assert(filename);
  assert(length > 0);
  assert(file);

  const int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if (fd == -1) {
    return ERRNO_EFORMAT("couldn't create file '%s' for writing", filename);
  }

  if (ftruncate(fd, (off_t)length) == -1) {
    return ERRNO_EFORMAT("couldn't set length of file '%s' to '%zu'", filename,
                         length);
  }

  void *const contents =
      mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (contents == MAP_FAILED) {
    close(fd);

    return ERRNO_EFORMAT("couldn't map file '%s' into memory", filename);
  }

  *file = (FileAndMapping){
      .filename = filename, .fd = fd, .contents = contents, .size = length};

  return NULL_ERROR;
}

Error unmap_unused_pages(FileAndMapping *file, size_t *first_unused_offset) {
  assert(file);
  assert(first_unused_offset);

  static const size_t UNMAP_SPAN_SIZE = 1 << 16;

  const size_t num_spans_to_unmap =
      (*first_unused_offset - 1) / UNMAP_SPAN_SIZE;

  if (num_spans_to_unmap == 0) {
    return NULL_ERROR;
  }

  const size_t num_bytes_to_unmap = num_spans_to_unmap * UNMAP_SPAN_SIZE;

  if (munmap(file->contents, num_bytes_to_unmap) == -1) {
    return ERRNO_EFORMAT("couldn't unmap part of file '%s' from memory",
                         file->filename);
  }

  file->contents = (char *)file->contents + num_bytes_to_unmap;
  file->size -= num_bytes_to_unmap;
  *first_unused_offset -= num_bytes_to_unmap;

  return NULL_ERROR;
}

Error expand_output_mapping(FileAndMapping *file, size_t *current_size,
                            size_t num_bytes_in_use) {
  assert(file);
  assert(current_size);

  if (file->size < num_bytes_in_use) {
    return NULL_ERROR;
  }

  const size_t new_size = *current_size + *current_size;

  if (ftruncate(file->fd, (off_t)new_size) == -1) {
    return ERRNO_EFORMAT("couldn't set length of file '%s' to '%zu'",
                         file->filename, new_size);
  }

  const size_t new_mapping_length = file->size + *current_size;
  void *const new_contents =
      mremap(file->contents, file->size, new_mapping_length, MREMAP_MAYMOVE);

  if (new_contents == MAP_FAILED) {
    return ERRNO_EFORMAT(
        "couldn't remap %zu more bytes to mapping associated with file '%s'",
        current_size, file->filename);
  }

  file->contents = new_contents;
  file->size = new_mapping_length;

  *current_size = new_size;

  return NULL_ERROR;
}

Error free_file(FileAndMapping file) {
  if (munmap(file.contents, file.size) == -1) {
    close(file.fd);

    return ERRNO_EFORMAT("couldn't unmap file '%s' from memory", file.filename);
  }

  if (close(file.fd) == -1) {
    return ERRNO_EFORMAT("couldn't close file '%s'", file.filename);
  }

  return NULL_ERROR;
}

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
