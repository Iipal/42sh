#include "minishell.h"

pid_t	g_child = 0;
bool	g_is_cq_piped = false;
int	g_opt_dbg_level = 0;
int	g_opt_stdout_redir = 0;
FILE	*g_defout = NULL;

static inline __attribute__((nonnull(2)))
	void cq_free(const size_t cq_length, struct command **cq) {
	for (size_t i = 0; cq_length >= i && cq[i]; ++i) {
		if (cq[i]->argv) {
			for (int j = 0; cq[i]->argc >= j; j++)
				free(cq[i]->argv[j]);
			free(cq[i]->argv);
		}
		free(cq[i]);
	}
	free(cq);
}

static inline void	pipe_redir(int from, int to, int trash) {
	dup2(from, to);
	close(from);
	close(trash);
}

static void	cmd_pipe_queuing(const ssize_t isender,
				const ssize_t ireceiver,
				struct command *restrict *restrict cq) {
	if (-1 >= isender)
		return ;

	int	fds[2] = { 0, 0 };
	assert(-1 != pipe(fds));
	assert(-1 != (g_child = fork()));

	if (!g_child) {
		pipe_redir(fds[1], STDOUT_FILENO, fds[0]);
		cmd_pipe_queuing(isender - 1, isender, cq);
		DBG_INFO("child  | %d(%d) %s created\n",
			getpid(), getppid(), cq[isender]->argv[0]);
		execvp(cq[isender]->argv[0], cq[isender]->argv);
		errx(EXIT_FAILURE, "%s: command not found...", cq[isender]->argv[0]);
	} else if (0 < g_child) {
		DBG_INFO("child  | %d(%d) %s wait for input from %s\n",
			getpid(), getppid(),
			cq[ireceiver]->argv[0], cq[isender]->argv[0]);
		pipe_redir(fds[0], STDIN_FILENO, fds[1]);
		execvp(cq[ireceiver]->argv[0], cq[ireceiver]->argv);
		errx(EXIT_FAILURE, "%s: command not found...", cq[ireceiver]->argv[0]);
	}
}

static inline char	*cmd_readline(void) {
	char	*out = NULL;
	size_t	n = 0;
	ssize_t	nb = getline(&out, &n, stdin);

	if (1 > nb)
		return (char*)-1;
	if (out)
		*((short*)(out + nb - 1)) = 0;
	return out;
}

static inline size_t	cq_precalc_pipe_length(char *restrict line) {
	size_t	n = 1;
	char	*l = strchr(line, '|');

	if (l) {
		g_is_cq_piped = true;
		do {
			l = strchr(l + 1, '|');
			++n;
		} while (l);
	}
	return n;
}

static inline void	add_redir_tofile(const char *path) {
	int	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (-1 == fd)
		err(EXIT_FAILURE, "open(%s)", path);
	DBG_INFO("all output from STDOUT re-directed to %s\n", path);
	dup2(fd, STDOUT_FILENO);
	close(fd);
}

static inline void	parse_opt(int ac, char *const *av) {
	static const struct option	l_opts[] = {
		{ "debug", no_argument, &g_opt_dbg_level   , 1 },
		{ "file" , no_argument, &g_opt_stdout_redir, 1 },
		{ 0      , 0          , 0                  , 0 }
	};

	int	opt;
	while (-1 != (opt = getopt_long(ac, av, "df", l_opts, NULL))) {
		switch (opt) {
			case 'd': g_opt_dbg_level    = 1; break ;
			case 'f': g_opt_stdout_redir = 1; break ;
			case '?': exit(EXIT_FAILURE);     break ;
			default : break ;
		}
	}
	if (g_opt_stdout_redir) {
		g_defout = stderr;
		add_redir_tofile("./.msh.out");
	}
}

static char	*line_ws_trimm(const char *restrict src) {
	size_t	start = 0;
	size_t	end = strlen(src);

	while (src[start] && isspace(src[start]))
		++start;
	while (start < end && isspace(src[end - 1]))
		--end;
	if (!src[start] || start == end)
		return NULL;

	char	*out = NULL;
	char	*acopy = strndupa(src + start, end - start);

	size_t	n = 1;
	for (size_t i = 1; acopy[i]; ++i)
		if (!isspace(acopy[i]) || !isspace(acopy[i - 1]))
			acopy[n++] = acopy[i];
	acopy[n] = '\0';
	assert(out = strndup(acopy, n));
	return out;
}

static char	*line_space_sep(const char *restrict str, int sep) {
	char *restrict	out;
	char *restrict	sep_ptr;

	assert(out = strdup(str));
	sep_ptr = strchr(str, sep);
	while (sep_ptr) {
		if (sep_ptr > str && !isspace(sep_ptr[-1])) {
			ptrdiff_t	sep_len = strstr(out, sep_ptr) - out;
			assert(out = realloc(out, strlen(out) + 1));
			out[sep_len++] = ' ';
			out[sep_len++] = sep;
			strcpy(out + sep_len, sep_ptr + 1);
		}
		sep_ptr = strchr(sep_ptr + 1, sep);
	}
	return out;
}

static inline char	*line_prepare(const char *restrict line) {
	char *restrict	line_no_ws;
	char *restrict	line_seps;

	if (!(line_no_ws = line_ws_trimm(line)))
		return NULL;
	line_seps = line_space_sep(line_no_ws, '|');

	free(line_no_ws);
	return line_seps;
}

static inline bool	cmd_parseline(char *restrict line,
					struct command *restrict *restrict cq) {
	struct command *restrict	c = NULL;
	size_t	cq_iter = 0;
	char	*save = NULL;
	char	*token = NULL;
	char	*cmd_line = NULL;

	if (!(cmd_line = line_prepare(line)))
		return false;
	token = strtok_r(cmd_line, " |", &save);
	while (token) {
		if (!cq[cq_iter])
			assert(c = cq[cq_iter] = calloc(1, sizeof(*c)));
		assert(c->argv = realloc(c->argv, sizeof(*(c->argv)) * (c->argc + 2)));
		assert(c->argv[c->argc++] = strdup(token));
		c->argv[c->argc] = NULL;
		if ('|' == *save)
			++cq_iter;
		token = strtok_r(NULL, " |", &save);
	}
	free(cmd_line);
	return true;
}

static inline void	cmd_solorun(const struct command *restrict cmd) {
	g_child = vfork();
	switch (g_child) {
		case  0:
			DBG_INFO("child  | %d(%d) %s\n", getpid(), getppid(), cmd->argv[0]);
			execvp(cmd->argv[0], cmd->argv);
		case -1:
			errx(EXIT_FAILURE, "%s: command not found...", cmd->argv[0]);
			break ;
		default: {
			DBG_INFO("wait   | %d\n", g_child);
			waitpid(g_child, NULL, WUNTRACED);
			break ;
		}
	}
}

static inline void	cmd_run(const size_t cq_length,
					struct command *restrict *restrict cq) {
	for (size_t i = 0; cq_length > i; i++)
		if (cmd_builtinrun(cq[i]))
			return ;

	DBG_INFO("parent | %d(%d)\n", getpid(), getppid());
	if (1 < cq_length) {
		if (!(g_child = fork())) {
			cmd_pipe_queuing(cq_length - 2, cq_length - 1, cq);
		} else if (0 < g_child) {
			DBG_INFO("wait   | %d\n", g_child);
			waitpid(g_child, NULL, WUNTRACED);
		}
	} else {
		cmd_solorun(cq[0]);
	}
}

int	main(int argc, char *argv[]) {
	g_defout = stdout;
	parse_opt(argc, argv);

	init_sig_handlers();

	setbuf(stdout, NULL);

	while (1) {
		g_is_cq_piped = false;
		g_child = 0;
		char *restrict	line;
		fprintf(g_defout, "$> ");

		if ((char*)-1 == (line = cmd_readline())) {
			rewind(stdin);
			fwrite("\n", sizeof(char), 1, g_defout);
			continue ;
		}
		if (!line)
			continue ;
		if (g_opt_stdout_redir)
			printf("$> %s\n", line);

		struct command	**cq;
		size_t	cq_length = cq_precalc_pipe_length(line);
		assert(cq = calloc(cq_length + 1, sizeof(*cq)));
		if (!cmd_parseline(line, cq))
			continue ;
		cmd_run(cq_length, cq);
		cq_free(cq_length, cq);
		free(line);
	}
}
