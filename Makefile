all:
	gcc src/main.c src/compspec.c src/map.c src/trie.c -o shell

run: all
	./shell

clean:
	rm -f shell

.PHONY: all run clean
