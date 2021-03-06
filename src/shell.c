#include "minishell.h"

void	cq_free(struct command_queue *restrict cq) {
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

static void	del_token(void *restrict data) {
	struct tk_key *restrict	tk = data;
	if (tk->str) {
		free(tk->str);
	}
	free(tk);
}
static ssize_t	print_token(void *restrict data, void *restrict ptr, size_t idx) {
	(void)ptr;
	static const char	*token_types[] = {
		"EXEC", "OPT", "ARG", "PIPE", "REDIR", "REDIRA", "REDIRD", "SEMI", "ENV"
	};
	const struct tk_key *restrict	key = data;

	DBG_INFO(" -- [%2zu]: %s: '%s'\n", idx, token_types[key->type],
		key->str ? key->str : "(null)");
	return 0;
}

static inline struct command_queue	*tokens_to_cq(dll_t *restrict tokens) {
	static dll_obj_t *restrict	last_obj;
	dll_obj_t *restrict iobj =
		((!tokens) ? last_obj : (last_obj = tokens->head));
	if (!iobj)
		return NULL;

	struct command_queue *restrict	cq = NULL;
	struct command *restrict	icmd = NULL;
	struct tk_key *restrict	k = NULL;
	char *restrict	env_var_temp = NULL;

	assert(cq = calloc(1, sizeof(*cq)));
	assert(cq->cmd = calloc(2, sizeof(*cq->cmd)));
	cq->type = CQ_DEFAULT;
	cq->size = 1;

	while (iobj) {
		k = dll_getdata(iobj);
		if (TK_PIPE == k->type) {
			cq->type = CQ_PIPE;
			++cq->size;
			last_obj = iobj = iobj->next;
			continue ;
		} else if (TK_MULTI_CMD == k->type) {
			last_obj = iobj->next;
			break ;
		} else if (TK_ENV_VAR == k->type) {
			env_var_temp = getenv(k->str + 1);
		}

		if (!cq->cmd[cq->size - 1]) {
			assert(icmd = cq->cmd[cq->size - 1] =
				calloc(cq->size + 1, sizeof(icmd)));
		}
		assert(icmd->argv =
			realloc(icmd->argv, sizeof(*icmd->argv) * (icmd->argc + 2)));
		if (env_var_temp) {
			assert(icmd->argv[icmd->argc] = strdup(env_var_temp));
			env_var_temp = NULL;
		} else {
			assert(icmd->argv[icmd->argc] = strdup(k->str));
		}
		icmd->argv[++icmd->argc] = NULL;
		last_obj = iobj = iobj->next;
	};
	return cq;
}

static inline struct tk_key	*token_last_word(size_t ilword, size_t ibuff,
		char *restrict line, dll_t *restrict tokens,
		struct tk_key *restrict ltok) {
	if (ilword >= ibuff)
		return ltok;
	dll_obj_t *restrict	new_obj;
	struct tk_key	tk;
	size_t	last_len = ibuff - ilword;

	bzero(&tk, sizeof(tk));
	if (1 == last_len) {
		switch (line[ilword]) {
			case '|': {
				tk.type = TK_PIPE;
				dll_assert(new_obj = dll_pushback(tokens, &tk, sizeof(tk),
					DLL_BIT_DUP, del_token));
				return dll_getdata(new_obj);
			}
			case ';': {
				tk.type = TK_MULTI_CMD;
				dll_assert(new_obj = dll_pushback(tokens, &tk, sizeof(tk),
					DLL_BIT_DUP, del_token));
				return dll_getdata(new_obj);
			}
			case ' ': return ltok;
			default: break ;
		}
	}

	char *restrict str;
	assert((str = strndup(line + ilword, last_len)));
	tk_type_t	tk_type = (('$' == *str) ? TK_ENV_VAR : TK_EXEC);
	tk = (struct tk_key) { str, last_len, tk_type };
	if (('$' == *str) || !dll_getlast(tokens)) {
		dll_assert(new_obj = dll_pushback(tokens, &tk, sizeof(tk),
			DLL_BIT_DUP | DLL_BIT_EIGN, del_token));
		return dll_getdata(new_obj);
	}
	switch (ltok->type) {
		case TK_OPT:
		case TK_EXEC: tk_type = (('-' == *str) ? TK_OPT : TK_ARG); break ;
		case TK_ARG:
		case TK_ENV_VAR: tk_type = TK_ARG; break ;
		case TK_REDIR:
		case TK_REDIR_APP: tk_type = TK_REDIR_DST; break ;
		case TK_PIPE:
		case TK_REDIR_DST:
		case TK_MULTI_CMD:
		default: tk_type = TK_EXEC; break ;
	}
	tk.type = tk_type;
	dll_assert(new_obj = dll_pushback(tokens, &tk, sizeof(tk),
		DLL_BIT_DUP | DLL_BIT_EIGN, del_token));
	return dll_getdata(new_obj);
}

static inline dll_t	*tokenize_line_to_dll(char *restrict line) {
	dll_t *restrict	out = NULL;
	struct tk_key *restrict	lkey = NULL;
	size_t	ilword = 0;
	size_t	i = 0;

	dll_assert(out = dll_init(DLL_BIT_DFLT));
	while (line[i]) {
		if (' ' == line[i] || !line[i + 1]) {
			size_t	ilword_len = i + (!line[i + 1] && ' ' != line[i]);
			lkey = token_last_word(ilword, ilword_len, line, out, lkey);
			ilword = i + 1;
		} else if ('|' == line[i] || ';' == line[i]) {
			if (i && ' ' != line[i - 1]) {
				lkey = token_last_word(ilword, i, line, out, lkey);
				ilword = i;
			}
			lkey = token_last_word(ilword, i + 1, line, out, lkey);
			ilword = i + 1;
		}
		++i;
	}
	return out;
}

void	shell(void) {
	char *restrict	line = NULL;
	struct command_queue *restrict	cq = NULL;
	dll_t *restrict	tokens = NULL;
	while (1) {
		fwrite("$> ", 3, 1, stdout);
		line = input_read();

		if (INPUT_EOF == line) {
			rewind(stdin);
			fwrite("\n", 1, 1, stdout);
			continue ;
		} else if (INPUT_EXIT == line) {
			break ;
		} else if (INPUT_CONTINUE == line) {
			continue ;
		}

		tokens = tokenize_line_to_dll(line);
		if (g_opt_dbg_level)
			dll_print(tokens, print_token);

		cq = tokens_to_cq(tokens);
		while (cq) {
			cmd_run(cq);
			cq_free(cq);
			cq = tokens_to_cq(NULL);
		}
		dll_free(&tokens);

		add_to_history(line, strlen(line));
	}
}
