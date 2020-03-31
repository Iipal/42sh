#include "minishell.h"

#define MSH_RAW_MODE_H
#include "msh_raw_mode.h"
#undef MSH_RAW_MODE_H

#define INPUT_BUFF_SIZE 8096

static char	buff[INPUT_BUFF_SIZE] = { 0 };
static size_t	ibuff = 0;
static size_t	ilword = 0;
static struct s_token_key	*lkey = NULL;

# define get_key(_obj) ((struct s_token_key*)dll_getdata(_obj))

# define KEY_DEL 0x7f
# define KEY_ESC 0x1b
# define KEY_CTRL(k) ((k) & 0x1f)

static handler_state_t	__itoken_last_word(struct s_input_state *restrict is) {
	if (ilword >= ibuff)
		return HS_STOP;

	size_t	last_len = ibuff - ilword;
	if (1 == last_len) {
		if (' ' == buff[ilword] || '|' == buff[ilword] || ';' == buff[ilword])
			return HS_STOP;
	}

	char *restrict str;
	assert((str = strndup(buff + ilword, last_len)));

	tk_type_t	tk_type = TK_EXEC;
	struct s_token_key	tk = { str, last_len, tk_type };

	if (!dll_getlast(is->tokens)) {
		lkey = get_key(dll_pushback(is->tokens, &tk, sizeof(tk), TK_DLL_BITS));
		return HS_STOP;
	}
	switch (lkey->type) {
		case TK_OPT:
		case TK_EXEC: tk_type = (('-' == *str) ? TK_OPT : TK_ARG); break ;
		case TK_ARG: tk_type = TK_ARG; break ;
		case TK_PIPE: tk_type = TK_EXEC; break ;
		case TK_REDIR:
		case TK_REDIR_APP: tk_type = TK_REDIR_DST; break ;
		case TK_REDIR_DST:
		case TK_MULTI_CMD:
		default: tk_type = TK_EXEC; break ;
	}
	tk.type = tk_type;
	lkey = get_key(dll_pushback(is->tokens, &tk, sizeof(tk), TK_DLL_BITS));
	return HS_STOP;
}

/**
 *	Handle key presses
 */
static handler_state_t	__ispace(struct s_input_state *restrict is) {
	(void)is;
	if (ibuff && ' ' != buff[ibuff - 1]) {
		putchar(' ');
		__itoken_last_word(is);
		buff[ibuff++] = ' ';
		ilword = ibuff;
	}
	return HS_CONTINUE;
}

static handler_state_t	__inew_line(struct s_input_state *restrict is) {
	putchar('\n');
	return __itoken_last_word(is);
}

static handler_state_t	__iprintable(struct s_input_state *restrict is) {
	if (' ' == buff[ibuff])
		ilword = ibuff + 1;
	buff[ibuff++] = is->ch[0];
	putchar(is->ch[0]);
	return HS_CONTINUE;
}

static handler_state_t	__iredir(struct s_input_state *restrict is) {
	putchar(is->ch[0]);
	if (TK_REDIR == lkey->type) {
		lkey->type = TK_REDIR_APP;
	} else if (TK_REDIR_APP == lkey->type) {
		dll_popback(is->tokens);
		__itoken_last_word(is);
	} else {
		struct s_token_key	tk = { NULL, 0, TK_REDIR };
		lkey = get_key(dll_pushback(is->tokens, &tk, sizeof(tk), TK_DLL_BITS));
	}
	return HS_CONTINUE;
}

static handler_state_t	__ipipe(struct s_input_state *restrict is) {
	if (ibuff && ' ' != buff[ibuff - 1])
		__ispace(is);

	dll_obj_t *restrict last = dll_getlast(is->tokens);
	dll_obj_t *restrict lprev = dll_getprev(last);
	if (TK_PIPE == get_key(last)->type && TK_PIPE == get_key(lprev)->type)
		return HS_CONTINUE;

	putchar('|');
	buff[ibuff++] = '|';

	struct s_token_key	tk = { NULL, 0, TK_PIPE };
	lkey = get_key(dll_pushback(is->tokens, &tk, sizeof(tk), TK_DLL_BITS));
	return __ispace(is);
}

static handler_state_t	__imulticmd(struct s_input_state *restrict is) {
	if (ibuff && ' ' != buff[ibuff - 1])
		__ispace(is);

	dll_obj_t *restrict last = dll_getlast(is->tokens);
	dll_obj_t *restrict lprev = dll_getprev(last);
	if (TK_MULTI_CMD == get_key(last)->type
	&& TK_MULTI_CMD == get_key(lprev)->type)
		return HS_CONTINUE;

	putchar(';');
	buff[ibuff++] = ';';

	struct s_token_key	tk = { NULL, 0, TK_MULTI_CMD };
	lkey = get_key(dll_pushback(is->tokens, &tk, sizeof(tk), TK_DLL_BITS));
	return __ispace(is);
}

static handler_state_t	__ihome_path(struct s_input_state *restrict is) {
	(void)is;
	char *restrict	home = getpwuid(getuid())->pw_dir;

	strcpy(buff + ibuff, home);
	printf("%s", buff + ibuff);
	ibuff += strlen(home);
	return HS_CONTINUE;
}

static handler_state_t	__idelch(struct s_input_state *restrict is) {
	if (!ibuff)
		return HS_CONTINUE;
	printf("\b \b");
	char	removed = buff[ibuff - 1];
	char	prev_ch = ((ibuff - 2) ? buff[ibuff - 2] : 0);
	buff[--ibuff] = 0;
	if (' ' == removed && prev_ch) {
		dll_popback(is->tokens);
		if (dll_getlast(is->tokens))
			lkey = get_key(dll_getlast(is->tokens));
		char	*last_word = strrchr(buff, ' ');
		ilword = ((last_word) ? (buff - last_word) : 0);
	}
	return HS_CONTINUE;
}

/**
 *	Handle keys what pressed with Ctrl
 */
static handler_state_t	__ictrl_cd(struct s_input_state *restrict is) {
	(void)is;
	ibuff = 0;
	printf("\n$> ");
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_l(struct s_input_state *restrict is) {
	(void)is;
	printf("\x1b[2J");
	printf("\x1b[H");
	printf("$> ");
	fwrite(buff, ibuff, sizeof(char), stdout);
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_q(struct s_input_state *restrict is) {
	(void)is;
	if (!ibuff)
		printf("exit\n");
	return HS_EXIT;
}

/**
 *	Handle Escape Sequences
 */
static handler_state_t	__iseq(struct s_input_state *restrict is) {
	DBG_INFO(" .SEQ: %d - '%c'\n", is->ch[2], is->ch[2]);
	return HS_CONTINUE;
};

/**
 *	Handle all invalid input from read()
 */
static handler_state_t	__ieof(struct s_input_state *restrict is) {
	(void)is;
	return HS_EOF;
};

// ilt - Input Lookup Table
static const input_handler *restrict	__ilt[] = {
	[IS_CHAR] = (input_handler[]) {
		['\t'] = __ispace,
		['\n'] = __inew_line,
		['\v' ... '\r'] = __ispace,
		[' '] = __ispace,
		['!' ... ':'] = __iprintable,
		[';'] = __imulticmd,
		['<' ... '=' ] = __iprintable,
		['>'] = __iredir,
		['?' ... '{'] = __iprintable,
		['|'] = __ipipe,
		['}'] = __iprintable,
		['~'] = __ihome_path,
		[KEY_DEL] = __idelch,
	},
	[IS_CTRL] = (input_handler[]) {
		[KEY_CTRL('C') ... KEY_CTRL('D')] = __ictrl_cd, // Ctrl+C, Ctrl+D
		[KEY_CTRL('J')] = __inew_line, // Ctrl+J
		[KEY_CTRL('L')] = __ictrl_l,   // Ctrl+L
		[KEY_CTRL('Q')] = __ictrl_q,   // Ctrl+Q
		[KEY_DEL] = __idelch // DEL
	},
	[IS_SEQ] = (input_handler[]) { [0 ... 127] = __iseq },
	[IS_EOF] = (input_handler[]) { [0 ... 127] = __ieof }
};

static inline void	debug_info(struct s_input_state *restrict is) {
	int i = -1;
	while (4 > ++i && is->ch[i]) {
		int	ch = is->ch[i];
		int	__ch = KEY_CTRL(ch);
		DBG_INFO(" -> %d", ch);
		if (!iscntrl(ch))
			DBG_INFO(" -- '%c'", ch);
		DBG_INFO(" || %d", __ch);
		if (!iscntrl(__ch))
			DBG_INFO(" -- '%c'", __ch);
		DBG_INFO(" < (%d)\n", is->state);
	}
}

static inline struct s_input_state
*key_read(struct s_input_state *restrict is) {
	ssize_t	nread = 0;

	*((int*)is->ch) = 0;
	is->state = IS_CHAR;
	while (1 != (nread = read(STDIN_FILENO, &is->ch[0], 1))) {
		if (-1 == nread) {
			if (EAGAIN == errno) {
				err(EXIT_FAILURE, "read");
			} else {
				is->state = IS_EOF;
				return is;
			}
		}
	}
	if (iscntrl(is->ch[0]))
		is->state = IS_CTRL;
	if ('\x1b' == is->ch[0]) {
		if (read(STDIN_FILENO, &is->ch[1], 1) != 1)
			return is;
		if (read(STDIN_FILENO, &is->ch[2], 1) != 1)
			return is;
		is->state = IS_SEQ;
	}
	return is;
}

static inline handler_state_t
run_key_handler(struct s_input_state *restrict is) {
	input_handler	ih = __ilt[is->state][(int)is->ch[0]];
	handler_state_t	state = HS_CONTINUE;

	if (ih) {
		state = ih(is);
		buff[ibuff] = 0;
	}
	debug_info(is);
	return state;
}

dll_t	*input_read(void) {
	handler_state_t	input_ret = HS_CONTINUE;
	struct s_input_state	is;
	is.tokens = dll_init(DLL_GBIT_QUIET);

	input_raw_mode();

	ibuff = 0;
	ilword = 0;
	lkey = NULL;
	while (HS_CONTINUE == (input_ret = run_key_handler(key_read(&is))))
		;

	input_disable_raw_mode();

	switch (input_ret) {
		case HS_STOP: {
			if (!ibuff)
				return INPUT_CONTINUE;
			return is.tokens;
		}
		case HS_EXIT: return INPUT_EXIT;
		case HS_EOF: return INPUT_EOF;
		default: return INPUT_CONTINUE;
	}
}
