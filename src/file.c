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

#include <common/file.h>

#include <assert.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

  const size_t size = (size_t)statbuf.st_size;
  void *const mapping = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

  if (mapping == MAP_FAILED) {
    close(fd);

    return ERRNO_EFORMAT("couldn't map file '%s' into memory", filename);
  }

  posix_madvise(mapping, size, POSIX_MADV_SEQUENTIAL);

  *file = (FileAndMapping){
      .filename = filename,

      .fd = fd,
      .file_size = size,

      .mapping = mapping,
      .mapping_size = size,
      .mapping_offset = 0,
  };

  return NULL_ERROR;
}

Error create_and_map_file(const char *filename, size_t size,
                          FileAndMapping *file) {
  assert(filename);
  assert(file);

  const int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if (fd == -1) {
    return ERRNO_EFORMAT("couldn't create file '%s' for writing", filename);
  }

  if (size > 0) {
    if (ftruncate(fd, (off_t)size) == -1) {
      return ERRNO_EFORMAT("couldn't set length of file '%s' to '%zu'",
                           filename, size);
    }
  }

  void *const mapping =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (mapping == MAP_FAILED) {
    close(fd);

    return ERRNO_EFORMAT("couldn't map file '%s' into memory", filename);
  }

  posix_madvise(mapping, size, POSIX_MADV_SEQUENTIAL);

  *file = (FileAndMapping){
      .filename = filename,

      .fd = fd,
      .file_size = size,

      .mapping = mapping,
      .mapping_size = size,
      .mapping_offset = 0,
  };

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

  if (munmap(file->mapping, num_bytes_to_unmap) == -1) {
    return ERRNO_EFORMAT("couldn't unmap part of file '%s' from memory",
                         file->filename);
  }

  file->mapping = (char *)file->mapping + num_bytes_to_unmap;
  file->mapping_size -= num_bytes_to_unmap;
  file->mapping_offset += num_bytes_to_unmap;
  *first_unused_offset -= num_bytes_to_unmap;

  return NULL_ERROR;
}

Error expand_output_mapping(FileAndMapping *file, size_t first_unused_offset) {
  assert(file);

  if (file->mapping_size < first_unused_offset) {
    return NULL_ERROR;
  }

  const size_t size_increment = file->file_size;
  const size_t new_size = file->file_size + size_increment;

  if (ftruncate(file->fd, (off_t)new_size) == -1) {
    return ERRNO_EFORMAT("couldn't set length of file '%s' to '%zu'",
                         file->filename, new_size);
  }

  file->file_size = new_size;

  const size_t new_mapping_size = file->mapping_size + size_increment;
  void *const new_mapping = mremap(file->mapping, file->mapping_size,
                                   new_mapping_size, MREMAP_MAYMOVE);

  if (new_mapping == MAP_FAILED) {
    return ERRNO_EFORMAT(
        "couldn't remap %zu more bytes to mapping associated with file '%s'",
        size_increment, file->filename);
  }

  posix_madvise(new_mapping, new_mapping_size, POSIX_MADV_SEQUENTIAL);

  file->mapping = new_mapping;
  file->mapping_size = new_mapping_size;

  return NULL_ERROR;
}

Error free_file(FileAndMapping file) {
  if (munmap(file.mapping, file.mapping_size) == -1) {
    close(file.fd);

    return ERRNO_EFORMAT("couldn't unmap file '%s' from memory", file.filename);
  }

  if (close(file.fd) == -1) {
    return ERRNO_EFORMAT("couldn't close file '%s'", file.filename);
  }

  return NULL_ERROR;
}
