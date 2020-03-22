#include "minishell.h"

static inline __attribute__((nonnull(2)))
	void cq_free(const size_t cq_size, command_t **cq) {
	for (size_t i = 0; cq_size >= i && cq[i]; ++i) {
		if (cq[i]->argv) {
			for (int j = 0; cq[i]->argc >= j && cq[i]->argv[j]; j++) {
				free(cq[i]->argv[j]);
			}
			free(cq[i]->argv);
		}
		free(cq[i]);
	}
	free(cq);
}

static inline void	pipe_redir(int from, int to, int trash) {
	dup2(from, to);
	close(from);
	if (trash != -1) {
		close(trash);
	}
}

static void	cmd_pipe_queuing(const ssize_t isender,
							const ssize_t ireceiver,
							command_t *restrict *restrict cq) {
	if (-1 >= isender) {
		return ;
	}

	int	fds[2] = { 0 };

	assert(-1 != pipe(fds));
	assert(-1 != (child = fork()));

	if (!child) {
		pipe_redir(fds[1], STDOUT_FILENO, fds[0]);
		cmd_pipe_queuing(isender - 1, isender, cq);
		DBG_INFO("child  | %d(%d) '%s' created\n",
			getpid(), getppid(), cq[isender]->argv[0]);
		execvp(cq[isender]->argv[0], cq[isender]->argv);
		errx(EXIT_FAILURE, EXEC_ERR_FMTMSG, cq[isender]->argv[0]);
	} else if (0 < child) {
		DBG_INFO("child  | %d(%d) '%s' wait for input from '%s'\n",
			getpid(), getppid(),
			cq[ireceiver]->argv[0], cq[isender]->argv[0]);
		pipe_redir(fds[0], STDIN_FILENO, fds[1]);
		execvp(cq[ireceiver]->argv[0], cq[ireceiver]->argv);
		errx(EXIT_FAILURE, EXEC_ERR_FMTMSG, cq[ireceiver]->argv[0]);
	}
}

static inline char	*cmd_readline(void) {
	char	*out = NULL;
	size_t	n = 0;
	ssize_t	nb = getline(&out, &n, stdin);

	if (!nb || !out) {
		return NULL;
	}
	*((short*)(out + nb - 1)) = 0;
	return out;
}

static inline size_t	cq_precalc_size(char *restrict line) {
	size_t	n = 1;
	char	*l = strchr(line, '|');

	if (l) {
		is_pipe = 1;
		do {
			l = strchr(l + 1, '|');
			++n;
		} while (l);
	}
	return n;
}

static inline void	add_redir_tofile(const char *path) {
	int	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (-1 == fd) {
		err(EXIT_FAILURE, "open(%s)", path);
	}
	dup2(fd, STDOUT_FILENO);
	close(fd);
}

static void	wait_child(int s) {
	DBG_INFO("wait   | %d\n", child);
	if (1 > child) {
		wait(&s);
	} else {
		waitpid(child, &s, WUNTRACED | WNOHANG);
	}
}

static inline void	init_sigchld_handler(void) {
	struct sigaction	sa;
	bzero(&sa, sizeof(sa));
	sa.sa_flags = SA_RESTART | SA_NODEFER;
	sa.sa_handler = wait_child;

	if (-1 == sigaction(SIGCHLD, &sa, NULL)) {
		err(EXIT_FAILURE, "sigaction");
	}
}

static inline void	parse_opt(int ac, char *const *av) {
	static const struct option	l_opts[] = {
		{ "debug", no_argument, &dbg_level    , 1 },
		{ "file" , no_argument, &stdout_tofile, 1 },
		{ 0      , 0          , 0             , 0 }
	};

	int	opt;
	while (-1 != (opt = getopt_long(ac, av, "df", l_opts, NULL))) {
		switch (opt) {
			case 'd': dbg_level      = 1; break ;
			case 'f': stdout_tofile  = 1; break ;
			case '?': exit(EXIT_FAILURE); break ;
			default :                     break ;
		}
	}
}

static char	*line_trim_extra_ws(const char *restrict src) {
	size_t	start = 0;
	size_t	end = strlen(src);

	while (src[start] && isspace(start)) {
		++start;
	}
	while (start < end && isspace(src[end - 1])) {
		--end;
	}
	if (!src[start] || start == end) {
		return NULL;
	}

	char	*acopy = strndupa(src + start, end - start);

	size_t	n = 0;
	for (size_t i = 0; end > i && acopy[i]; ++i) {
		if (!isspace(acopy[i]) || (0 < i && !isspace(acopy[i - 1]))) {
			acopy[n++] = acopy[i];
		}
	}
	acopy[n] = 0;

	char	*out;
	assert(out = strndup(acopy, n));
	return out;
}

static char	*line_put_sep_space(const char *restrict str, int sep) {
	char	*out;
	char	*sep_ptr = strchr(str, sep);

	assert(out = strdup(str));
	while (sep_ptr) {
		size_t	sep_len = 0;
		if (sep_ptr > str && !isspace(sep_ptr[-1])) {
			sep_len = strstr(out, sep_ptr) - out;
			assert(out = realloc(out, strlen(out) + 2));
			out[sep_len++] = ' ';
			out[sep_len++] = sep;
			strcpy(out + sep_len, sep_ptr + 1);
		}
		if (!isspace(sep_ptr[1])) {
			sep_len = strstr(out, sep_ptr) - out + 1;
			assert(out = realloc(out, strlen(out) + 1));
			out[sep_len++] = ' ';
			strcpy(out + sep_len, sep_ptr + 1);
		}
		sep_ptr = strchr(sep_ptr + 1, sep);
	}
	return out;
}

static inline char	*line_prepare(const char *restrict line) {
	char *restrict	line_no_ws = line_trim_extra_ws(line);
	char *restrict	line_seps = line_put_sep_space(line_no_ws, '|');

	DBG_INFO("line transformation:\n -> '%s'\n -> '%s'\n -> '%s'\n",
		line, line_no_ws, line_seps);

	free(line_no_ws);
	return line_seps;
}

static inline void	cmd_parseline(char *restrict line,
					command_t *restrict *restrict cq) {
	char	*cmd_line = line_prepare(line);
	char	*save = NULL;
	char *restrict	token = strtok_r(cmd_line, " |", &save);
	size_t	cq_iter = 0;
	command_t *restrict	c = NULL;

	while (token) {
		if (!cq[cq_iter]) {
			assert(cq[cq_iter] = calloc(1, sizeof(**cq)));
			c = cq[cq_iter];
		}
		assert(c->argv = realloc(c->argv, sizeof(*(c->argv)) * (c->argc + 2)));
		assert(c->argv[c->argc++] = strdup(token));
		c->argv[c->argc] = NULL;
		if ('|' == *save) {
			++cq_iter;
		}
		token = strtok_r(NULL, " |", &save);
	}
	free(cmd_line);
}

static inline void	cmd_solorun(const command_t *restrict cmd) {
	if (!(child = fork())) {
		DBG_INFO("child  | %d(%d) '%s'\n", getpid(), getppid(), cmd->argv[0]);
		execvp(cmd->argv[0], cmd->argv);
		errx(EXIT_FAILURE, EXEC_ERR_FMTMSG, cmd->argv[0]);
	} else {
		DBG_INFO("parent | %d(%d)\n", getpid(), getppid());
		pause();
	}
}

static inline void	bunsupported(void) {
	fprintf(stderr, "%s: builtins do not work with pipes or re-directions\n"
					"\tType 'help' for more information.\n",
		program_invocation_short_name);
}

static inline void	becho(const command_t *restrict cmd) {
	(void)cmd;
	printf("builtin: echo\n");
}
static inline void	bcd(const command_t *restrict cmd) {
	(void)cmd;
	printf("builtin: cd\n");
}
static inline void	bsetenv(const command_t *restrict cmd) {
	(void)cmd;
	printf("builtin: setenv\n");
}
static inline void	bunsetenv(const command_t *restrict cmd) {
	(void)cmd;
	printf("builtin: unsetenv\n");
}
static inline void	benv(const command_t *restrict cmd) {
	(void)cmd;
	printf("builtin: env\n");
}
static inline void	bexit(const command_t *restrict cmd) {
	(void)cmd;
	_Exit(EXIT_SUCCESS);
}
static inline void	bhelp(const command_t *restrict cmd) {
	(void)cmd;
	fprintf(stderr, "Builtins help information:\n"
		"\techo: display a line of text\n"
		"\tcd: change the current directory\n"
		"\tsetenv: add the variable to the environment\n"
		"\tunsetenv: delete the variable from the environment\n"
		"\tenv: run a program in modified environment\n"
		"\texit: exit from '%s'\n"
		"\thelp: print this info message\n"
		"\n(!!!) No pipes or re-directions do not work for builtin commands\n",
		program_invocation_short_name);
}

static inline bool	cmd_builtinrun(const command_t *restrict cmd) {
	static void	(*cmd_fnptr_builtins[])(const command_t *restrict) = {
		becho, bcd, bsetenv, bunsetenv, benv, bexit, bhelp, NULL
	};
	static const char	*cmd_str_builtins[] = {
		"echo", "cd", "setenv", "unsetenv", "env", "exit", "help", NULL
	};

	size_t	i;
	for (i = 0; cmd_str_builtins[i]; i++) {
		if (!strcmp(cmd->argv[0], cmd_str_builtins[i])) {
			break ;
		}
	}
	if (cmd_str_builtins[i]) {
		if (is_pipe) {
			bunsupported();
		} else {
			cmd_fnptr_builtins[i](cmd);
		}
		return true;
	}
	return false;
}

static inline void	cmd_run(const size_t cq_size,
					command_t *restrict *restrict cq) {
	for (size_t i = 0; cq_size > i; i++) {
		if (cmd_builtinrun(cq[i])) {
			return ;
		}
	}

	if (1 < cq_size) {
		if (!(child = fork())) {
			cmd_pipe_queuing(cq_size - 2, cq_size - 1, cq);
		} else if (0 < child) {
			pause();
		}
	} else {
		cmd_solorun(cq[0]);
	}
}

int	main(int argc, char *argv[]) {
	parse_opt(argc, argv);

	init_sigchld_handler();
	if (stdout_tofile) {
		add_redir_tofile("./result.out");
	}

	while (1) {
		char *restrict	line = NULL;
		is_pipe = 0;

		DBG_INFO("%d(%d) ", getpid(), getppid());
		fprintf(stderr, "$> ");
		if (!(line = cmd_readline())) {
			continue ;
		}

		command_t	**cq;
		size_t	cq_size = cq_precalc_size(line);
		assert(cq = calloc(cq_size + 1, sizeof(*cq)));
		cmd_parseline(line, cq);
		cmd_run(cq_size, cq);
		cq_free(cq_size, cq);
		free(line);
	}
}
