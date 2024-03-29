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

#include <common/argparse.h>

#include "trie.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static Error do_parse_integer(ArgumentParser *self_base,
                              const char *maybe_value_str);
static Error do_parse_string(ArgumentParser *self_base,
                             const char *maybe_value_str);
static Error do_parse_passthrough(ArgumentParser *self_base,
                                  const char *maybe_value_str);

IntegerArgumentParser make_integer_parser(const char *name,
                                          const char *metavariable,
                                          long long min_value,
                                          long long max_value) {
  assert(name);
  assert(metavariable);
  assert(min_value <= max_value);

  return (IntegerArgumentParser){
      .argument_parser = {.name = name,
                          .metavariable = metavariable,
                          .parser = do_parse_integer},
      .min_value = min_value,
      .max_value = max_value,
  };
}

StringArgumentParser
make_string_parser(const char *name, const char *metavariable,
                   size_t num_possible_values,
                   const char *const possible_values[num_possible_values]) {
  assert(name);
  assert(metavariable);

#ifndef NDEBUG
  if (num_possible_values > 0) {
    assert(possible_values);

    for (size_t i = 0; i < num_possible_values; ++i) {
      assert(possible_values[i]);
    }
  }
#endif

  return (StringArgumentParser){
      .argument_parser = {.name = name,
                          .metavariable = metavariable,
                          .parser = do_parse_string},
      .possible_values = possible_values,
      .num_possible_values = num_possible_values,
  };
}

PassthroughArgumentParser make_passthrough_parser(const char *name,
                                                  const char *metavariable) {
  assert(name);

  return (PassthroughArgumentParser){
      .argument_parser = {.name = name,
                          .metavariable = metavariable,
                          .parser = do_parse_passthrough}};
}

static int keyword_argument_long_name_strcmp(const void *lhs_v,
                                             const void *rhs_v);
static int keyword_argument_short_name_strcmp(const void *lhs_v,
                                              const void *rhs_v);

Error parse_arguments(Arguments *arguments, int argc,
                      const char *const argv[argc]) {
#ifndef NDEBUG
  assert(arguments);

  assert(arguments->executable_name);
  assert(arguments->version);
  assert(arguments->author);

  assert(argc > 0);
  assert(argv);

  // check for NULL argv elements
  for (size_t i = 0; i < (size_t)argc; ++i) {
    assert(argv[i]);
  }

  // check for keyword argument programmer errors
  for (size_t i = 0; i < arguments->num_keyword_args; ++i) {
    const KeywordArgument *const this_keyword_arg = arguments->keyword_args[i];
    assert(this_keyword_arg);

    assert(this_keyword_arg->short_name != 'h');
    assert(this_keyword_arg->short_name != 'v');
    assert(char_to_index(this_keyword_arg->short_name) != SIZE_MAX);

    assert(this_keyword_arg->long_name);
    assert(strcmp(this_keyword_arg->long_name, "help") != 0);
    assert(strcmp(this_keyword_arg->long_name, "version") != 0);

    for (const char *ch = this_keyword_arg->long_name; *ch != '\0'; ++ch) {
      assert(char_to_index(*ch) != SIZE_MAX || *ch == '-');
    }

    if (this_keyword_arg->parser) {
      assert(this_keyword_arg->parser->parser);
      assert(this_keyword_arg->parser->name);
      assert(this_keyword_arg->parser->metavariable);
    }
  }

  // check for positional argument programmer errors
  for (size_t i = 0; i < arguments->num_positional_args; ++i) {
    const PositionalArgument *const this_positional_arg =
        arguments->positional_args[i];

    assert(this_positional_arg);

    assert(this_positional_arg->name);

    assert(this_positional_arg->parser);
    assert(this_positional_arg->parser->parser);
    assert(this_positional_arg->parser->name);
  }

  // check for duplicate short names
  qsort(arguments->keyword_args, arguments->num_keyword_args,
        sizeof(KeywordArgument *), keyword_argument_short_name_strcmp);

  for (size_t i = 1; i < arguments->num_keyword_args; ++i) {
    const KeywordArgument *const first_keyword_arg =
        arguments->keyword_args[i - 1];
    const KeywordArgument *const second_keyword_arg =
        arguments->keyword_args[i];

    assert(first_keyword_arg->short_name != second_keyword_arg->short_name);
  }
#endif

  executable_name = argv[0];
  qsort(arguments->keyword_args, arguments->num_keyword_args,
        sizeof(KeywordArgument *), keyword_argument_long_name_strcmp);

#ifndef NDEBUG
  // check for duplicate long names
  for (size_t i = 1; i < arguments->num_keyword_args; ++i) {
    const KeywordArgument *const first_keyword_arg =
        arguments->keyword_args[i - 1];
    const KeywordArgument *const second_keyword_arg =
        arguments->keyword_args[i];

    assert(strcmp(first_keyword_arg->long_name,
                  second_keyword_arg->long_name) != 0);
  }
#endif

  KeywordArgument *short_option_mapping[NUM_NODE_CHILDREN - 1] = {NULL};

  for (size_t i = 0; i < arguments->num_keyword_args; ++i) {
    KeywordArgument *const this_keyword_arg = arguments->keyword_args[i];
    const size_t index = char_to_index(this_keyword_arg->short_name);

    short_option_mapping[index] = this_keyword_arg;
  }

  size_t last_index = (size_t)argc;
  arguments->has_help = false;
  arguments->has_version = false;

  for (size_t i = 1; i < (size_t)argc; ++i) {
    const char *const this_argument = argv[i];

    if (strcmp(this_argument, "--") == 0) {
      last_index = i;

      break;
    } else if (this_argument[0] != '-') {
      // positional argument
      continue;
    }

    if (argv[i][1] == '-') {
      // long option
      if (strcmp(this_argument + 2, "help") == 0) {
        arguments->has_help = true;

        return NULL_ERROR;
      } else if (strcmp(this_argument + 2, "version") == 0) {
        arguments->has_version = true;

        return NULL_ERROR;
      }
    } else {
      // short option(s)

      for (const char *ch = this_argument + 1; *ch != '\0'; ++ch) {
        if (*ch == 'h') {
          arguments->has_help = true;

          return NULL_ERROR;
        } else if (*ch == 'v') {
          arguments->has_version = true;

          return NULL_ERROR;
        } else {
          const size_t index = char_to_index(*ch);

          if (index == SIZE_MAX) {
            continue;
          }

          KeywordArgument *const selected_arg = short_option_mapping[index];

          if (!selected_arg) {
            continue;
          }

          if (selected_arg->parser) {
            break;
          }
        }
      }
    }
  }

  TrieArena arena;
  Error error = NULL_ERROR;

  if (arguments->num_keyword_args > 0) {
    arena = (TrieArena){
        .root = malloc(sizeof(TrieNode) * 8), .size = 1, .capacity = 8};

    if (!arena.root) {
      return ERROR_OUT_OF_MEMORY;
    }

    arena.root[0] = (TrieNode){.child_offsets = {0}, .value = NULL};

    for (size_t i = 0; i < arguments->num_keyword_args; ++i) {
      KeywordArgument *const this_keyword_arg = arguments->keyword_args[i];

      if (insert_unique(&arena, 0, this_keyword_arg->long_name,
                        this_keyword_arg) == SIZE_MAX) {
        error = ERROR_OUT_OF_MEMORY;

        goto cleanup;
      }
    }
  } else {
    arena = (TrieArena){.root = NULL, .size = 0, .capacity = 0};
  }

  size_t positional_arg_index = 0;
  for (size_t i = 1; i < last_index; ++i) {
    const char *const this_argument = argv[i];

    if (this_argument[0] != '-') {
      // positional argument
      if (positional_arg_index >= arguments->num_positional_args) {
        error =
            eformat("expected %zu positional arguments, got at least %zu",
                    arguments->num_positional_args, positional_arg_index + 1);

        goto cleanup;
      }

      PositionalArgument *const this_positional_arg =
          arguments->positional_args[positional_arg_index];
      ++positional_arg_index;

      error = this_positional_arg->parser->parser(this_positional_arg->parser,
                                                  this_argument);

      if (error.what) {
        goto cleanup;
      }

      continue;
    }

    if (this_argument[1] == '-') {
      // long option
      if (arguments->num_keyword_args == 0) {
        // no cleanup to do
        return eformat("unrecognized option --%s", this_argument + 2);
      }

      const char *maybe_value;
      KeywordArgument *const this_keyword_arg =
          find(arena.root, this_argument + 2, &maybe_value);

      if (!this_keyword_arg) {
        error = eformat("unrecognized option --%s", this_argument + 2);

        goto cleanup;
      }

      if (!maybe_value) {
        // --key value
        if (i + 1 >= last_index) {
          error = eformat("missing required argument %s for option -%c, --%s",
                          this_keyword_arg->parser->metavariable,
                          this_keyword_arg->short_name,
                          this_keyword_arg->long_name);

          goto cleanup;
        }

        maybe_value = argv[i + 1];
        ++i;
      } // otherwise, --key=value

      error = this_keyword_arg->parser->parser(this_keyword_arg->parser,
                                               maybe_value);

      if (error.what) {
        goto cleanup;
      }
    } else {
      // short option(s)
      if (arguments->num_keyword_args == 0) {
        // no cleanup to do
        return eformat("unrecognized option -%c", this_argument[1]);
      }

      for (const char *ch = this_argument + 1; *ch != '\0'; ++ch) {
        const size_t index = char_to_index(*ch);

        if (index == SIZE_MAX) {
          error = eformat("unrecognized option -%c", *ch);

          goto cleanup;
        }

        KeywordArgument *const this_keyword_arg = short_option_mapping[index];

        if (!this_keyword_arg) {
          error = eformat("unrecognized option -%c", *ch);

          goto cleanup;
        }

        if (!this_keyword_arg->parser) {
          this_keyword_arg->was_found = true;

          continue;
        }

        const char *maybe_value;
        bool contains_value;

        if (*(ch + 1) == '\0') {
          // -k value
          if (i + 1 >= last_index) {
            error = eformat("missing required argument %s for option -%c, --%s",
                            this_keyword_arg->parser->metavariable,
                            this_keyword_arg->short_name,
                            this_keyword_arg->long_name);

            goto cleanup;
          }

          maybe_value = argv[i + 1];
          assert(maybe_value);
          ++i;

          contains_value = false;
        } else if (*(ch + 1) == '=') {
          // -k=value
          maybe_value = ch + 2;
          contains_value = true;
        } else {
          // -kvalue
          maybe_value = ch + 1;
          contains_value = true;
        }

        error = this_keyword_arg->parser->parser(this_keyword_arg->parser,
                                                 maybe_value);

        if (error.what) {
          goto cleanup;
        }

        if (contains_value) {
          break;
        }
      }
    }
  }

  for (size_t i = last_index + 1; i < (size_t)argc; ++i) {
    if (positional_arg_index >= arguments->num_positional_args) {
      const size_t num_positional_args = (size_t)argc - (last_index + 1);

      error = eformat("expected %zu positional arguments, got %zu",
                      arguments->num_positional_args, num_positional_args);

      break;
    }

    PositionalArgument *const this_positional_arg =
        arguments->positional_args[positional_arg_index];
    ++positional_arg_index;

    assert(this_positional_arg);
    assert(this_positional_arg->parser);

    error = this_positional_arg->parser->parser(this_positional_arg->parser,
                                                argv[i]);

    if (error.what) {
      break;
    }
  }

  if (positional_arg_index < arguments->num_positional_args) {
    const PositionalArgument *const this_positional_arg =
        arguments->positional_args[positional_arg_index];

    error = eformat("missing required positional argument %s",
                    this_positional_arg->name);
  }

cleanup:
  free(arena.root);

  return error;
}

#define UNWRITEABLE_HELP_TEXT()                                                \
  ERRNO_EFORMAT("couldn't write help text to file")

static Error print_paragraph(const char *paragraph, size_t indent);
static Error print_help_info(void);
static Error print_version_info(void);
static size_t
lower_bound(size_t num_keyword_args,
            const KeywordArgument *const keyword_args[num_keyword_args],
            const char *long_name);

Error print_help(const Arguments *arguments) {
  assert(arguments);

  assert(arguments->executable_name);
  assert(arguments->version);
  assert(arguments->author);

  if (printf("%s %s\n%s", arguments->executable_name, arguments->version,
             arguments->author) < 0) {
    return UNWRITEABLE_HELP_TEXT();
  }

  Error error;

  if (arguments->description) {
    if (putchar('\n') == EOF) {
      return UNWRITEABLE_HELP_TEXT();
    }

    error = print_paragraph(arguments->description, 0);

    if (error.what) {
      return error;
    }
  }

  if (printf("\n\nUSAGE:\n    %s [OPTIONS]", executable_name) < 0) {
    return UNWRITEABLE_HELP_TEXT();
  }

  for (size_t i = 0; i < arguments->num_positional_args; ++i) {
    if (printf(" %s", arguments->positional_args[i]->name) < 0) {
      return UNWRITEABLE_HELP_TEXT();
    }
  }

  if (arguments->num_positional_args > 0) {
    if (fputs("\n\nARGS:", stdout) == EOF) {
      return UNWRITEABLE_HELP_TEXT();
    }

    for (size_t i = 0; i < arguments->num_positional_args; ++i) {
      const PositionalArgument *const this_positional_arg =
          arguments->positional_args[i];
      assert(this_positional_arg);

      if (printf("\n    <%s>", this_positional_arg->name) < 0) {
        return UNWRITEABLE_HELP_TEXT();
      }

      const char *const maybe_help_text = this_positional_arg->help_text;

      if (maybe_help_text) {
        error = print_paragraph(maybe_help_text, 12);

        if (error.what) {
          return error;
        }
      }

      if (i + 1 < arguments->num_positional_args) {
        if (putchar('\n') == EOF) {
          return UNWRITEABLE_HELP_TEXT();
        }
      }
    }
  }

  if (fputs("\n\nOPTIONS:", stdout) == EOF) {
    return UNWRITEABLE_HELP_TEXT();
  }

  bool printed_help = false;
  bool printed_version = false;

  const size_t help_before_index = lower_bound(
      arguments->num_keyword_args,
      (const KeywordArgument *const *)arguments->keyword_args, "help");
  const size_t version_before_index = lower_bound(
      arguments->num_keyword_args,
      (const KeywordArgument *const *)arguments->keyword_args, "version");

  for (size_t i = 0; i < arguments->num_keyword_args; ++i) {
    if (i == help_before_index) {
      error = print_help_info();

      if (error.what) {
        return error;
      }

      if (putchar('\n') == EOF) {
        return UNWRITEABLE_HELP_TEXT();
      }

      printed_help = true;
    }

    if (i == version_before_index) {
      error = print_version_info();

      if (error.what) {
        return error;
      }

      if (putchar('\n') == EOF) {
        return UNWRITEABLE_HELP_TEXT();
      }

      printed_version = true;
    }

    const KeywordArgument *const this_keyword_arg = arguments->keyword_args[i];
    assert(this_keyword_arg);

    if (this_keyword_arg->parser) {
      assert(this_keyword_arg->parser->metavariable);

      if (printf("\n    -%c, --%s=%s", this_keyword_arg->short_name,
                 this_keyword_arg->long_name,
                 this_keyword_arg->parser->metavariable) < 0) {
        return UNWRITEABLE_HELP_TEXT();
      }
    } else {
      if (printf("\n    -%c, --%s", this_keyword_arg->short_name,
                 this_keyword_arg->long_name) < 0) {
        return UNWRITEABLE_HELP_TEXT();
      }
    }

    const char *const maybe_help_text = this_keyword_arg->help_text;

    if (maybe_help_text) {
      error = print_paragraph(maybe_help_text, 12);

      if (error.what) {
        return error;
      }
    }

    if (i + 1 < arguments->num_positional_args || !printed_help ||
        !printed_version) {
      if (putchar('\n') == EOF) {
        return UNWRITEABLE_HELP_TEXT();
      }
    }
  }

  if (!printed_help) {
    error = print_help_info();

    if (error.what) {
      return error;
    }

    if (putchar('\n') == EOF) {
      return UNWRITEABLE_HELP_TEXT();
    }
  }

  if (!printed_version) {
    error = print_version_info();

    if (error.what) {
      return error;
    }
  }

  if (putchar('\n') == EOF) {
    return UNWRITEABLE_HELP_TEXT();
  }

  return NULL_ERROR;
}

Error print_version(const Arguments *arguments) {
  assert(arguments);

  if (printf("%s %s\n", arguments->executable_name, arguments->version) < 0) {
    return UNWRITEABLE_HELP_TEXT();
  }

  return NULL_ERROR;
}

static Error print_paragraph(const char *paragraph, size_t indent) {
  static const size_t MAX_NUM_COLUMNS = 80;

  assert(paragraph);
  assert(indent < MAX_NUM_COLUMNS);

  const size_t length = strlen(paragraph);
  size_t num_paragraph_written = 0;

  while (num_paragraph_written < length) {
    size_t line_length = 0;
    size_t restricted_length;

    const bool can_print_entire_line =
        length - num_paragraph_written < MAX_NUM_COLUMNS - indent;

    if (can_print_entire_line) {
      restricted_length = length - num_paragraph_written;
    } else {
      restricted_length = MAX_NUM_COLUMNS - indent;
    }

    const char *const newline_ptr =
        memchr(paragraph + num_paragraph_written, '\n', restricted_length);

    if (newline_ptr) {
      line_length = (size_t)(newline_ptr - (paragraph + num_paragraph_written));
    } else if (!can_print_entire_line) {
      size_t space_index = 0;

      // memrchr? never heard of it
      for (size_t i = restricted_length - 1; i < restricted_length; --i) {
        if (paragraph[num_paragraph_written + i] == ' ') {
          space_index = i;
          break;
        }
      }

      line_length = space_index;
    } else {
      line_length = restricted_length;
    }

    if (putchar('\n') == EOF) {
      return UNWRITEABLE_HELP_TEXT();
    }

    for (size_t i = 0; i < indent; ++i) {
      if (putchar(' ') == EOF) {
        return UNWRITEABLE_HELP_TEXT();
      }
    }

    const int result =
        printf("%.*s", (int)line_length, paragraph + num_paragraph_written);

    if (result < 0) {
      return UNWRITEABLE_HELP_TEXT();
    }

    num_paragraph_written += (size_t)line_length + 1;

    if (newline_ptr) {
      if (putchar('\n') == EOF) {
        return UNWRITEABLE_HELP_TEXT();
      }
    }
  }

  return NULL_ERROR;
}

static size_t
lower_bound(size_t num_keyword_args,
            const KeywordArgument *const keyword_args[num_keyword_args],
            const char *long_name) {
  assert(long_name);

  if (num_keyword_args == 0) {
    return 0;
  }

  size_t left = 0;
  size_t right = num_keyword_args;

  while (left < right) {
    const size_t middle = (left + right) / 2;
    assert(middle < num_keyword_args);

    const int comparison = strcmp(keyword_args[middle]->long_name, long_name);

    if (comparison < 0) {
      left = middle + 1;
    } else {
      right = middle;
    }
  }

  return left;
}

static Error print_help_info(void) {
  if (printf("\n    -h, --help\n            Prints help information.") < 0) {
    return UNWRITEABLE_HELP_TEXT();
  }

  return NULL_ERROR;
}
static Error print_version_info(void) {
  if (printf("\n    -v, --version\n            Prints version information.") <
      0) {
    return UNWRITEABLE_HELP_TEXT();
  }

  return NULL_ERROR;
}

static Error do_parse_integer(ArgumentParser *self_base,
                              const char *maybe_value_str) {
  assert(self_base);
  assert(maybe_value_str);

  IntegerArgumentParser *const self = (IntegerArgumentParser *)self_base;

  errno = 0;
  char *end;
  const long long maybe_value = strtoll(maybe_value_str, &end, 10);

  if (*maybe_value_str == '\0' || *end != '\0') {
    return eformat("invalid argument for %s: couldn't parse '%s' as an integer",
                   self_base->name, maybe_value_str);
  }

  if (errno != 0) {
    assert(errno == ERANGE);

    return eformat("invalid argument for %s: expected an integer in the range "
                   "[%lld, %lld], got %s",
                   self_base->name, self->min_value, self->max_value,
                   maybe_value_str);
  }

  if (maybe_value < self->min_value || maybe_value > self->max_value) {
    return eformat("invalid argument for %s: expected an integer in the range "
                   "[%lld, %lld], got %lld",
                   self_base->name, self->min_value, self->max_value,
                   maybe_value);
  }

  self->value = maybe_value;

  return NULL_ERROR;
}

static char *stringify_string_array(const char *const *strings,
                                    size_t num_strings);

static Error do_parse_string(ArgumentParser *self_base,
                             const char *maybe_value_str) {
  assert(self_base);
  assert(maybe_value_str);

  StringArgumentParser *const self = (StringArgumentParser *)self_base;

  for (size_t i = 0; i < self->num_possible_values; ++i) {
    assert(self->possible_values[i]);

    if (strcmp(maybe_value_str, self->possible_values[i]) == 0) {
      self->value_index = i;

      return NULL_ERROR;
    }
  }

  char *possible_values_str =
      stringify_string_array(self->possible_values, self->num_possible_values);

  if (!possible_values_str) {
    return ERROR_OUT_OF_MEMORY;
  }

  const Error error =
      eformat("invalid argument for %s: expected one of %s, got '%s'",
              self_base->name, possible_values_str, maybe_value_str);
  free(possible_values_str);

  return error;
}

static Error do_parse_passthrough(ArgumentParser *self_base,
                                  const char *maybe_value_str) {
  assert(self_base);
  assert(maybe_value_str);

  PassthroughArgumentParser *const self =
      (PassthroughArgumentParser *)self_base;
  self->value = maybe_value_str;

  return NULL_ERROR;
}

static char *stringify_string_array(const char *const *strings,
                                    size_t num_strings) {
  if (num_strings == 0) {
    char *buffer = malloc(3);

    if (!buffer) {
      return NULL;
    }

    memcpy(buffer, "{}", 3);

    return buffer;
  }

  assert(strings);

  // two characters per array for the {}
  // two characters per string for the ''
  // two characters per every string but the last, for the comma and space
  // one character for the null terminator
  size_t buffer_size = 2 + 2 * num_strings + 2 * (num_strings - 1) + 1;

  size_t *const lengths = malloc(num_strings * sizeof(size_t));

  if (!lengths) {
    return NULL;
  }

  for (size_t i = 0; i < num_strings; ++i) {
    lengths[i] = strlen(strings[i]);
    buffer_size += lengths[i];
  }

  char *buffer = malloc(buffer_size);

  if (!buffer) {
    free(lengths);

    return NULL;
  }

  buffer[0] = '{';

  size_t destination_offset = 1;

  for (size_t i = 0; i < num_strings; ++i) {
    if (i != 0) {
      memcpy(buffer + destination_offset, "', '", 4);
      destination_offset += 4;
    } else {
      buffer[destination_offset] = '\'';
      ++destination_offset;
    }

    memcpy(buffer + destination_offset, strings[i], lengths[i]);
    destination_offset += lengths[i];
  }

  free(lengths);

  // including null terminator
  memcpy(buffer + destination_offset, "'}", 3);
  destination_offset += 3;

  assert(destination_offset == buffer_size);

  return buffer;
}

static int keyword_argument_long_name_strcmp(const void *lhs_v,
                                             const void *rhs_v) {
  assert(lhs_v);
  assert(rhs_v);

  const KeywordArgument *const lhs = *(const KeywordArgument *const *)lhs_v;
  const KeywordArgument *const rhs = *(const KeywordArgument *const *)rhs_v;

  return strcmp(lhs->long_name, rhs->long_name);
}

static int keyword_argument_short_name_strcmp(const void *lhs_v,
                                              const void *rhs_v) {
  assert(lhs_v);
  assert(rhs_v);

  const KeywordArgument *const lhs = *(const KeywordArgument *const *)lhs_v;
  const KeywordArgument *const rhs = *(const KeywordArgument *const *)rhs_v;

  if (lhs->short_name < rhs->short_name) {
    return -1;
  } else if (lhs->short_name > rhs->short_name) {
    return 1;
  }

  return 0;
}
