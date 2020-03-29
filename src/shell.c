#include "minishell.h"

static inline void	cq_free(void) {
	for (size_t i = 0; g_cq_len >= i && g_cq[i]; ++i) {
		if (g_cq[i]->argv) {
			for (size_t j = 0; g_cq[i]->argc >= j; j++)
				free(g_cq[i]->argv[j]);
			free(g_cq[i]->argv);
		}
		free(g_cq[i]);
	}
	free((void*)g_cq);
}

static inline void	shell_refresh_global_data(void) {
	g_is_cq_piped = false;
	g_child = 0;
	if (g_cq) {
		cq_free();
		g_cq = NULL;
	}
	g_cq_len = 1;
}

static inline bool	line_tokenize_to_cq(char *restrict line,
					struct command *restrict *restrict cq) {
	struct command *restrict	c = NULL;
	size_t	cq_iter = 0;
	char	*save = NULL;
	char	*token = strtok_r(line, " |", &save);

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
	return true;
}

void	shell(void) {
	char *restrict	line;
	while (1) {
		shell_refresh_global_data();
		fprintf(g_defout, "$> ");
		line = input_read();

		if (INPUT_EOF == line) {
			rewind(stdin);
			fwrite("\n", sizeof(char), 1, g_defout);
			continue ;
		} else if (INPUT_EXIT == line) {
			break ;
		} else if (INPUT_CONTINUE == line) {
			continue ;
		}

		if (g_opt_stdout_redir)
			printf("$> %s\n", line);

		assert(g_cq = calloc(g_cq_len + 1, sizeof(*g_cq)));
		if (!line_tokenize_to_cq(line, g_cq))
			continue ;
		cmd_run(g_cq_len, g_cq);
		free(line);
	}
}
