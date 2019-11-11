#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <zlib.h>

#define MAKE_ERROR(MESSAGE)                                                    \
  ((Error){.what = (MESSAGE), .size = sizeof(MESSAGE), .allocated = false})
#define ERRNO_EFORMAT(FORMAT, ...)                                             \
  eformat(FORMAT ": %s (%d)", __VA_ARGS__, strerror(errno), errno)

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

#ifdef __cplusplus
extern "C" {
#endif

extern const char *executable_name;

Error open_and_map_file(const char *filename, FileAndMapping *file);
Error create_and_map_file(const char *filename, size_t length,
                          FileAndMapping *file);
Error free_file(FileAndMapping file);

Error transform_mapped_file(FileAndMapping *input, FileAndMapping *output,
                            Error (*f)(z_stream *stream, bool *finished),
                            z_stream *stream);

Error eformat(const char *restrict format, ...);
int print_error(Error error);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
