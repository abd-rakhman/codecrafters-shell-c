#ifndef TRIE_H
#define TRIE_H

typedef struct Trie Trie;
typedef struct { 
	int count;
	char **results;
} TrieResult;

Trie *trie_create(void);

void trie_add(Trie *trie, const char* str);
TrieResult *trie_autocomplete(Trie *trie, const char *str);

void trie_destroy(Trie *trie);
void trie_result_destroy(TrieResult* trie_result);

#endif

