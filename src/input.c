#include "minishell.h"

static struct termios	g_termios_save;

#define CANON_MODE_MAX_LINE 4096

static char	buff[CANON_MODE_MAX_LINE * 2] = { 0 };
static size_t	ibuff = 0;

typedef enum e_handler_state {
	e_hs_continue = 0,
	e_hs_exit = 1
} __attribute__((packed)) handler_state_t;

typedef handler_state_t	(*input_handler)(int);

static inline handler_state_t	__ispace(int ch) {
	(void)ch;
	if (ibuff && ' ' != buff[ibuff - 1]) {
		buff[ibuff++] = ' ';
		putchar(' ');
	}
	return e_hs_continue;
}

static inline handler_state_t	__iend_line(int ch) {
	putchar(ch);
	return e_hs_exit;
}

static inline handler_state_t	__iprintable(int ch) {
	buff[ibuff++] = ch;
	putchar(ch);
	return e_hs_continue;
}

static inline handler_state_t	__ipipe(int ch) {
	g_is_cq_piped = true;
	if (ibuff && ' ' != buff[ibuff - 1])
		buff[ibuff++] = ' ';
	buff[ibuff++] = ch;
	++g_cq_len;
	putchar(ch);
	return e_hs_continue;
}

static inline handler_state_t	__idel(int ch) {
	(void)ch;
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

static inline handler_state_t	__ihome_path(int ch) {
	(void)ch;
	char *restrict	home = getpwuid(getuid())->pw_dir;

	strcpy(buff + ibuff, home);
	printf("%s", buff + ibuff);
	ibuff += strlen(home);
	return e_hs_continue;
}

# define KEY_DEL 127

// ilt - Input Lookup Table
static const input_handler	__ilt[] = {
	['\t'] = __ispace,
	['\n'] = __iend_line,
	['\v' ... '\r'] = __ispace,
	[' '] = __ispace,
	['!' ... '{'] = __iprintable,
	['|'] = __ipipe,
	['}'] = __iprintable,
	['~'] = __ihome_path,
	[KEY_DEL] = __idel,
};

static void	input_disable_raw_mode(void) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_termios_save);
}

static inline void	input_raw_mode(void) {
	tcgetattr(STDIN_FILENO, &g_termios_save);

	struct termios	raw = g_termios_save;
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_lflag |= ECHOE | ECHOK | ECHONL;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

	atexit(input_disable_raw_mode);
}

char	*cmd_readline(void) {
	input_raw_mode();

	input_handler	ih = NULL;
	int	ch = 0;

	ibuff = 0;
	while (EOF != read(STDIN_FILENO, &ch, 1)) {
		if ((ih = __ilt[ch]))
			if (e_hs_exit == ih(ch))
				break ;
		buff[ibuff] = 0;
	}

	if (EOF == ch)
		return (char*)EOF;
	return strndup(buff, ibuff);
}
