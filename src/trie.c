#include "trie.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define ALPHABET_SIZE 256
#define BUFFER_SIZE 1024

typedef struct Node {
	struct Node *next[ALPHABET_SIZE];
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
		unsigned char c = (unsigned char)*p;
		if (node->next[c] == NULL) {
			Node *next = calloc(1, sizeof(Node));
			node->next[c] = next;
		}
		node = node->next[c];
	}
	node->complete = true;
}

static void search_completions(Node *node, TrieResult *res, char *suffix) {
	if (node->complete) {
		res->results[res->count] = strdup(suffix);
		res->count++;
	}

	for (int i = 0; i < ALPHABET_SIZE; i++) {
		if (node->next[i]) {
			size_t len = strlen(suffix);
			suffix[len] = (char)i;
			suffix[len + 1] = '\0';
			search_completions(node->next[i], res, suffix);
			suffix[len] = '\0';
		}
	}
}

static int compare(const void *a, const void *b) {
		return strcmp(*(const char**)a, *(const char**)b);
}


TrieResult *trie_autocomplete(Trie *trie, const char *str) {
	TrieResult *trie_result = malloc(sizeof(TrieResult));
	trie_result->count = 0;
	trie_result->results = malloc(BUFFER_SIZE * sizeof(char *));

	Node *node = trie->root;
	for (const char *p = str; *p; p++) {
		Node *next = node->next[(unsigned char)*p];
		if (next == NULL) {
			return trie_result;
		}
		node = next;
	}

	char *suffix = calloc(BUFFER_SIZE, sizeof(char));
	search_completions(node, trie_result, suffix);
	free(suffix);
	qsort(trie_result->results, trie_result->count, sizeof(trie_result->results[0]), compare);
	return trie_result;
}

static void destroy_node(Node *node) {
	for (int i = 0; i < ALPHABET_SIZE; i++) {
		if (node->next[i] != NULL) {
			destroy_node(node->next[i]);
		}
	}
	free(node);
}

void trie_destroy(Trie *trie) {
	if (trie == NULL) return;
	if (trie->root) destroy_node(trie->root);
	free(trie);
}

void trie_result_destroy(TrieResult* trie_result) {
	for (int i = 0; i < trie_result->count; i++) {
		free(trie_result->results[i]);
	}
	free(trie_result->results);
	free(trie_result);
}

