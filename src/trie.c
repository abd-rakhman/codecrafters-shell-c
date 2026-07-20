#include "trie.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define ALPHABET_SIZE 256
#define BUFFER_SIZE 1024

typedef struct Node {
  struct Node *next[256];
  int count;
  bool complete;
} Node;

struct Trie {
  Node *root;
};

Trie *trie_create(void) {
  Trie *trie = malloc(sizeof(Trie));
  trie->root = calloc(1, sizeof(Node));
  return trie;
}

void trie_add(Trie *trie, const char *str) {
  Node *node = trie->root;
  for (const char *p = str; *p; p++) {
    node->count++;
    if (node->next[*p] == NULL) {
      Node *next = calloc(1, sizeof(Node));
      node->next[*p] = next;
    }
    node = node->next[*p];
  }
  node->count++;
  node->complete = true;
}

void dfs(Node *node, TrieResult *res, char* suffix) {
  if (node -> complete) {
    res->results[res->count] = malloc(BUFFER_SIZE * sizeof(char));
    strcpy(res->results[res->count], suffix);
    res->count++;
  }

  for (int i = 0; i < ALPHABET_SIZE; i++) {
    if (node -> next[i]) {
      char *next_suffix = malloc(BUFFER_SIZE * sizeof(char));
      strcpy(next_suffix, suffix);
      size_t len = strlen(next_suffix);
      next_suffix[len] = i;
      next_suffix[len+1] = '\0';
      dfs(node -> next[i], res, next_suffix);
      free(next_suffix);
    }
  }
}

int compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}


TrieResult *trie_autocomplete(Trie *trie, const char *str) {
  TrieResult *trie_result = malloc(sizeof(TrieResult));
  trie_result->count = 0;
  trie_result->results = malloc(BUFFER_SIZE * sizeof(char*));

  Node *node = trie->root;
  for (const char *p = str; *p; p++) {
    Node *next = node->next[*p];
    if (next == NULL) {
      return trie_result;
    }
    node = next;
  }

  char *suffix = calloc(BUFFER_SIZE, sizeof(char));
  dfs(node, trie_result, suffix);
  qsort(trie_result->results, trie_result->count, sizeof(trie_result->results[0]), compare);
  return trie_result;
}

void trie_destroy(Trie *trie) {
  // TODO: complete it

}

void trie_result_destroy(TrieResult* trie_result) {
  // TODO: complete it

}

