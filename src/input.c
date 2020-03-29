#include "minishell.h"

static struct termios	g_termios_save;

static void	input_disable_raw_mode(void) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_termios_save);
}

static inline void	input_raw_mode(void) {
	tcgetattr(STDIN_FILENO, &g_termios_save);

	struct termios	raw = g_termios_save;
	raw.c_iflag &= ~(ISTRIP | INPCK | IXON);
	raw.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
	raw.c_lflag |= (ECHONL | ECHOE | ECHOK);
	raw.c_cflag |= (CS8);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

	atexit(input_disable_raw_mode);
}

#define INPUT_BUFF_SIZE 1024

static char	buff[INPUT_BUFF_SIZE] = { 0 };
static size_t	ibuff = 0;

# define KEY_DEL 0x7f
# define KEY_ESC 0x1b
# define KEY_CTRL(k) ((k) & 0x1f)

//	hs == handler state from Input Lookup State[]
typedef enum e_handler_state {
	e_hs_continue = 0, // All input
	e_hs_stop,         // Ctrl+D, Ctrl+C or '\n'
	e_hs_exit,         // Ctrl+Q
	e_hs_eof           // read() error
} __attribute__((packed)) handler_state_t;

//	is == input state from key_read()
typedef enum e_input_state {
	e_is_char = 0, // Single character input
	e_is_ctrl,     // Character pressed with Ctrl
	e_is_seq,      // Escape Sequence
	e_is_eof,      // read() error
} __attribute__((packed)) input_state_t;

struct s_input_state {
	char	ch[4];
	input_state_t	state;
	char	__dummy_align[3] __attribute__((deprecated, unused));
} __attribute__((aligned(8)));

typedef handler_state_t	(*input_handler)(struct s_input_state);

/**
 *	Handle key presses
 */
static handler_state_t	__ispace(struct s_input_state is) {
	(void)is;
	if (ibuff && ' ' != buff[ibuff - 1]) {
		buff[ibuff++] = ' ';
		putchar(' ');
	}
	return e_hs_continue;
}

static handler_state_t	__iend_line(struct s_input_state is) {
	putchar(is.ch[0]);
	return e_hs_stop;
}

static handler_state_t	__iprintable(struct s_input_state is) {
	char	ch = is.ch[0];
	buff[ibuff++] = ch;
	putchar(ch);
	return e_hs_continue;
}

static handler_state_t	__ipipe(struct s_input_state is) {
	char	ch = is.ch[0];
	g_is_cq_piped = true;
	if (ibuff && ' ' != buff[ibuff - 1])
		buff[ibuff++] = ' ';
	buff[ibuff++] = ch;
	++g_cq_len;
	putchar(ch);
	return e_hs_continue;
}

static handler_state_t	__idel(struct s_input_state is) {
	(void)is;
	if (ibuff) {
		int	removed = buff[ibuff--];
		if ('|' == removed) {
			if (1 < g_cq_len)
				--g_cq_len;
			if (1 == g_cq_len)
				g_is_cq_piped = false;
		}
		printf("\b \b");
	}
	return e_hs_continue;
}

static handler_state_t	__ihome_path(struct s_input_state is) {
	(void)is;
	char *restrict	home = getpwuid(getuid())->pw_dir;

	strcpy(buff + ibuff, home);
	printf("%s", buff + ibuff);
	ibuff += strlen(home);
	return e_hs_continue;
}

/**
 *	Handle keys what pressed with Ctrl
 */
static handler_state_t	__ictrl_cd(struct s_input_state is) {
	(void)is;
	ibuff = 0;
	return e_hs_stop;
}

static handler_state_t	__ictrl_l(struct s_input_state is) {
	(void)is;
	fprintf(g_defout, "\x1b[2J");
	fprintf(g_defout, "\x1b[H");
	fprintf(g_defout, "$> ");
	for (size_t i = 0; ibuff > i; ++i)
		fwrite(&buff[i], sizeof(buff[i]), 1, g_defout);
	return e_hs_continue;
}

static handler_state_t	__ictrl_q(struct s_input_state is) {
	(void)is;
	fprintf(g_defout, "exit\n");
	return e_hs_exit;
}

static handler_state_t	__ictrl_j(struct s_input_state is) {
	(void)is;
	putchar('\n');
	return e_hs_stop;
}

/**
 *	Handle Escape Sequences
 */
static handler_state_t	__iseq(struct s_input_state is) {
	DBG_INFO(" .SEQ: %d - '%c'\n", is.ch[2], is.ch[2]);
	return e_hs_continue;
};

/**
 *	Handle all invalid input from read()
 */
static handler_state_t	__ieof(struct s_input_state is) {
	(void)is;
	return e_hs_eof;
};

// ilt - Input Lookup Table
static const input_handler *restrict	__ilt[] = {
	[e_is_char] = (input_handler[]) {
		['\t'] = __ispace,
		['\n'] = __iend_line,
		['\v' ... '\r'] = __ispace,
		[' '] = __ispace,
		['!' ... '{'] = __iprintable,
		['|'] = __ipipe,
		['}'] = __iprintable,
		['~'] = __ihome_path,
		[KEY_DEL] = __idel,
	},
	[e_is_ctrl] = (input_handler[]) {
		[3 ... 4] = __ictrl_cd,
		[10] = __ictrl_j,
		[12] = __ictrl_l,
		[17] = __ictrl_q,
		[18 ... 127] = NULL
	},
	[e_is_seq] = (input_handler[]) { [0 ... 127] = __iseq },
	[e_is_eof] = (input_handler[]) { [0 ... 127] = __ieof }
};

static inline void	debug_info(struct s_input_state is) {
	int i = 0;
	while (is.ch[i]) {
		int	ch = is.ch[i++];
		int	__ch = KEY_CTRL(ch);
		DBG_INFO(" -> %d", ch);
		if (!iscntrl(ch))
			DBG_INFO(" -- '%c'", ch);
		DBG_INFO(" || %d", __ch);
		if (!iscntrl(__ch))
			DBG_INFO(" -- '%c'", __ch);
		if (g_opt_dbg_level) {
			putc('\n', stderr);
		}
	}
}

static inline struct s_input_state	key_read(void) {
	ssize_t	nread = 0;
	struct s_input_state	is;

	*((int*)is.ch) = 0;
	is.state = e_is_char;
	while (1 != (nread = read(STDIN_FILENO, &is.ch[0], 1))) {
		if (-1 == nread) {
			if (EAGAIN == errno) {
				err(EXIT_FAILURE, "read");
			} else {
				is.state = e_is_eof;
				return is;
			}
		}
	}
	if (iscntrl(is.ch[0]))
		is.state = e_is_ctrl;
	if ('\x1b' == is.ch[0]) {
		if (read(STDIN_FILENO, &is.ch[1], 1) != 1)
			return is;
		if (read(STDIN_FILENO, &is.ch[2], 1) != 1)
			return is;
		is.state = e_is_seq;
	}
	return is;
}

static inline handler_state_t	run_key_handler(struct s_input_state is) {
	input_handler	ih = __ilt[is.state][(int)is.ch[0]];
	handler_state_t	state = e_hs_continue;

	if (ih) {
		state = ih(is);
		buff[ibuff] = 0;
	}
	debug_info(is);
	return state;
}

char	*input_read(void) {
	input_raw_mode();

	handler_state_t	s = e_hs_continue;

	ibuff = 0;
	while (e_hs_continue == (s = run_key_handler(key_read())))
		;

	input_disable_raw_mode();

	switch (s) {
		case e_hs_stop: return ibuff ? strndup(buff, ibuff) : INPUT_CONTINUE;
		case e_hs_exit: return INPUT_EXIT;
		case e_hs_eof: return INPUT_EOF;
		default: return INPUT_CONTINUE;
	}
}
