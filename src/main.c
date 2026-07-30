#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include "history.h"
#include "pipeline.h"
#include "trie.h"
#include "compspec.h"
#include "jobs.h"

#define BUFFER_SIZE 1024

int stdout_fd, stderr_fd;

static void free_str_array(char **args, int argc) {
	for (int i = 0; i < argc; i++) free(args[i]);
	free(args);
}

static void find_all_executables(int *count, char **executables) {
	char *path_env = getenv("PATH");
	if (path_env == NULL) return ;

	char *path = strdup(path_env);

	for (char *dir_path = strtok(path, ":"); dir_path != NULL; dir_path = strtok(NULL, ":")) {
		if (dir_path[0] == '\0') continue;

		DIR *dir = opendir(dir_path);
		if (dir == NULL) {
				continue;
		}

		struct dirent *entry;

		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type != DT_REG && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN) continue;
			executables[*count] = malloc(BUFFER_SIZE);
			strcpy(executables[*count], entry->d_name);
			*count = *count + 1;
		}
		closedir(dir);
	}

	free(path);
}

static int compare(const void *a, const void *b) {
		return strcmp(*(const char**)a, *(const char**)b);
}

static void list_path_completions(const char *dir_path, const char *prefix, int *count,
																	char **completions) {
	*count = 0;

	DIR *dir = opendir(dir_path);
	if (dir == NULL) {
		return;
	}

	size_t prefix_len = strlen(prefix);
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		const char *name = entry->d_name;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

		if (strncmp(name, prefix, prefix_len) != 0) continue;
		bool is_dir = (entry->d_type == DT_DIR);
		const char* rest = name + prefix_len;
		size_t n = strlen(rest);

		completions[*count] = malloc(n + 2);
		memcpy(completions[*count], rest, n);
		if (is_dir) {
			completions[*count][n] = '/';
			completions[*count][n+1] = '\0';
		} else {
			completions[*count][n] = '\0';
		}
		(*count)++;
	}

	qsort(completions, *count, sizeof(*completions), compare);

	closedir(dir);
}


static void configure_terminal(void) {
	struct termios old, raw;

	tcgetattr(STDIN_FILENO, &old);
	raw = old;

	raw.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}


void parse_line(const char *line, int line_len, int *n, char **args) {
	int tok_len = 0;
	const char *quote = NULL;

	args[0] = malloc(BUFFER_SIZE);
	for (int i = 0; i < line_len; i++) {
		char ch = line[i];
		if (ch == ' ' && !quote) {
			if (tok_len > 0) {
				args[*n][tok_len] = '\0';
				tok_len = 0;
				(*n)++;
				args[*n] = malloc(BUFFER_SIZE);
			}
			continue;
		} else if (ch == '\'' || ch == '\"') {
			if (!quote) quote = &line[i];
			else if (*quote == ch) quote = NULL;
			else args[*n][tok_len++] = ch;
		} else if (ch == '\\' && (!quote || *quote != '\'')) {
			i++;
			if (i >= line_len) break;
			args[*n][tok_len++] = line[i];
		} else {
			args[*n][tok_len++] = ch;
		}
	}
	if (tok_len > 0) {
		args[*n][tok_len] = '\0';
		(*n)++;
	}
	args[*n] = NULL;
}

void execute(History *history, Compspec *compspecs, Jobs* jobs, char line[], int line_len) {
	int n = 0;
	char **args = malloc(BUFFER_SIZE * sizeof(char*));
	history_add(history, line);
	parse_line(line, line_len, &n, args);

	Pipeline *pipeline = pipeline_create(args, n);
	free_str_array(args, n);
	if (pipeline_empty(pipeline)) {
		jobs_reap(jobs);
		printf("$ ");
		return;
	}

	pipeline_execute(pipeline, history, compspecs, jobs);
	pipeline_destroy(pipeline);

	jobs_reap(jobs);
	printf("$ ");
}

static void handle_backspace(char *line, int *len) {
	if (*len > 0) {
		(*len)--;
		line[*len] = '\0';
		write(STDOUT_FILENO, "\b \b", 3);
	}
}

static void append_to_line(char *line, int *len, const char *s) {
	for (; *s; s++) {
		line[(*len)++] = *s;
		putchar(*s);
	}
	line[*len] = '\0';
}

static void remove_line(char *line, int *len) {
	while(*len > 0) {
		(*len)--;
		line[*len] = '\0';
		write(STDOUT_FILENO, "\b \b", 3);
	}
}

static void handle_up_down_arrow(History *history, bool up, char *line, int *len) {
	remove_line(line, len);

	char *new_line;
	if (up) new_line = history_move_cursor_up(history);
	else new_line = history_move_cursor_down(history);

	if (line != NULL) append_to_line(line, len, new_line);
}

static void handle_enter(History *history, Compspec *compspecs, Jobs* jobs, char *line, int *len) {
	printf("\n");
	execute(history, compspecs, jobs, line, *len);
	*len = 0;
	line[0] = '\0';
}

static void handle_char(char *line, int *len, int c) {
	line[(*len)++] = c;
	line[*len] = '\0';
	putchar(c);
}

static char *split_path_token(const char *token, const char **filename) {
	const char *last_slash = strrchr(token, '/');
	if (last_slash == NULL) {
		*filename = token;
		return strdup(".");
	}
	size_t dir_len = (size_t)(last_slash - token);
	*filename = last_slash + 1;
	if (dir_len == 0) return strdup("/");
	return strndup(token, dir_len);
}


static size_t common_prefix_len(char **completions, int count) {
	size_t n = strlen(completions[0]);
	for (int i = 1; i < count; i++) {
		size_t len = strlen(completions[i]);
		if (len < n) n = len;
		for (size_t j = 0; j < n; j++) {
			if (completions[i][j] != completions[0][j]) {
				n = j;
				break;
			}
		}
		if (n == 0) break;
	}
	return n;
}

static void apply_completions(char *line, int *len, const char *word, int *tab_count,
															int count, char **completions) {
	if (count == 0) {
		printf("\a");
		*tab_count = 0;
		return;
	}

	if (count == 1) {
		append_to_line(line, len, completions[0]);
		bool is_dir = *len > 0 && line[*len - 1] == '/';
		if (!is_dir) {
			line[(*len)++] = ' ';
			putchar(' ');
		}
		*tab_count = 0;
		return;
	}

	size_t shared = common_prefix_len(completions, count);
	if (shared == 0) {
		if (*tab_count >= 2) {
			const char *base = strrchr(word, '/');
			base = base ? base + 1 : word;

			printf("\n");
			for (int i = 0; i < count; i++) {
				if (i > 0) printf("  ");
				printf("%s%s", base, completions[i]);
			}
			printf("\n$ %.*s", *len, line);
			*tab_count = 0;
		} else {
			printf("\a");
		}
		return;
	}

	for (size_t j = 0; j < shared; j++) {
		line[(*len)++] = completions[0][j];
		putchar(completions[0][j]);
	}
	line[*len] = '\0';
	*tab_count = 0;
}

static void handle_tab(Compspec *compspec, char *line, int *len, int *tab_count, Trie *trie) {
	char **args = malloc(BUFFER_SIZE * sizeof(char *));
	int argc = 0;
	parse_line(line, *len, &argc, args);

	bool trailing_space = *len > 0 && line[*len - 1] == ' ';
	if (argc == 1 && !trailing_space) {
		TrieResult *result = trie_autocomplete(trie, args[0]);
		apply_completions(line, len, args[0], tab_count, result->count, result->results);
		trie_result_destroy(result);
		free_str_array(args, argc);
		return;
	}

	if (argc >= 1 && compspec_get_path(compspec, args[0]) != NULL) {
		const char *prefix = trailing_space ? "" : (argc > 1 ? args[argc - 1] : "");
		const char *word_before = trailing_space
			? args[argc - 1]
			: (argc > 1 ? args[argc - 2] : "");
		setenv("COMP_LINE", line, 1);
		char *line_length = malloc(BUFFER_SIZE);
		snprintf(line_length, sizeof(line_length), "%d", *len);
		setenv("COMP_POINT", line_length, 1);
		char **completions = malloc(BUFFER_SIZE * sizeof(char*));
		int count = 0;
		compspec_run(compspec, completions, &count, args[0], prefix, word_before);
		apply_completions(line, len, prefix, tab_count, count, completions);
		free_str_array(completions, count);
		free_str_array(args, argc);
		return;
	}

	const char *token = trailing_space ? "" : (argc > 0 ? args[argc - 1] : "");
	const char *filename;
	char *dir_path = split_path_token(token, &filename);
	char **completions = malloc(BUFFER_SIZE * sizeof(char *));
	int count = 0;
	list_path_completions(dir_path, filename, &count, completions);
	apply_completions(line, len, token, tab_count, count, completions);

	free_str_array(completions, count);
	free(dir_path);
	free_str_array(args, argc);
}

static Trie *build_executables_trie(void) {
	Trie *trie = trie_create();
	for (int i = 0; builtins[i] != NULL; i++) trie_add(trie, builtins[i]);

	char **executables = malloc(256 * BUFFER_SIZE * sizeof(char *));
	int *executables_count = malloc(sizeof(int));
	*executables_count = 0;
	find_all_executables(executables_count, executables);
	for (int i = 0; i < *executables_count; i++) {
		trie_add(trie, executables[i]);
	}
	return trie;
}

static void run_repl(History *history, Trie *trie, Compspec *compspecs, Jobs* jobs) {
	char line[BUFFER_SIZE];
	int len = 0;
	int tab_count = 0;
	int c;

	line[0] = '\0';

	while ((c = getchar()) != EOF) {
		if (c == 27) {
			int c2 = getchar();
			if (c2 == '[') {
				int c3 = getchar();
				if (c3 == 'A') handle_up_down_arrow(history, true, line, &len);
				else if (c3 == 'B') handle_up_down_arrow(history, false, line, &len);
			}
		} else if (c == 127 || c == 8) {
			handle_backspace(line, &len);
			tab_count = 0;
		} else if (c == '\n') {
			handle_enter(history, compspecs, jobs, line, &len);
			tab_count = 0;
		} else if (c == '\t') {
			tab_count++;
			handle_tab(compspecs, line, &len, &tab_count, trie);
		} else {
			handle_char(line, &len, c);
			tab_count = 0;
		}
	}
}

int main(void) {
	configure_terminal();
	setbuf(stdout, NULL);
	stdout_fd = dup(1), stderr_fd = dup(2);
	History *history = history_create();
	Trie* trie = build_executables_trie();
	Compspec* compspecs = compspec_create();
	Jobs *jobs = jobs_create();

	printf("$ ");
	run_repl(history, trie, compspecs, jobs); 

	history_destroy(history);
	trie_destroy(trie);
	compspec_destroy(compspecs);
	jobs_destroy(jobs);
	close(stdout_fd);
	return 0;
}
