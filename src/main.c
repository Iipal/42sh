#include "minishell.h"

pid_t	g_child = 0;
bool	g_is_cq_piped = false;
int	g_opt_dbg_level = 0;
int	g_opt_stdout_redir = 0;
static int	g_opt_help = 0;
FILE	*g_defout = NULL;

static inline __attribute__((nonnull(2)))
	void cq_free(const size_t cq_length, struct command **cq) {
	for (size_t i = 0; cq_length >= i && cq[i]; ++i) {
		if (cq[i]->argv) {
			for (size_t j = 0; cq[i]->argc >= j; j++)
				free(cq[i]->argv[j]);
			free(cq[i]->argv);
		}
		free(cq[i]);
	}
	free(cq);
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
	static struct option	l_opts[] = {
		{ "debug", no_argument, &g_opt_dbg_level   , 1 },
		{ "file" , no_argument, &g_opt_stdout_redir, 1 },
		{ "help" , no_argument, &g_opt_help        , 1 },
		{ 0      , 0          , 0                  , 0 }
	};

	int	opt;
	while (-1 != (opt = getopt_long(ac, av, "dfh", l_opts, NULL))) {
		switch (opt) {
			case 'd': g_opt_dbg_level = 1;    break ;
			case 'f': g_opt_stdout_redir = 1; break ;
			case 'h': g_opt_help = 1;         break ;
			case '?': exit(EXIT_FAILURE);     break ;
			default : break ;
		}
	}
	if (g_opt_help) {
		printf("Mini-Shell help:\n"
"\t-d(--debug)\toutput additional debug information to stderr\n"
"\t-f(--file) \tre-direct stdout to ./.msh.out (not applicable to builtins)\n"
"\t-h(--help) \tprint this info message and exit\n");
		cmd_fast_builtinrun(CMD_FAST_NEW(1, "help", NULL));
		exit(EXIT_SUCCESS);
	}
	if (g_opt_stdout_redir) {
		g_defout = stderr;
		add_redir_tofile("./.msh.out");
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
