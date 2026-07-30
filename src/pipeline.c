#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include "history.h"
#include "pipeline.h"
#include "compspec.h"
#include "jobs.h"
#include "variables.h"

#define BUFFER_SIZE 1024

extern int stdout_fd, stderr_fd;

struct Pipeline {
	Command **cmds;
	int n;
	bool is_background;
};

// ------------------- EXECUTING LAYER ----------------------

const char *builtins[] = {"echo", "type", "exit", "pwd", "cd", "complete", "jobs", "history", "declare", NULL};

static int is_builtin(const char *cmd) {
	for (int i = 0; builtins[i] != NULL; i++) {
		if (strcmp(cmd, builtins[i]) == 0) return 1;
	}
	return 0;
}

static char *find_executable(const char *cmd) {
	char *path_env = getenv("PATH");
	if (path_env == NULL) return NULL;

	char *path = strdup(path_env);
	char *result = NULL;

	for (char *dir = strtok(path, ":"); dir != NULL; dir = strtok(NULL, ":")) {
		char full_path[BUFFER_SIZE];
		snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
		if (access(full_path, X_OK) == 0) {
			result = strdup(full_path);
			break;
		}
	}

	free(path);
	return result;
}

static void echo_command(char *args[]) {
	for (int i = 1; args[i] != NULL; i++) {
		printf("%s%s", args[i], args[i + 1] != NULL ? " " : "");
	}
	printf("\n");
}

static void cd_command(const char* path) {
	if (path == NULL || strcmp(path, "~") == 0) {
		path = getenv("HOME");
	}
	if (path != NULL && chdir(path) != 0) {
		printf("cd: %s: No such file or directory\n", path);
	}
}

static void type_command(const char *cmd) {
	if (is_builtin(cmd)) {
		printf("%s is a shell builtin\n", cmd);
		return;
	}
	char *path = find_executable(cmd);
	if (path != NULL) {
		printf("%s is %s\n", cmd, path);
		free(path);
	} else {
		printf("%s: not found\n", cmd);
	}
}

static void pwd_command(void) {
	char cwd[BUFFER_SIZE];

	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
	} else {
		perror("pwd");
	}
}

static void complete_command(Compspec *compspecs, char *args[]) {
	if (strcmp(args[1], "-p") == 0) {
		const char *cmd = args[2];
		if (cmd == NULL) {
			printf("complete: invalid format\n");
			return ;
		}
		const char *path = compspec_get_path(compspecs, cmd);
		if (path == NULL) {
			printf("complete: %s: no completion specification\n", cmd);
		} else {
			printf("complete -C '%s' %s\n", path, cmd);
		}
	} else if (strcmp(args[1], "-C") == 0) {
		const char *path = args[2];
		const char *cmd = args[3];
		if (path == NULL || cmd == NULL) {
			printf("complete: invalid format\n");
			return ;
		}
		compspec_add_path(compspecs, cmd, path);
	} else if (strcmp(args[1], "-r") == 0) {
		const char *cmd = args[2];
		if (cmd == NULL) {
			printf("complete: invalid format\n");
			return ;
		}
		compspec_remove_path(compspecs, cmd);
	}else {
		printf("complete: invalid format\n");
	}
}

int *my_atoi(char *str) {
	if (str == NULL) return NULL;
	char *end;
	int *value = malloc(sizeof(int));
	*value = strtol(str, &end, 10);

	if (*end != '\0') {
		return NULL;
	}

	return value;
}

static void history_command(History *history, char **argv) {
	if (argv[1] != NULL) {
		if (strcmp(argv[1], "-r") == 0) return history_read(history, argv[2]);
		if (strcmp(argv[1], "-w") == 0) return history_write(history, argv[2], false);
		if (strcmp(argv[1], "-a") == 0) return history_write(history, argv[2], true);
	}
	history_print(history, my_atoi(argv[1]));
}

static void jobs_command(Jobs* jobs) {
	jobs_print(jobs);
}

void parse(char *arg, char **identifier, char **value) {
	char *copy = strdup(arg);
	char *token = strtok(copy, "=");
	*identifier = token;
	token = strtok(NULL, "=");
	*value = token;
}

bool is_identifier_valid(char *identifier) {
	if (identifier == NULL) return false;
	if ('0' <= identifier[0] && identifier[0] <= '9') return false;
	for (char *p = identifier; *p != '\0'; p++) {
		if (*p == '_') continue;
		if ('a' <= *p && *p <= 'z') continue;
		if ('A' <= *p && *p <= 'Z') continue;
		if ('0' <= *p && *p <= '9') continue;
		return false;
	}
	return true;
}

static void declare_command(Variables *variables, char **argv) {
	if (argv[1] == NULL) return ;

	if (strcmp(argv[1], "-p") == 0) {
		if (argv[2] == NULL) return;
		char *value = variables_get(variables, argv[2]);
		if (value == NULL) {
			printf("declare: %s: not found\n", argv[2]);
		} else {
				printf("declare -- %s=\"%s\"\n", argv[2], value);
		}
	}

	char *identifier, *value;
	parse(argv[1], &identifier, &value);
	if (identifier == NULL || value == NULL) {
		return;
	}
	if (!is_identifier_valid(identifier)) {
		printf("declare: `%s': not a valid identifier\n", argv[1]);
		return ;
	}

	variables_declare(variables, identifier, value);


	return ;
}

// ------------------- COMMAND ----------------------

static void apply_trailing_redirect(Command *command) {
	int *n = &command->argc;
	char **argv = command->argv;
	if (*n <= 2) return;

	int fd_target = 0;
	bool append = false;

	if (strcmp(argv[*n - 2], "1>") == 0 || strcmp(argv[*n - 2], ">") == 0) {
		fd_target = 1;
	} else if (strcmp(argv[*n - 2], "1>>") == 0 || strcmp(argv[*n - 2], ">>") == 0) {
		fd_target = 1;
		append = true;
	} else if (strcmp(argv[*n - 2], "2>") == 0) {
		fd_target = 2;
	} else if (strcmp(argv[*n - 2], "2>>") == 0) {
		fd_target = 2;
		append = true;
	}

	if (!fd_target) return;

	int open_flags = append ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC);
	int fd = open(argv[*n - 1], open_flags, 0644);
	command->fd[fd_target] = fd;
	argv[*n - 2] = NULL;
}

static Command *command_create(void) {
	Command *command = malloc(sizeof(Command)); 
	command->argc = 0;
	command->argv = malloc(BUFFER_SIZE * sizeof(char*));
	command->fd[0] = -1, command->fd[1] = -1, command->fd[2] = -1;
	return command;
}

static void command_execute(Command *command, History *history, Compspec *compspecs, Jobs *jobs, Variables *variables) {
	for (int i = 0; i < 3; i++) {
		if (command->fd[i] != -1) {
			dup2(command->fd[i], i);
			close(command->fd[i]);
		}
	}
	char **argv = command->argv;
	if (strcmp(argv[0], "exit") == 0) {
		history_write(history, getenv("HISTFILE"), false);
		exit(0);
	}
	else if (strcmp(argv[0], "echo") == 0) echo_command(argv);
	else if (strcmp(argv[0], "type") == 0) type_command(argv[1] ? argv[1] : "");
	else if (strcmp(argv[0], "pwd") == 0) pwd_command();
	else if (strcmp(argv[0], "cd") == 0) cd_command(argv[1]);
	else if (strcmp(argv[0], "complete") == 0) complete_command(compspecs, argv);
	else if (strcmp(argv[0], "history") == 0) history_command(history, argv);
	else if (strcmp(argv[0], "jobs") == 0) jobs_command(jobs);
	else if (strcmp(argv[0], "declare") == 0) declare_command(variables, argv);
	else {
		char *path = find_executable(argv[0]);
		if (path == NULL) {
			printf("%s: command not found\n", argv[0]);
			return;
		}
		execv(path, argv);
		free(path);
	}
	dup2(stdout_fd, 1);
	dup2(stderr_fd, 2);
}

static void command_execute_via_child(Command *command, History *history, Compspec *compspecs, Jobs *jobs, Variables *variables, bool is_background) {
	pid_t pid = fork();
	if (pid == 0) {
		command_execute(command, history, compspecs, jobs, variables);
		exit(0);
	} else if (pid > 0) {
		if (is_background) {
			int n = jobs_add(jobs, pid, command->argv);
			printf("[%d] %d\n", n, pid);
		} else {
			waitpid(pid, NULL, 0);
		}
	} else {
		perror("fork");
	}
}

static void command_destroy(Command *command) {
	for (int i = 0; i < command->argc; i++) {
		free(command->argv[i]);
	}
	free(command->argv);
	free(command);
}

// ----------------- PIPELINE ---------------------

Pipeline *pipeline_create(char **argv, int argc) {
	Pipeline *pipeline = malloc(sizeof(Pipeline));
	pipeline->is_background = (argc >= 1 && strcmp(argv[argc-1], "&") == 0);
	if (pipeline->is_background) {
		argv[--argc] = NULL;
	}

	pipeline->n = 0;
	pipeline->cmds = malloc(BUFFER_SIZE * sizeof(Command*));

	int i = 0;
	while (i < argc) {
		Command *command = command_create();
		while (i < argc && strcmp(argv[i], "|") != 0) {
			command->argv[command->argc++] = strdup(argv[i]);
			i++;
		}

		if (command->argc == 0) {
			perror("empty command");
			command_destroy(command);
			pipeline_destroy(pipeline);
			return NULL;
		}
		
		apply_trailing_redirect(command);
		
		command->argv[command->argc] = NULL;
		pipeline->cmds[pipeline->n++] = command;
		i++;
	}
	return pipeline;
}


void pipeline_execute(Pipeline *pipeline, History *history, Compspec *compspecs, Jobs* jobs, Variables *variables) {
	int n = pipeline->n;
	if (n == 1) {
		Command *cmd = pipeline->cmds[0];
		if (is_builtin(cmd->argv[0]) && !pipeline->is_background) {
			command_execute(cmd, history, compspecs, jobs, variables);
		} else {
			command_execute_via_child(cmd, history, compspecs, jobs, variables, pipeline->is_background);
		}
		return ;
	}

	pid_t *pids = malloc(n * sizeof(pid_t));
	int **pipefds = malloc((n-1) * sizeof(int*));
	for (int i = 0; i + 1 < n; i++) {
		pipefds[i] = malloc(2 * sizeof(int));
		pipe(pipefds[i]);
	}
	for (int i = 0; i < n; i++) {
		Command *cmd = pipeline->cmds[i];

		pid_t pid = fork();
		if (pid == 0) {
			if (i > 0 && cmd->fd[0] == -1) {
				cmd->fd[0] = pipefds[i-1][0];
			}
			if (i + 1 < n && cmd->fd[1] == -1) {
				cmd->fd[1] = pipefds[i][1];
			}
			command_execute(cmd, history, compspecs, jobs, variables);
			if (i + 1 < n && cmd->fd[1] == -1) {
				close(pipefds[i][1]);
			}
			if (i > 0 && cmd->fd[0] == -1) {
				close(pipefds[i-1][0]);
			}
			exit(0);
		}
		if (i + 1 < n) close(pipefds[i][1]);
		pids[i] = pid;
	}
	for (int i = 0; i + 1 < n; i++) {
		close(pipefds[i][0]);
	}

	for (int i = 0; i < n; i++) {
		waitpid(pids[i], NULL, 0);
	}
}

bool pipeline_empty(Pipeline *pipeline) {
	return pipeline->n == 0 ? true : false;
}

void pipeline_destroy(Pipeline *pipeline) {
	for (int i = 0; i < pipeline->n; i++) {
		command_destroy(pipeline->cmds[i]);
	}
	free(pipeline->cmds);
	free(pipeline);
}
