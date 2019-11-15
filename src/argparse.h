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

#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <common.h>

#include <stdbool.h>
#include <stdio.h>

typedef struct ArgumentParser {
  const char *name;
  Error (*parser)(struct ArgumentParser *self, const char *maybe_value_str);
} ArgumentParser;

typedef struct IntegerArgumentParser {
  ArgumentParser argument_parser;
  long long min_value;
  long long max_value;

  long long value;
} IntegerArgumentParser;

typedef struct StringArgumentParser {
  ArgumentParser argument_parser;
  const char **possible_values;
  size_t num_possible_values;

  size_t value_index;
} StringArgumentParser;

typedef struct PassthroughArgumentParser {
  ArgumentParser argument_parser;

  const char *value;
} PassthroughArgumentParser;

typedef struct PositionalArgument {
  const char *name;
  const char *help_text;
  ArgumentParser *parser;
} PositionalArgument;

typedef struct KeywordArgument {
  char short_name;
  const char *long_name;
  const char *help_text;
  ArgumentParser *parser;

  bool was_found;
} KeywordArgument;

typedef struct Arguments {
  const char *executable_name;
  const char *version;
  const char *author;
  const char *description;

  PositionalArgument **positional_args;
  size_t num_positional_args;

  KeywordArgument **keyword_args;
  size_t num_keyword_args;

  bool has_help;
  bool has_version;
} Arguments;

#ifdef __cplusplus
extern "C" {
#endif

IntegerArgumentParser make_integer_parser(const char *name, long long min_value,
                                          long long max_value);
StringArgumentParser make_string_parser(const char *name,
                                        const char **possible_values,
                                        size_t num_possible_values);
PassthroughArgumentParser make_passthrough_parser(const char *name);

Error parse_arguments(Arguments *arguments, int argc, char **argv);
Error print_help(const Arguments *arguments);
Error print_version(const Arguments *arguments);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
