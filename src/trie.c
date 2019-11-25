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

#include "trie.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

KeywordArgument *find(const TrieNode *node, const char *key,
                      const char **maybe_value) {
  assert(node);
  assert(key);
  assert(maybe_value);

  if (*key == '\0') {
    *maybe_value = NULL;

    return node->value;
  } else if (*key == '=') {
    *maybe_value = key + 1;

    return node->value;
  }

  size_t index = char_to_index(*key);

  if (index == SIZE_MAX) {
    if (*key == '-') {
      index = NUM_NODE_CHILDREN - 1;
    } else {
      return NULL;
    }
  }

  const size_t child_offset = node->child_offsets[index];

  if (child_offset == 0) {
    return NULL;
  }

  return find(node + child_offset, key + 1, maybe_value);
}

size_t insert_unique(TrieArena *arena, size_t node_offset, const char *key,
                     KeywordArgument *value) {
  assert(arena);

  assert(arena->root);
  assert(arena->size > node_offset);

  assert(key);
  assert(value);

  TrieNode *this_node = &arena->root[node_offset];

  if (*key == '\0') {
    assert(!this_node->value);
    this_node->value = value;

    return node_offset;
  }

  size_t index = char_to_index(*key);

  if (index == SIZE_MAX) {
    assert(*key == '-');
    index = NUM_NODE_CHILDREN - 1;
  }

  size_t *maybe_child_offset = &this_node->child_offsets[index];

  if (*maybe_child_offset == 0) {
    if (arena->size == arena->capacity) {
      const size_t new_capacity = arena->capacity * 2;
      TrieNode *const new_root =
          realloc(arena->root, new_capacity * sizeof(TrieNode));

      if (!new_root) {
        return SIZE_MAX;
      }

      arena->root = new_root;
      arena->capacity = new_capacity;

      this_node = &arena->root[node_offset];
      maybe_child_offset = &this_node->child_offsets[index];
    }

    const size_t new_child_offset = arena->size;
    ++arena->size;

    TrieNode *const new_child = arena->root + new_child_offset;
    *new_child = (TrieNode){.child_offsets = {0}, .value = NULL};

    *maybe_child_offset = new_child_offset - node_offset;
  }

  return insert_unique(arena, node_offset + *maybe_child_offset, key + 1,
                       value);
}

size_t char_to_index(char ch) {
  if (ch >= 'a' && ch <= 'z') {
    return (size_t)(ch - 'a');
  } else if (ch >= 'A' && ch <= 'Z') {
    return (size_t)(ch - 'A') + 26;
  } else if (ch >= '0' && ch <= '9') {
    return (size_t)(ch - '0') + 52;
  } else {
    return SIZE_MAX;
  }
}
