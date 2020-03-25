#include "minishell.h"

static inline void	bunsupported(void) {
	errx(EXIT_SUCCESS, "%s: builtins do not work with pipes or re-directions\n"
		"\tType 'help' for more information.\n",
		program_invocation_short_name);
}

static inline void	becho(const struct command *restrict cmd) {
	for (int i = 1; cmd->argc > i; ++i) {
		printf("%s", cmd->argv[i]);
		if (i + 1 != cmd->argc)
			printf(" ");
	}
	printf("\n");
}

static inline char	*bcd_replacer(const struct command *restrict cmd) {
	size_t	search_len = strlen(cmd->argv[1]);
	size_t	replace_len = strlen(cmd->argv[2]);
	char	*rep_cwd = NULL;
	char	*out = get_current_dir_name();
	char	*copy = strdupa(out);

	if (!(rep_cwd = strstr(copy, cmd->argv[1]))) {
		fprintf(stderr, "cd: string not in pwd: %s\n", cmd->argv[1]);
		free(out);
		return NULL;
	}
	off_t	start_offset = rep_cwd - copy;
	if (replace_len > search_len)
		assert(out = realloc(out, strlen(out) + replace_len - search_len));
	memcpy(out + start_offset, cmd->argv[2], replace_len);
	strcpy(out + start_offset + replace_len, rep_cwd + search_len);
	return out;
}

static inline void	bcd(const struct command *restrict cmd) {
	char	*chdir_path = NULL;

	if (1 == cmd->argc) {
		if (!(chdir_path = getenv("HOME"))) {
			chdir_path = getpwuid(getuid())->pw_dir;
		}
	} else if (2 == cmd->argc) {
		chdir_path = cmd->argv[1];
	} else if (3 == cmd->argc) {
		if (!(chdir_path = bcd_replacer(cmd)))
			return ;
	}
	int	chdir_ret = chdir(chdir_path);
	if (-1 == chdir_ret) {
		fprintf(stderr, "cd: %m: %s\n", chdir_path);
	}
	if (3 == cmd->argc && !chdir_ret) {
		printf("%s\n", chdir_path);
		free(chdir_path);
	}
}

static inline void	bsetenv(const struct command *restrict cmd) {
	(void)cmd;
	printf("builtin: setenv\n");
}
static inline void	bunsetenv(const struct command *restrict cmd) {
	(void)cmd;
	printf("builtin: unsetenv\n");
}
static inline void	benv(const struct command *restrict cmd) {
	(void)cmd;
	printf("builtin: env\n");
}
static inline void	bexit(const struct command *restrict cmd) {
	(void)cmd;
	exit(EXIT_SUCCESS);
}
static inline void	bhelp(const struct command *restrict cmd) {
	if (1 != cmd->argc) {
		fprintf(stderr, "help: too many arguments\n");
		return ;
	}
	printf("Builtins help:\n"
		"\techo: display a line of text\n"
		"\tcd: change the current directory\n"
		"\tsetenv: add the variable to the environment\n"
		"\tunsetenv: delete the variable from the environment\n");
	printf("\tenv: run a program in modified environment\n"
		"\texit: exit from '%s'\n"
		"\thelp: print this info message\n"
		"\n(!!!) No pipes or re-directions do not work for builtin commands\n",
		program_invocation_short_name);
}

bool	cmd_builtinrun(const struct command *restrict cmd) {
	static void	(*cmd_fnptr_builtins[])(const struct command *restrict) = {
		becho, bcd, bsetenv, bunsetenv, benv, bexit, bhelp, NULL
	};
	static const char	*cmd_str_builtins[] = {
		"echo", "cd", "setenv", "unsetenv", "env", "exit", "help", NULL
	};
	size_t	i;

	for (i = 0; cmd_str_builtins[i]; i++)
		if (!strcmp(cmd->argv[0], cmd_str_builtins[i]))
			break ;
	if (cmd_str_builtins[i]) {
		if (g_is_cq_piped) {
			bunsupported();
		} else {
			cmd_fnptr_builtins[i](cmd);
		}
		return true;
	}
	return false;
}
