#include "minishell.h"

static inline void	shell_refresh_global_data(void) {
	g_child = 0;
}

static inline void	cq_free(struct command_queue *restrict cq) {
	for (size_t i = 0; cq->size > i && cq->cmd[i]; ++i) {
		if (cq->cmd[i]->argv) {
			for (size_t j = 0; cq->cmd[i]->argc >= j; j++)
				free(cq->cmd[i]->argv[j]);
			free(cq->cmd[i]->argv);
		}
		free(cq->cmd[i]);
	}
	free(cq);
}

static inline struct command_queue	*tokens_to_cq(dll_t *restrict tokens) {
	static dll_obj_t *restrict	last_obj;
	struct command_queue	*cq;
	struct command *restrict	icmd = NULL;
	dll_obj_t *restrict iobj =
		((!tokens) ? last_obj : (last_obj = tokens->head));

	if (!iobj)
		return NULL;
	assert(cq = calloc(1, sizeof(*cq)));
	cq->size = 1;
	cq->type = CQ_DEFAULT;
	assert(cq->cmd = calloc(cq->size, sizeof(*cq->cmd)));
	while (iobj) {
		struct s_token_key *restrict	k = dll_getdata(iobj);
		if (TK_PIPE == k->type) {
			cq->type = CQ_PIPE;
			++cq->size;
			last_obj = iobj = iobj->next;
			continue ;
		} else if (TK_MULTI_CMD == k->type) {
			last_obj = iobj->next;
			break ;
		}
		if (!cq->cmd[cq->size - 1]) {
			assert(icmd = cq->cmd[cq->size - 1] =
				calloc(cq->size, sizeof(icmd)));
		}
		assert(icmd->argv = realloc(icmd->argv,
			sizeof(*icmd->argv) * icmd->argc + 2));
		assert(icmd->argv[icmd->argc++] = strdup(k->str));
		icmd->argv[icmd->argc] = NULL;
		last_obj = iobj = iobj->next;
	};
	return cq;
}

static int	print_token(const void *restrict data) {
	static const char	*tk_str_types[] = {
		"EXEC", "OPT", "ARG", "PIPE", "REDIR", "REDIRA", "REDIRD", "SEMI"
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

		if (g_opt_dbg_level)
			dll_print(tokens, print_token);

		cq = tokens_to_cq(tokens);
		while (cq) {
			cmd_run(cq);
			cq_free(cq);
			cq = tokens_to_cq(NULL);
		}
		dll_free(tokens);
	}
}
