#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>

#include <zlib.h>

typedef struct Arguments {
  const char *input_filename;
  const char *output_filename;
  bool has_help;
  int level;
  int strategy;
} Arguments;

const char *const USAGE_FORMAT =
    "Usage: %s [OPTION]... INPUT_FILE OUTPUT_FILE\n"
    "Compress an INPUT_FILE into OUTPUT_FILE using zlib.\n"
    "\n"
    " -h, --help               display this message and exit\n"
    " -l, --level=LEVEL        compression level for zlib.\n"
    "                          integer in the range [0, 9].\n"
    " -s, --strategy=STRATEGY  compression strategy for zlib.\n"
    "                          one of {'default', 'filtered', 'huffman-only', "
    "\n"
    "                          'rle', or 'fixed'}.\n";

Error parse_arguments(int argc, char *argv[], Arguments *args);
size_t max_compressed_size(size_t uncompressed_size);
Error do_compress(z_stream *stream, bool *finished);

int main(int argc, char *argv[]) {
  Arguments args;
  Error error = parse_arguments(argc, argv, &args);

  if (error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  if (args.has_help) {
    printf(USAGE_FORMAT, executable_name);

    return EXIT_SUCCESS;
  }

  FileAndMapping input_file;
  error = open_and_map_file(args.input_filename, &input_file);
  int return_code = EXIT_SUCCESS;

  if (error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  FileAndMapping output_file;
  error = create_and_map_file(
      args.output_filename, max_compressed_size(input_file.size), &output_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;

    goto cleanup_input_only;
  }

  z_stream stream = {.zalloc = Z_NULL, .zfree = Z_NULL, .opaque = Z_NULL};

  const int init_errc = deflateInit(&stream, Z_BEST_COMPRESSION);

  if (init_errc != Z_OK) {
    assert(init_errc != Z_STREAM_ERROR);

    return_code = EXIT_FAILURE;

    const char *what;
    switch (init_errc) {
    case Z_MEM_ERROR:
      what = "out of memory";

      break;

    case Z_VERSION_ERROR:
      what = "zlib library version mismatch";

      break;
    default:
      abort();
    }

    if (stream.msg) {
      error = eformat("couldn't initialize deflate stream: %s (%d): %s", what,
                      init_errc, stream.msg);
    } else {
      error = eformat("couldn't initialize deflate stream: %s (%d)", what,
                      init_errc);
    }

    print_error(error);

    goto cleanup;
  }

  error =
      transform_mapped_file(&input_file, &output_file, do_compress, &stream);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

  const int deflate_end_errc = deflateEnd(&stream);
  assert(deflate_end_errc == Z_OK);

cleanup:
  error = free_file(output_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

cleanup_input_only:
  error = free_file(input_file);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

  return return_code;
}

static const struct option OPTIONS[] = {
    {.name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'},
    {.name = "level", .has_arg = optional_argument, .flag = NULL, .val = 'l'},
    {.name = "strategy",
     .has_arg = optional_argument,
     .flag = NULL,
     .val = 's'},
    {0}};

Error parse_arguments(int argc, char *argv[], Arguments *args) {
  assert(argc >= 1);
  assert(argv[0]);
  assert(args);

  executable_name = argv[0];
  opterr = false;

  bool has_help = false;
  int level = Z_DEFAULT_COMPRESSION;
  int strategy = Z_DEFAULT_STRATEGY;

  while (true) {
    int option_index;
    const int option_char =
        getopt_long(argc, argv, "hl::s::", OPTIONS, &option_index);

    if (option_char == -1) {
      break;
    }

    switch (option_char) {
    case 'h':
      has_help = true;

      break;
    case 'l': {
      const char *const level_str = optarg;

      if (!level_str) {
        return MAKE_ERROR("missing argument LEVEL for -l, --level");
      }

      char *endptr;
      const long long maybe_level = strtoll(level_str, &endptr, 10);

      if (!level_str || *endptr) {
        return eformat("couldn't parse '%s' as a compression level", level_str);
      } else if (maybe_level < Z_NO_COMPRESSION ||
                 maybe_level > Z_BEST_COMPRESSION) {
        return eformat("expected LEVEL to be in the range [%d, %d], got %lld",
                       Z_NO_COMPRESSION, Z_BEST_COMPRESSION, maybe_level);
      }

      level = (int)maybe_level;

      break;
    }

    case 's': {
      const char *const maybe_strategy_str = optarg;

      if (!maybe_strategy_str) {
        return MAKE_ERROR("missing argument STRATEGY for -s, --strategy");
      }

      if (strcmp(maybe_strategy_str, "default") == 0) {
        strategy = Z_DEFAULT_STRATEGY;
      } else if (strcmp(maybe_strategy_str, "filtered") == 0) {
        strategy = Z_FILTERED;
      } else if (strcmp(maybe_strategy_str, "huffman-only") == 0) {
        strategy = Z_HUFFMAN_ONLY;
      } else if (strcmp(maybe_strategy_str, "rle") == 0) {
        strategy = Z_RLE;
      } else if (strcmp(maybe_strategy_str, "fixed") == 0) {
        strategy = Z_FIXED;
      } else {
        return eformat(
            "invalid argument for -s, --strategy: expected one of {'default', "
            "'filtered', 'huffman-only', 'rle', or 'fixed'}, got '%s'",
            maybe_strategy_str);
      }

      break;
    }
    case '?':
      return eformat("unrecognized option '%s'", argv[optind - 1]);
    default:
      assert(false);
    }
  }

  const char *input_filename;
  if (optind >= argc) {
    if (!has_help) {
      return MAKE_ERROR("missing argument INPUT_FILE");
    } else {
      input_filename = NULL;
    }
  } else {
    input_filename = argv[optind];
  }

  const char *output_filename;
  if (optind >= argc - 1) {
    if (!has_help) {
      return MAKE_ERROR("missing argument OUTPUT_FILE");
    } else {
      output_filename = NULL;
    }
  } else {
    output_filename = argv[optind + 1];
  }

  *args = (Arguments){.input_filename = input_filename,
                      .output_filename = output_filename,
                      .has_help = has_help,
                      .level = level,
                      .strategy = strategy};

  return NULL_ERROR;
}

Error do_compress(z_stream *stream, bool *finished) {
  assert(stream);
  assert(finished);

  int flag;

  if ((size_t)stream->avail_out >=
      max_compressed_size((size_t)stream->avail_in)) {
    flag = Z_FINISH;
  } else {
    flag = Z_NO_FLUSH;
  }

  const int errc = deflate(stream, flag);

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
      abort();
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

size_t max_compressed_size(size_t uncompressed_size) {
  static const size_t BLOCK_SIZE = 16000;

  size_t num_blocks = uncompressed_size / BLOCK_SIZE; // 16 KB

  if (uncompressed_size % BLOCK_SIZE == 0) {
    ++num_blocks;
  }

  return uncompressed_size + num_blocks * 5 + 6;
}
