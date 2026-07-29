all:
	gcc src/main.c src/compspec.c src/map.c src/trie.c src/jobs.c src/pipeline.c -o shell

run: all
	./shell

clean:
	rm -f shell

.PHONY: all run clean
