#include "minishell.h"

static inline void	bunsupported(void) {
	errx(EXIT_SUCCESS, "%s: builtins do not work with pipes or re-directions\n"
		"\tType 'help' for more information.\n",
		program_invocation_short_name);
}

static inline void	becho(const struct command *restrict cmd) {
	bool	is_trail_newline = true;
	int	i = 1;
	if (2 == cmd->argc && !strcmp("--help", cmd->argv[1])) {
		goto becho_help_message;
	} else if (2 <= cmd->argc && !strcmp("-n", cmd->argv[1])) {
		is_trail_newline = false;
		i = 2;
	}

	for (; cmd->argc > i; ++i) {
		fwrite(cmd->argv[i], sizeof(char), strlen(cmd->argv[i]), g_defout);
		if (i + 1 != cmd->argc) {
			fwrite(" ", sizeof(char), 1, g_defout);
		}
	}
	if (is_trail_newline)
		fwrite("\n", sizeof(char), 1, g_defout);
	return ;

becho_help_message:
	printf("Usage: %s [SHORT-OPTION]... [STRING]...\n"
		"  or : %s LONG-OPTION\n"
		"Echo the STRING(s) to standard output.\n"
		"\t-n\tdo not output the trailing newline\n"
		"\t--help\tdisplay this help and exit\n",
		cmd->argv[0], cmd->argv[0]);
}

static inline char	*bcd_replacer(const struct command *restrict cmd) {
	char *restrict	rep_cwd = NULL;
	char *restrict	out = get_current_dir_name();
	char *restrict	copy = strdupa(out);

	if (!(rep_cwd = strstr(copy, cmd->argv[1]))) {
		fprintf(stderr, "cd: string not in pwd: %s\n", cmd->argv[1]);
		free(out);
		return NULL;
	}
	size_t	search_len = strlen(cmd->argv[1]);
	size_t	replace_len = strlen(cmd->argv[2]);
	off_t	start_offset = rep_cwd - copy;
	if (replace_len > search_len)
		assert(out = realloc(out, strlen(out) + replace_len - search_len));
	memcpy(out + start_offset, cmd->argv[2], replace_len);
	strcpy(out + start_offset + replace_len, rep_cwd + search_len);
	return out;
}

static inline void	bcd(const struct command *restrict cmd) {
	char *restrict	chdir_path = NULL;

	switch(cmd->argc) {
		case 1: {
			if (!(chdir_path = getenv("HOME")))
				chdir_path = getpwuid(getuid())->pw_dir;
			break ;
		}
		case 2: chdir_path = cmd->argv[1]; break ;
		case 3: {
			if (!(chdir_path = bcd_replacer(cmd)))
				return ;
			break ;
		}
		default: fprintf(stderr, "cd: too many arguments\n"); return ;
	}
	int	chdir_ret = chdir(chdir_path);
	if (-1 == chdir_ret) {
		fprintf(stderr, "cd: %m: %s\n", chdir_path);
	} else if (3 == cmd->argc) {
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
	int	exit_status = EXIT_SUCCESS;
	if (2 < cmd->argc) {
		fprintf(stderr, "%s: too many arguments\n", cmd->argv[0]);
		return ;
	} else if (2 == cmd->argc) {
		exit_status = atoi(cmd->argv[1]);
	}
	exit(exit_status);
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
		"\tunsetenv: delete the variable from the environment\n"
		"\tenv: run a program in modified environment\n"
		"\texit: exit from %s\n"
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
