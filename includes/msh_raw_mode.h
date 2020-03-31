#ifndef MSH_RAW_MODE_H
# error "msh_raw_mode.h is deprecated include, be careful"
#endif /* MSH_RAW_MODE_H */

#include <unistd.h>
#include <termios.h>

static struct termios	g_termios_save;

static inline void	input_disable_raw_mode(void) {
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
