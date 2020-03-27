#include "minishell.h"

#define CANON_MODE_MAX_LINE 4096

static char	buff[CANON_MODE_MAX_LINE * 2] = { 0 };
static size_t	ibuff = 0;

typedef enum e_handler_state {
	e_hs_continue = 0,
	e_hs_exit = 1
} __attribute__((packed)) handler_state_t;

typedef handler_state_t	(*fsm_handler)(int);

static inline handler_state_t	__fsm_space(int ch) {
	(void)ch;
	if (ibuff && ' ' != buff[ibuff - 1]) {
		buff[ibuff++] = ' ';
		putchar(' ');
	}
	return e_hs_continue;
}
static inline handler_state_t	__fsm_end_line(int ch) {
	putchar(ch);
	return e_hs_exit;
}
static inline handler_state_t	__fsm_printable(int ch) {
	buff[ibuff++] = ch;
	putchar(ch);
	return e_hs_continue;
}
static inline handler_state_t	__fsm_pipe(int ch) {
	g_is_cq_piped = true;
	if (ibuff && ' ' != buff[ibuff - 1])
		buff[ibuff++] = ' ';
	buff[ibuff++] = ch;
	++g_cq_len;
	putchar(ch);
	return e_hs_continue;
}
static inline handler_state_t	__fsm_del(int ch) {
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
static inline handler_state_t	__fsm_home_path(int ch) {
	(void)ch;
	char *restrict	home = getpwuid(getuid())->pw_dir;

	strcpy(buff + ibuff, home);
	printf("%s", buff + ibuff);
	ibuff += strlen(home);
	return e_hs_continue;
}

# define KEY_PRINTABLE 32 ... 126
# define KEY_DEL 127

static const fsm_handler	__fsm_lt[] = {
	['\t'] = __fsm_space,
	['\n'] = __fsm_end_line,
	['\v' ... '\r'] = __fsm_space,
	[' '] = __fsm_space,
	[33 ... 123] = __fsm_printable,
	['|'] = __fsm_pipe,
	['}'] = __fsm_printable,
	['~'] = __fsm_home_path,
	[KEY_DEL] = __fsm_del,
};

static struct termios	g_termios_save;

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

	fsm_handler	fsm = NULL;
	int	ch = 0;

	while (EOF != read(STDIN_FILENO, &ch, 1)) {
		if ((fsm = __fsm_lt[ch]))
			if (e_hs_exit == fsm(ch))
				break ;
		buff[ibuff] = 0;
	}

	if (-1 == ch)
		return (char*)-1;
	char	*out;
	assert(out = strndup(buff, ibuff));
	ibuff = 0;
	return out;
}
