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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#define COMPRESSION_INPUT_HELP_TEXT                                            \
  "Uncompressed file to read from. The current user must have the correct "    \
  "permissions to read from this file."
#define COMPRESSION_OUTPUT_HELP_TEXT_FORMAT                                    \
  "Filename of the compressed file to create. If this file already exists, "   \
  "it is truncated to length 0 before being written to. Should %s exit with "  \
  "an error after truncating this file, it will be deleted. The current user " \
  "must have write permissions in this file's parent directory and, if the "   \
  "file already exists, write permissions on this file."

#define DECOMPRESSION_INPUT_HELP_TEXT                                          \
  "Compressed file to read from. The current user must have the correct "      \
  "permissions to read from this file."
#define DECOMPRESSION_OUTPUT_HELP_TEXT_FORMAT                                  \
  "Filename of the uncompressed file to create. If this file already exists, " \
  "it is truncated to length 0 before being written to. Should %s exit with "  \
  "an error after truncating this file, it will be deleted. The current user " \
  "must have write permissions in this file's parent directory and, if the "   \
  "file already exists, write permissions on this file."

static int run_transformer_app(int argc, const char *const argv[argc],
                               const AppParams *params,
                               const char *input_help_text,
                               const char *output_help_text_format);

int run_compression_app(int argc, const char *const argv[argc],
                        const AppParams *params) {
  return run_transformer_app(argc, argv, params, COMPRESSION_INPUT_HELP_TEXT,
                             COMPRESSION_OUTPUT_HELP_TEXT_FORMAT);
}

int run_decompression_app(int argc, const char *const argv[argc],
                          const AppParams *params) {
  return run_transformer_app(argc, argv, params, DECOMPRESSION_INPUT_HELP_TEXT,
                             DECOMPRESSION_OUTPUT_HELP_TEXT_FORMAT);
}

static int run_transformer_app(int argc, const char *const argv[argc],
                               const AppParams *params,
                               const char *input_help_text,
                               const char *output_help_text_format) {
#ifndef NDEBUG
  assert(argc > 0);
  assert(argv);

  assert(params);
  assert(params->executable_name);
  assert(params->version);
  assert(params->author);

  if (params->num_keyword_args > 0) {
    assert(params->keyword_args);

    for (size_t i = 0; i < params->num_keyword_args; ++i) {
      assert(params->keyword_args[i]);
    }
  }

  assert(params->size);
  assert(params->run);

  assert(input_help_text);
  assert(output_help_text_format);
#endif

  const int required_buffer_length =
      snprintf(NULL, 0, output_help_text_format, params->executable_name);
  assert(required_buffer_length >= 0);

  const size_t output_help_text_length = (size_t)required_buffer_length;
  char *const output_help_text = malloc(output_help_text_length + 1);

  const int ret = snprintf(output_help_text, output_help_text_length + 1,
                           output_help_text_format, params->executable_name);
  assert(ret == required_buffer_length);

  PassthroughArgumentParser input_filename_parser =
      make_passthrough_parser("INPUT_FILE", NULL);
  PassthroughArgumentParser output_filename_parser =
      make_passthrough_parser("OUTPUT_FILE", NULL);

  Arguments arguments = {
      .executable_name = params->executable_name,
      .version = params->version,
      .author = params->author,
      .description = params->description,

      .positional_args =
          (PositionalArgument *[]){
              &(PositionalArgument){
                  .name = "INPUT_FILE",
                  .help_text = input_help_text,
                  .parser = &input_filename_parser.argument_parser,
              },
              &(PositionalArgument){
                  .name = "OUTPUT_FILE",
                  .help_text = output_help_text,
                  .parser = &output_filename_parser.argument_parser,
              },
          },
      .num_positional_args = 2,

      .keyword_args = params->keyword_args,
      .num_keyword_args = params->num_keyword_args,
  };

  int return_code = EXIT_SUCCESS;
  Error error = parse_arguments(&arguments, argc, argv);

  if (error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;

    goto cleanup_help;
  }

  if (arguments.has_help) {
    print_help(&arguments);

    goto cleanup_help;
  } else if (arguments.has_version) {
    print_version(&arguments);

    goto cleanup_help;
  }

  free(output_help_text);

  AppIOState io_state = {.input_mapping_first_unused_offset = 0,
                         .output_mapping_first_unused_offset = 0,
                         .output_bytes_written = 0};

  if ((error = open_and_map_file(input_filename_parser.value,
                                 &io_state.input_file)),
      error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  const size_t output_file_size =
      params->size(io_state.input_file.file_size, params->arg);

  if ((error = create_and_map_file(output_filename_parser.value,
                                   output_file_size, &io_state.output_file)),
      error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;

    goto cleanup_input_only;
  }

  if (params->init) {
    if ((error = params->init(&io_state, params->arg)), error.what) {
      print_error(error);
      return_code = EXIT_FAILURE;

      goto cleanup_files;
    }
  }

  bool finished = false;

  while (!finished) {
    if ((error = params->run(&io_state, &finished, params->arg)), error.what) {
      print_error(error);
      return_code = EXIT_FAILURE;

      goto cleanup;
    }

    // not the end of the world if we can't unmap unused pages
    if ((error =
             unmap_unused_pages(&io_state.input_file,
                                &io_state.input_mapping_first_unused_offset)),
        error.what) {
      print_warning(error);
    }

    if ((error =
             unmap_unused_pages(&io_state.output_file,
                                &io_state.output_mapping_first_unused_offset)),
        error.what) {
      print_warning(error);
    }

    if ((error = expand_output_mapping(
             &io_state.output_file,
             io_state.output_mapping_first_unused_offset)),
        error.what) {
      print_error(error);
      return_code = EXIT_FAILURE;

      goto cleanup;
    }
  }

  if (ftruncate(io_state.output_file.fd,
                (off_t)io_state.output_bytes_written) == -1) {
    print_error(ERRNO_EFORMAT("couldn't resize output file '%s'",
                              output_filename_parser.value));
    return_code = EXIT_FAILURE;
  }

cleanup:
  if (params->cleanup) {
    params->cleanup(&io_state, params->arg);
  }

cleanup_files:
  if ((error = free_file(io_state.output_file)), error.what) {
    print_error(error);
    return_code = EXIT_FAILURE;
  }

  if (return_code != EXIT_SUCCESS) {
    if (unlink(output_filename_parser.value) == -1) {
      print_error(ERRNO_EFORMAT("couldn't remove file '%s'",
                                output_filename_parser.value));
      // no need to set return_code, it is already != EXIT_SUCCESS
    }
  }

cleanup_input_only:;
  if ((error = free_file(io_state.input_file)), error.what) {
    print_error(error);

    return EXIT_FAILURE;
  }

  return return_code;

cleanup_help:
  free(output_help_text);

  return return_code;
}
