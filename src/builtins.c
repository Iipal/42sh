#include "minishell.h"

static inline void	becho(const struct command *restrict cmd);
static inline void	bcd(const struct command *restrict cmd);
static inline void	benv(const struct command *restrict cmd);
static inline void	bsetenv(const struct command *restrict cmd);
static inline void	bunsetenv(const struct command *restrict cmd);
static inline void	bexit(const struct command *restrict cmd);
static inline void	bhelp(const struct command *restrict cmd);

struct s_builtin_data_set {
	const char *restrict	bname;
	void	(*bfnptr)(const struct command *restrict cmd);
	size_t	max_argc;
} __bds[] = {
	{ "echo"    , becho    , ~((size_t)0) },
	{ "cd"      , bcd      , 3 },
	{ "env"     , benv     , ~((size_t)0) },
	{ "setenv"  , bsetenv  , 3 },
	{ "unsetenv", bunsetenv, 2 },
	{ "exit"    , bexit    , 2 },
	{ "help"    , bhelp    , 1 },
	{ NULL      , NULL     , 0 }
};

static inline bool	bunsupported(void) {
	fprintf(stderr, "%s: builtins do not work with pipes or re-directions\n"
		"\tType 'help' for more information.\n",
		program_invocation_short_name);
	return false;
}

static inline void	becho(const struct command *restrict cmd) {
	bool	is_trail_newline = true;
	size_t	i = 1;

	if (2 == cmd->argc) {
		if (!strcmp("--help", cmd->argv[1]))
			goto becho_help_message;
	}
	if (2 <= cmd->argc) {
		if (!strcmp("-n", cmd->argv[1])) {
			is_trail_newline = false;
			i = 2;
		}
	}

	for (; cmd->argc > i; ++i) {
		fwrite(cmd->argv[i], sizeof(*(cmd->argv[i])),
			strlen(cmd->argv[i]), stdout);
		if (i + 1 != cmd->argc)
			fwrite(" ", sizeof(char), 1, stdout);
	}
	if (is_trail_newline)
		fwrite("\n", sizeof(char), 1, stdout);
	return ;
becho_help_message:
	printf("Usage: echo [SHORT-OPTION]... [STRING]...\n"
		"  or : echo LONG-OPTION\n"
		"Echo the STRING(s) to standard output.\n"
		"\t-n\tdo not output the trailing newline\n"
		"\t--help\tdisplay this help and exit\n");
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
	char *restrict	chdir_path = cmd->argv[1];
	if (1 == cmd->argc) {
		if (!(chdir_path = getenv("HOME")))
			chdir_path = getpwuid(getuid())->pw_dir;
	} else if (3 == cmd->argc) {
		if (!(chdir_path = bcd_replacer(cmd)))
			return ;
	}

	if (-1 == chdir(chdir_path))
		fprintf(stderr, "cd: %m: %s\n", chdir_path);
	if (3 == cmd->argc)
		free(chdir_path);
}

static inline void	benv(const struct command *restrict cmd) {
	if (1 == cmd->argc) {
		for (size_t i = 0; environ[i]; ++i)
			puts(environ[i]);
	} else {
		struct command	c;
		c.argc = cmd->argc - 1;
		assert(c.argv = calloc(c.argc, sizeof(*c.argv)));
		for (size_t i = 0; c.argc >= i; ++i)
			c.argv[i] = cmd->argv[i + 1];
		cmd_solorun(&c);
		free(c.argv);
	}
}

static inline void	bsetenv(const struct command *restrict cmd) {
	if (1 == cmd->argc) {
		benv(cmd);
	} else {
		if (-1 == setenv(cmd->argv[1], (3 == cmd->argc) ? cmd->argv[2] : "", 1))
			perror(cmd->argv[0]);
	}
}

static inline void	bunsetenv(const struct command *restrict cmd) {
	if (1 == cmd->argc) {
		benv(cmd);
	} else if (2 == cmd->argc) {
		if (-1 == unsetenv(cmd->argv[1]))
			perror(cmd->argv[0]);
	}
}

static inline __attribute__((noreturn)) void
bexit(const struct command *restrict cmd) {
	int	exit_status = EXIT_SUCCESS;
	if (2 == cmd->argc) {
		exit_status = atoi(cmd->argv[1]);
	}
	_Exit(exit_status);
}

static inline void	bhelp(const struct command *restrict cmd) {
	if (1 != cmd->argc) {
		fprintf(stderr, "%s: too many arguments\n", cmd->argv[0]);
		return ;
	}
	printf("Builtins help:\n"
		"\techo    \tdisplay a line of text\n"
		"\tcd      \tchange the current directory\n"
		"\tsetenv  \tadd the variable to the environment\n"
		"\tunsetenv\tdelete the variable from the environment\n"
		"\tenv     \trun a program in modified environment\n"
		"\texit    \texit the shell\n"
		"\thelp    \tprint this info message\n"
		"\n(!!!) No pipes or re-directions do not work for builtin commands\n");
}

bool	cmd_builtinrun(const struct command *restrict cmd, cq_type_t cq_type) {
	size_t	match;
	for (match = 0; __bds[match].bname; ++match)
		if (!strcmp(__bds[match].bname, cmd->argv[0]))
			break ;

	if (!__bds[match].bname)
		return false;
	if (CQ_PIPE == cq_type)
		return bunsupported();
	if (__bds[match].max_argc < cmd->argc) {
		fprintf(stderr, "%s: too many arguments\n", __bds[match].bname);
	} else {
		__bds[match].bfnptr(cmd);
	}
	return true;
}

bool	cmd_fast_builtinrun(const struct command cmd) {
	return cmd_builtinrun(&cmd, CQ_DEFAULT);
}

bool	cmd_fast_tbuiltinrun(const struct command cmd, cq_type_t cq_type) {
	return cmd_builtinrun(&cmd, cq_type);
}
