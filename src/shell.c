#include "minishell.h"

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

void	shell(void) {
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
