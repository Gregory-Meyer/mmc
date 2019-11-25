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

#ifndef COMMON_INTERNAL_TRIE_H
#define COMMON_INTERNAL_TRIE_H

#include <common/argparse.h>

#include <stddef.h>

// [A-Za-z0-9\-]
#define NUM_NODE_CHILDREN 63

typedef struct TrieNode {
  size_t child_offsets[NUM_NODE_CHILDREN];
  KeywordArgument *value;
} TrieNode;

typedef struct TrieArena {
  TrieNode *root;
  size_t size;
  size_t capacity;
} TrieArena;

KeywordArgument *find(const TrieNode *node, const char *key,
                      const char **maybe_value);
size_t insert_unique(TrieArena *arena, size_t node_offset, const char *key,
                     KeywordArgument *value);
size_t char_to_index(char ch);

#endif
