#include "minishell.h"

#define MSH_RAW_MODE_H
#include "msh_raw_mode.h"
#undef MSH_RAW_MODE_H

#define MSH_INPUT_DATA
#include "msh_input_data.h"
#undef MSH_INPUT_DATA

static void	__dlldel_token(void *restrict key) {
	struct tk_key *restrict	k = key;
	free(k->str);
	free(k);
}

static handler_state_t	__itoken_last_word(void) {
	if (g_ilword >= g_ibuff)
		return HS_STOP;
	size_t	last_len = g_ibuff - g_ilword;

	if (1 == last_len && (' ' == g_buff[g_ilword]
						|| '|' == g_buff[g_ilword]
						|| ';' == g_buff[g_ilword]))
		return HS_STOP;

	char *restrict str;
	assert((str = strndup(g_buff + g_ilword, last_len)));

	tk_type_t	tk_type = (('$' == *str) ? TK_ENV_VAR : TK_EXEC);
	struct tk_key	tk = { str, last_len, tk_type };
	const bool	force_push = (('$' == *str) || !dll_getlast(g_tokens));

	if (force_push) {
		g_lkey = get_key(dll_pushback(g_tokens, &tk, sizeof(tk),
			TK_DLL_BITS, __dlldel_token));
		return HS_STOP;
	}
	switch (g_lkey->type) {
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
	g_lkey = get_key(dll_pushback(g_tokens, &tk, sizeof(tk),
		TK_DLL_BITS, __dlldel_token));
	return HS_STOP;
}

// Handle key presses
static handler_state_t	__ispace(void) {
	if (g_ibuff && ' ' != g_buff[g_ibuff - 1]) {
		putchar(' ');
		__itoken_last_word();
		g_buff[g_ibuff++] = ' ';
		g_ilword = g_ibuff;
	}
	return HS_CONTINUE;
}

static handler_state_t	__inew_line(void) {
	putchar('\n');
	return __itoken_last_word();
}

static handler_state_t	__iprintable(void) {
	g_buff[g_ibuff++] = g_ch[0];
	if (g_ibuff && ' ' == g_buff[g_ibuff - 1])
		g_ilword = g_ibuff;
	putchar(g_ch[0]);
	return HS_CONTINUE;
}

static handler_state_t	__iredir(void) {
	putchar(g_ch[0]);
	if (TK_REDIR == g_lkey->type) {
		g_lkey->type = TK_REDIR_APP;
	} else if (TK_REDIR_APP == g_lkey->type) {
		dll_popback(g_tokens);
		__itoken_last_word();
	} else {
		struct tk_key	tk = { NULL, 0, TK_REDIR };
		g_lkey = get_key(dll_pushback(g_tokens, &tk, sizeof(tk),
			TK_DLL_BITS, __dlldel_token));
	}
	return HS_CONTINUE;
}

static handler_state_t	__ipipe(void) {
	if (!g_ibuff)
		return HS_CONTINUE;
	if (' ' != g_buff[g_ibuff])
		__ispace();
	if (TK_PIPE == get_key(dll_getlast(g_tokens))->type)
		return HS_CONTINUE;

	struct tk_key	tk = { NULL, 0, TK_PIPE };
	putchar('|');
	g_buff[g_ibuff++] = '|';
	g_lkey = get_key(dll_pushback(g_tokens, &tk, sizeof(tk),
		TK_DLL_BITS, __dlldel_token));
	return __ispace();
}

static handler_state_t	__imulticmd(void) {
	if (!g_ibuff)
		return HS_CONTINUE;
	if (' ' != g_buff[g_ibuff])
		__ispace();
	if (TK_MULTI_CMD == get_key(dll_getlast(g_tokens))->type)
		return HS_CONTINUE;

	struct tk_key	tk = { NULL, 0, TK_MULTI_CMD };
	putchar(';');
	g_buff[g_ibuff++] = ';';
	g_lkey = get_key(dll_pushback(g_tokens, &tk, sizeof(tk),
		TK_DLL_BITS, __dlldel_token));
	return __ispace();
}

static handler_state_t	__ihome_path(void) {
	char *restrict	home = getpwuid(getuid())->pw_dir;
	size_t	home_len = strlen(home);

	strncpy(g_buff + g_ibuff, home, home_len);
	fwrite(g_buff + g_ibuff, home_len, 1, stdout);
	g_ibuff += home_len;
	return HS_CONTINUE;
}

static handler_state_t	__idelch(void) {
	if (!g_ibuff)
		return HS_CONTINUE;
	char	removed = g_buff[g_ibuff - 1];
	char	prev_ch = ((g_ibuff - 2) ? g_buff[g_ibuff - 2] : 0);
	printf("\b \b");
	g_buff[--g_ibuff] = 0;
	if (' ' == removed && prev_ch) {
		dll_popback(g_tokens);
		if (dll_getlast(g_tokens))
			g_lkey = get_key(dll_getlast(g_tokens));
		char	*last_word = strrchr(g_buff, ' ');
		g_ilword = ((last_word) ? (g_buff - last_word) : 0);
	}
	return HS_CONTINUE;
}

// Handle keys what pressed with Ctrl
static handler_state_t	__ictrl_cd(void) {
	refresh_global_input_data();
	printf("\n$> ");
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_l(void) {
	printf("\x1b[2J");
	printf("\x1b[H");
	printf("$> ");
	fwrite(g_buff, g_ibuff, 1, stdout);
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_q(void) {
	if (!g_ibuff)
		printf("exit\n");
	refresh_global_input_data();
	return HS_EXIT;
}

// Handle Escape Sequences
static handler_state_t	__iseq(void) {
	size_t i = 1;
	DBG_INFO("%c", '\n');
	while (4 > i && g_ch[i]) {
		DBG_INFO(" .SEQ: %d - '%c'\n", g_ch[i], g_ch[i]);
		++i;
	}
	return HS_CONTINUE;
};

// Handle all invalid input from read()
static handler_state_t	__ieof(void) {
	return HS_EOF;
};

// Read key go g_ch and setup current input character type to g_is_state
static inline void	key_read(void) {
	ssize_t	nread = 0;

	*((int*)g_ch) = 0;
	g_is_state = IS_CHAR;
	while (1 != (nread = read(STDIN_FILENO, &g_ch[0], 1))) {
		if (-1 == nread) {
			if (EAGAIN == errno) {
				err(EXIT_FAILURE, "read");
			} else {
				g_is_state = IS_EOF;
				return ;
			}
		}
	}
	if (iscntrl(g_ch[0]))
		g_is_state = IS_CTRL;
	if ('\x1b' == g_ch[0]) {
		if (read(STDIN_FILENO, &g_ch[1], 1) != 1)
			return ;
		if (read(STDIN_FILENO, &g_ch[2], 1) != 1)
			return ;
		g_is_state = IS_SEQ;
	}
}

static inline handler_state_t	run_key_handler(void) {
	input_handler_t	ih = __ihlt[g_is_state][(int)g_ch[0]];
	handler_state_t	state = HS_CONTINUE;

	if (ih) {
		state = ih();
		if (IS_SEQ != g_is_state)
			g_buff[g_ibuff] = 0;
	}
	return state;
}

dll_t	*input_read(void) {
	refresh_global_input_data();
	handler_state_t	input_ret = HS_CONTINUE;
	dll_t	*out_ptr = g_tokens = dll_init(DLL_GBIT_QUIET);

	input_raw_mode();

	do {
		key_read();
		input_ret = run_key_handler();
	} while (HS_CONTINUE == input_ret);

	input_disable_raw_mode();

	switch (input_ret) {
		case HS_STOP: {
			if (!g_ibuff)
				return INPUT_CONTINUE;
			g_tokens = NULL;
			return out_ptr;
		}
		case HS_EXIT: return INPUT_EXIT;
		case HS_EOF: return INPUT_EOF;
		default: return INPUT_CONTINUE;
	}
}
