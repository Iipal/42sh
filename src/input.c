#include "minishell.h"

#define MSH_RAW_MODE_H
#include "msh_raw_mode.h"
#undef MSH_RAW_MODE_H

#define MSH_INPUT_DATA
#include "msh_input_data.h"
#undef MSH_INPUT_DATA

// Handle key presses
static handler_state_t	__ispace(void) {
	if (g_ibuff && ' ' != g_buff[g_ibuff - 1]) {
		putchar(' ');
		g_buff[g_ibuff++] = ' ';
	}
	return HS_CONTINUE;
}

static handler_state_t	__inew_line(void) {
	putchar('\n');
	return HS_STOP;
}

static handler_state_t	__iprintable(void) {
	g_buff[g_ibuff++] = g_ch[0];
	putchar(g_ch[0]);
	return HS_CONTINUE;
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
	g_buff[--g_ibuff] = 0;
	printf("\b \b");
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

// Read key go g_ch and setup current input character type to g_is_state
static inline input_state_t	key_read(void) {
	ssize_t	nread = 0;

	*((int*)g_ch) = 0;
	while (1 != (nread = read(STDIN_FILENO, &g_ch[0], 1))) {
		if (-1 == nread) {
			if (EAGAIN == errno) {
				err(EXIT_FAILURE, "read");
			} else {
				return IS_EOF;
			}
		}
	}
	if ('\x1b' == g_ch[0]) {
		if (read(STDIN_FILENO, &g_ch[1], 1) != 1)
			return IS_CHAR;
		if (read(STDIN_FILENO, &g_ch[2], 1) != 1)
			return IS_CHAR;
		return IS_SEQ;
	}
	return IS_CHAR;
}

static inline handler_state_t	run_key_handler(input_state_t is) {
	handler_state_t	state = HS_CONTINUE;
	switch (is) {
		case IS_SEQ: return __iseq(); break ;
		case IS_EOF: return HS_EOF; break ;
		default: {
			input_handler_t	ih = __ihlt[(int)g_ch[0]];
			if (ih) {
				return ih();
			}
		}
	}
	return state;
}

char	*input_read(void) {
	refresh_global_input_data();
	handler_state_t	hs = HS_CONTINUE;

	input_raw_mode();
	while (HS_CONTINUE == (hs = run_key_handler(key_read())))
		;
	input_disable_raw_mode();

	switch (hs) {
		case HS_STOP: {
			if (!g_ibuff)
				return INPUT_CONTINUE;
			return strndup(g_buff, g_ibuff);
		}
		case HS_EXIT: return INPUT_EXIT;
		case HS_EOF: return INPUT_EOF;
		default: return INPUT_CONTINUE;
	}
}
