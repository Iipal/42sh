#include "minishell.h"

static inline void	shell_refresh_global_data(void) {
	g_child = 0;
}

static inline void	cq_free(struct command_queue *restrict cq) {
	for (size_t i = 0; cq->size >= i && cq->cmd[i]; ++i) {
		if (cq->cmd[i]->argv) {
			for (size_t j = 0; cq->cmd[i]->argc >= j; j++)
				free(cq->cmd[i]->argv[j]);
			free(cq->cmd[i]->argv);
		}
		free(cq->cmd[i]);
	}
	free((void*)cq);
}

static inline struct command_queue
*tokens_to_cq(dll_t *restrict tokens, struct command_queue *restrict cq) {
	(void)tokens;
	(void)cq;
	return NULL;
}

static int	print_token(const void *restrict data) {
	static const char	*tk_str_types[] = {
		"EXEC", "OPT", "ARG", "PIPE", "REDIR", "REDIRA", "REDIRD", NULL
	};
	const struct s_token_key *restrict	tk = data;

	DBG_INFO(" %s: %s(%zu)\n", tk_str_types[tk->type], tk->str, tk->len);
	return 0;
}

void	shell(void) {
	dll_t *restrict	tokens = NULL;
	struct command_queue	*cq = NULL;
	while (1) {
		shell_refresh_global_data();
		printf("$> ");
		tokens = input_read();

		if (INPUT_EOF == tokens) {
			rewind(stdin);
			puts("");
			continue ;
		} else if (INPUT_EXIT == tokens) {
			break ;
		} else if (INPUT_CONTINUE == tokens) {
			continue ;
		}

		dll_print(tokens, print_token);
		continue ;

		while ((cq = tokens_to_cq(tokens, cq))) {
			cmd_run(cq);
			cq_free(cq);
		}
		dll_free(tokens);
	}
}
