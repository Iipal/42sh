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
		++g_buff_len;
	}
	return HS_CONTINUE;
}

static handler_state_t	__inew_line(void) {
	putchar('\n');
	return HS_STOP;
}

static handler_state_t	__iprintable(void) {
	g_buff[g_ibuff++] = g_ch[0];
	++g_buff_len;
	putchar(g_ch[0]);
	return HS_CONTINUE;
}

static handler_state_t	__ihome_path(void) {
	char *restrict	home = getpwuid(getuid())->pw_dir;
	size_t	home_len = strlen(home);

	strncpy(g_buff + g_ibuff, home, home_len);
	fwrite(g_buff + g_ibuff, home_len, 1, stdout);
	g_ibuff += home_len;
	g_buff_len += home_len;
	return HS_CONTINUE;
}

static handler_state_t	__idelch(void) {
	if (!g_ibuff)
		return HS_CONTINUE;
	if (g_buff[g_ibuff]) {
		putchar('\b');
		size_t	cursor_shifted = g_buff_len - g_ibuff;
		strcpy(g_buff + g_ibuff - 1, g_buff + g_ibuff);
		fwrite(g_buff + g_ibuff - 1, cursor_shifted, 1, stdout);
		fwrite(" \b", 2, 1, stdout);
		while (cursor_shifted--)
			putchar('\b');
	} else {
		fwrite("\b \b", 3, 1, stdout);;
		g_buff[--g_ibuff] = 0;
	}
	--g_buff_len;
	return HS_CONTINUE;
}

// Handle keys what pressed with Ctrl
static handler_state_t	__ictrl_cd(void) {
	refresh_global_input_data();
	fwrite("\n$> ", 4, 1, stdout);
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_l(void) {
	fwrite("\x1b[2J\x1b[H$> ", 10, 1, stdout);
	fwrite(g_buff, g_buff_len, 1, stdout);
	size_t	cursor_shifted = g_buff_len - g_ibuff;
	while (cursor_shifted--)
		putchar('\b');
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_q(void) {
	if (!g_ibuff)
		puts("exit");
	refresh_global_input_data();
	return HS_EXIT;
}

// Handle Escape Sequences
static inline void	__imove_cursos_left(void) {
	if (g_ibuff) {
		--g_ibuff;
		putchar('\b');
	}
}

static inline void	__imove_cursos_right(void) {
	if (g_buff[g_ibuff])
		fwrite(g_buff + g_ibuff++, 1, 1, stdout);
}

static int find_buff_dup(const void *restrict data) {
	int	ret = strcmp(g_buff, data);
	if (0 > ret)
		ret = 1;
	return ret;
}

static inline void	__ihistory_updatesave(dll_obj_t *restrict obj) {
	if (obj != g_input_save) {
		if (g_input_save && !dll_findkeyr(g_session_history, find_buff_dup)) {
			char *restrict	save_str = dll_getdata(g_input_save);
			if (strcmp(save_str, g_buff)) {
				dll_freeobj(g_input_save);
				g_input_save = dll_new(strdup(g_buff), g_buff_len,
					DLL_BIT_EIGN | DLL_BIT_FREE, NULL);
			}
		} else if (!g_input_save) {
			g_input_save = dll_new(strdup(g_buff), g_buff_len,
				DLL_BIT_EIGN | DLL_BIT_FREE, NULL);
		}
	}
}

static inline void	__ihistory_putdata(dll_obj_t *restrict obj) {
	char *restrict	str = dll_getdata(obj);
	size_t len = dll_getsizeobj(obj);

	if (g_buff_len) {
		if (g_buff[g_ibuff])
			fwrite(g_buff + g_ibuff, g_buff_len - g_ibuff, 1, stdout);
		bzero(g_buff, g_buff_len);
	}
	while (g_buff_len--)
		fwrite("\b \b", 3, 1, stdout);
	fwrite(str, len, 1, stdout);
	strcpy(g_buff, str);
	g_ibuff = g_buff_len = len;
}

static inline void	__ihistory_base(
		dll_obj_t *(*fn_get_next)(const dll_obj_t *restrict),
		dll_obj_t *(*fn_get_head)(const dll_t *restrict)) {
	if (!g_history_current) {
		__ihistory_updatesave((void*)0x1);
		if (!(g_history_current = fn_get_head(g_session_history))) {
			if (!g_input_save)
				return ;
			g_history_current = g_input_save;
		}
	} else {
		if (!(g_history_current = fn_get_next(g_history_current))) {
			if (!g_input_save)
				return ;
			g_history_current = g_input_save;
		}
	}
	__ihistory_putdata(g_history_current);
	if (g_history_current == g_input_save)
		g_history_current = NULL;
	return ;
}

static inline void	__ihistory_prev(void) {
	__ihistory_base(dll_getprev, dll_getlast);
}

static inline void	__ihistory_next(void) {
	__ihistory_base(dll_getnext, dll_gethead);
}

static handler_state_t	__iseq(void) {
	static void (*seq_handlers[])(void) = {
		__ihistory_prev, __ihistory_next,
		__imove_cursos_right, __imove_cursos_left
	};
	if ('[' == g_ch[1]) {
		if ('A' <= g_ch[2] && 'D' >= g_ch[2]) {
			seq_handlers[g_ch[2] - 'A']();
		} else {
			DBG_INFO("%c", '\n');
			size_t i = 0;
			while (4 > ++i && g_ch[i])
				DBG_INFO(" .SEQ: %d - '%c'\n", g_ch[i], g_ch[i]);
		}
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
		if (1 != read(STDIN_FILENO, &g_ch[1], 1))
			return IS_CHAR;
		if (1 != read(STDIN_FILENO, &g_ch[2], 1))
			return IS_CHAR;
		return IS_SEQ;
	}
	return IS_CHAR;
}

static inline handler_state_t	run_key_handler(input_state_t is) {
	switch (is) {
		case IS_SEQ: return __iseq();
		case IS_EOF: return HS_EOF;
		default: {
			input_handler_t	ih = __ihlt[(int)g_ch[0]];
			if (ih)
				return ih();
		}
	}
	return HS_CONTINUE;
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
			if (!g_buff_len)
				return INPUT_CONTINUE;
			return strndup(g_buff, g_buff_len);
		}
		case HS_EXIT: return INPUT_EXIT;
		case HS_EOF: return INPUT_EOF;
		default: return INPUT_CONTINUE;
	}
}
