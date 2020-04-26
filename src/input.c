#include "minishell.h"

#include <termios.h>

# define FILL_EMPTY_LINE "                                                                                                                                                                                     "
# define FILL_MOVE_BACK "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"

static struct termios	termios_save;

static inline void	input_disable_raw_mode(void) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_save);
}

static inline void	input_raw_mode(void) {
	tcgetattr(STDIN_FILENO, &termios_save);

	struct termios	raw = termios_save;
	raw.c_iflag &= ~(ISTRIP | INPCK | IXON);
	raw.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
	raw.c_lflag |= (ECHONL | ECHOE | ECHOK);
	raw.c_cflag |= (CS8);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

	atexit(input_disable_raw_mode);
}

//	hs == handler state from Input Lookup State[]
typedef enum e_handler_state {
	HS_CONTINUE = 0, // All input, just continue parsing
	HS_STOP,         // Ctrl+D, Ctrl+C or '\n'
	HS_EXIT,         // Ctrl+Q
	HS_EOF,          // read() error
} msh_attr_pack handler_state_t;

//	is == input state from key_read()
typedef enum e_input_state {
	IS_CHAR = 0, // Single character input
	IS_SEQ,      // Escape Sequence
	IS_EOF,      // read() error
} msh_attr_pack input_state_t;

typedef handler_state_t	(*input_handler_t)(void);

#define INPUT_BUFF_SIZE 8096
#define MAX_SEQ_BYTES 4

static char	g_ch[MAX_SEQ_BYTES] = { 0 };
static char	g_buff[INPUT_BUFF_SIZE] = { 0 };
static size_t	g_ibuff = 0;
static size_t	g_buff_len = 0;

static dll_obj_t *restrict	g_input_save = NULL;
static dll_obj_t *restrict	g_history_current = NULL;

static dll_t *restrict	g_currdir_suggestions = NULL;
static dll_obj_t *restrict	g_selected_suggest = NULL;
static size_t	g_suggest_bytes_printed = 0;

struct suggest_obj {
	char *restrict	name;
	size_t	name_len;
	size_t	real_len;
	size_t	offset;
	bool	selected;
};

static void	suggest_del(void *restrict data) {
	struct suggest_obj	*so = data;
	free(so->name);
	free(so);
}

static inline void	clear_suggestion_from_screen(const struct suggest_obj *restrict so) {
	size_t	prompt_len_printed = g_buff_len + so->name_len + (sizeof("$> ") - 1);
	fwrite(FILL_MOVE_BACK, prompt_len_printed, 1, stdout);
	fwrite(FILL_EMPTY_LINE, prompt_len_printed, 1, stdout);
	fwrite(FILL_MOVE_BACK, prompt_len_printed, 1, stdout);
	fwrite("\x1b[1A", 4, 1, stdout);
	fwrite(FILL_EMPTY_LINE, g_suggest_bytes_printed, 1, stdout);
	fwrite(FILL_MOVE_BACK, g_suggest_bytes_printed, 1, stdout);
}

static inline dll_t	*init_suggestions(void) {
	dll_t	*out;
	dll_assert(out = dll_init(DLL_BIT_DFLT));

	char	*dirname;
	assert(dirname = get_current_dir_name());

	struct dirent	*d = NULL;
	DIR	*dir = opendir(dirname);
	free(dirname);
	if (!dir)
		assert_perror(errno);

	struct suggest_obj	s;
	while ((d = readdir(dir))) {
		assert(s.name = strdup(d->d_name));
		s.name_len = strlen(d->d_name);
		s.selected = 0;
		dll_assert(dll_pushback(out, &s, sizeof(s), DLL_BIT_DUP, suggest_del));
	}
	if (-1 == closedir(dir))
		assert_perror(errno);
	return out;
}

static inline void	free_global_input_data(void) {
	dll_free(&g_currdir_suggestions);
	dll_freeobj(&g_input_save);
}

static inline void	refresh_global_input_data(void) {
	bzero(g_buff, g_buff_len);
	free_global_input_data();
	g_currdir_suggestions = init_suggestions();
	g_selected_suggest = NULL;
	g_ibuff = g_buff_len = g_suggest_bytes_printed = 0;
}

# define KEY_DEL 0x7f
# define KEY_ESC 0x1b
# define KEY_CTRL_MASK 0x1f
# define KEY_CTRL(k) ((k) & KEY_CTRL_MASK)

static handler_state_t	__ispace(void);
static handler_state_t	__inew_line(void);
static handler_state_t	__iprintable(void);
static handler_state_t	__ihome_path(void);
static handler_state_t	__idelch(void);
static handler_state_t	__isuggestions(void);

static handler_state_t	__ictrl_c(void);
static handler_state_t	__ictrl_l(void);
static handler_state_t	__ictrl_d(void);

static handler_state_t	__iseq(void);

// IHLT - Input Handlers Lookup Table
static const input_handler_t	__ihlt[] = {
	[KEY_CTRL('C')] = __ictrl_c,
	[KEY_CTRL('D')] = __ictrl_d,
	['\t'] = __isuggestions,
	[KEY_CTRL('J') /* '\n' */ ] = __inew_line,
	['\v'] = __ispace,
	[KEY_CTRL('L') /* '\f' */ ] = __ictrl_l,
	['\r'] = __ispace,
	[' '] = __ispace,
	['!' ... '}'] = __iprintable,
	['~'] = __ihome_path,
	[KEY_DEL] = __idelch,
};

// Handle key presses
static handler_state_t	__ispace(void) {
	if (g_ibuff && ' ' != g_buff[g_ibuff - 1]) {
		putchar(' ');
		size_t	shifted_cursor = g_buff_len - g_ibuff;
		if (shifted_cursor) {
			strncpy(g_buff + g_ibuff + 1, g_buff + g_ibuff, shifted_cursor);
			fwrite(g_buff + g_ibuff + 1, shifted_cursor, 1, stdout);
			fwrite(FILL_MOVE_BACK, shifted_cursor, 1, stdout);
		}
		++g_buff_len;
		g_buff[g_ibuff++] = ' ';
	}
	return HS_CONTINUE;
}

static handler_state_t	__inew_line(void) {
	if (g_selected_suggest) {
		struct suggest_obj *restrict	so;
		dll_assert(so = dll_getdata(g_selected_suggest));

		size_t	cursor_shifted = g_buff_len - g_ibuff;
		if (cursor_shifted) {
			char *restrict	save = strndupa(g_buff + g_ibuff, cursor_shifted);
			memcpy(g_buff + g_ibuff, so->name, so->name_len);
			memcpy(g_buff + g_ibuff + so->real_len, save, cursor_shifted);
		} else {
			memcpy(g_buff + g_ibuff, so->name + so->offset, so->real_len);
		}

		clear_suggestion_from_screen(so);
		g_ibuff += so->real_len;
		g_buff_len += so->real_len;
		fwrite("$> ", 3, 1, stdout);
		fwrite(g_buff, g_buff_len, 1, stdout);
		fwrite(FILL_MOVE_BACK, cursor_shifted, 1, stdout);

		g_selected_suggest = NULL;
		g_suggest_bytes_printed = 0;
		return HS_CONTINUE;
	} else {
		putchar('\n');
		return HS_STOP;
	}
}

static handler_state_t	__iprintable(void) {
	putchar(g_ch[0]);
	if (g_buff[g_ibuff]) {
		size_t	shifted_cursor = g_buff_len - g_ibuff;
		strncpy(g_buff + g_ibuff + 1, g_buff + g_ibuff, shifted_cursor);
		fwrite(g_buff + g_ibuff + 1, shifted_cursor, 1, stdout);
		fwrite(FILL_MOVE_BACK, shifted_cursor, 1, stdout);
	}
	++g_buff_len;
	g_buff[g_ibuff++] = g_ch[0];
	return HS_CONTINUE;
}

static handler_state_t	__ihome_path(void) {
	char *restrict	home = getpwuid(getuid())->pw_dir;
	size_t	home_len = strlen(home);

	strcpy(g_buff + g_ibuff, home);
	fwrite(g_buff + g_ibuff, home_len, 1, stdout);
	g_ibuff += home_len;
	g_buff_len += home_len;
	return HS_CONTINUE;
}

static handler_state_t	__idelch(void) {
	if (g_selected_suggest) {
		struct suggest_obj *restrict	so;
		dll_assert(so = dll_getdata(g_selected_suggest));
		clear_suggestion_from_screen(so);
		fwrite("$> ", 3, 1, stdout);
		fwrite(g_buff, g_buff_len, 1, stdout);
		fwrite(FILL_MOVE_BACK, g_buff_len - g_ibuff, 1, stdout);
		g_selected_suggest = NULL;
		g_suggest_bytes_printed = 0;
		return HS_CONTINUE;
	}
	if (!g_ibuff)
		return HS_CONTINUE;
	if (g_buff[g_ibuff]) {
		size_t	cursor_shifted = g_buff_len - g_ibuff;
		strncpy(g_buff + g_ibuff - 1, g_buff + g_ibuff, cursor_shifted);
		putchar('\b');
		fwrite(g_buff + g_ibuff - 1, cursor_shifted, 1, stdout);
		fwrite(" \b", 2, 1, stdout);
		fwrite(FILL_MOVE_BACK, cursor_shifted, 1, stdout);
	} else {
		fwrite("\b \b", 3, 1, stdout);
	}
	--g_ibuff;
	g_buff[--g_buff_len] = 0;
	return HS_CONTINUE;
}

static ssize_t	suggest_item_print(void *restrict data, void *restrict ptr, size_t idx) {
	(void)idx;
	(void)ptr;
	const struct suggest_obj *restrict	so = data;
	if (so->selected)
		g_suggest_bytes_printed += fwrite(">", 1, 1, stdout);
	fwrite(so->name, so->name_len, 1, stdout);
	fwrite(" ", 1, 1, stdout);
	g_suggest_bytes_printed += so->name_len + 1;
	return 0;
}

static ssize_t	find_suggests_match(void *restrict data, void *restrict ptr, size_t idx) {
	(void)idx;
	struct suggest_obj *restrict	obj = data;
	struct suggest_obj *restrict	find = ptr;
	int	ret = 0;
	if (0 != find->name_len)
		ret = strncmp(obj->name, find->name, find->name_len);
	return !ret ? 0 : 1;
}

static inline handler_state_t	__isuggestions(void) {
	static dll_t *restrict	suggests = NULL;
	static dll_obj_t *restrict	last = NULL;
	static char *restrict	str_last_search = NULL;
	struct suggest_obj	find_so = { NULL, 0, 0, 0, 0 };
	char *restrict	str_find = strrchr(g_buff, ' ');

	if (str_find && strlen(str_find)) {
		find_so.name = str_find + 1;
		find_so.offset = find_so.real_len = find_so.name_len = g_buff_len - (find_so.name - g_buff);
	} else {
		find_so.name = g_buff;
		find_so.real_len = find_so.name_len = find_so.offset = g_buff_len;
	}
	if (!str_last_search || (str_last_search && strcmp(find_so.name, str_last_search))) {
		dll_assert_soft(suggests = dll_dupkey(g_currdir_suggestions,
			find_suggests_match, &find_so, 1, dll_getsize(g_currdir_suggestions)));
		str_last_search = strdup(find_so.name);
	}

	if (!g_selected_suggest && last != dll_getlast(suggests)) {
		g_selected_suggest = dll_gethead(suggests);
	} else {
		if (!(g_selected_suggest = dll_getnext(g_selected_suggest)))
			g_selected_suggest = dll_gethead(suggests);
	}

	if (!g_selected_suggest) {
		g_suggest_bytes_printed = 0;
		return HS_CONTINUE;
	}

	if (g_suggest_bytes_printed) {
		struct suggest_obj *restrict	pso;
		dll_assert(pso = dll_getdata(last));
		clear_suggestion_from_screen(pso);
		g_suggest_bytes_printed = 0;
	} else {
		size_t	prompt_size = sizeof("$> ") - 1;
		fwrite(FILL_MOVE_BACK, g_ibuff + prompt_size, 1, stdout);
		fwrite(FILL_EMPTY_LINE, g_buff_len + prompt_size, 1, stdout);
		fwrite(FILL_MOVE_BACK, g_buff_len + prompt_size, 1, stdout);
	}
	struct suggest_obj *restrict	so = dll_getdata(g_selected_suggest);
	so->selected = 1;
	dll_assert_soft(dll_print(suggests, suggest_item_print));
	so->selected = 0;
	so->offset = find_so.offset;
	so->real_len = so->name_len - find_so.offset;
	fwrite("\n$> ", 4, 1, stdout);
	fwrite(g_buff, g_ibuff, 1, stdout);
	fwrite(so->name + so->offset, so->real_len, 1, stdout);
	size_t cursor_shifted = g_buff_len - g_ibuff;
	if (cursor_shifted) {
		fwrite(g_buff + g_ibuff, cursor_shifted, 1, stdout);
		fwrite(FILL_MOVE_BACK, cursor_shifted, 1, stdout);
	}
	last = g_selected_suggest;
	return HS_CONTINUE;
}

// Handle keys what pressed with Ctrl
static handler_state_t	__ictrl_c(void) {
	refresh_global_input_data();
	fwrite("\n$> ", 4, 1, stdout);
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_l(void) {
	size_t	cursor_shifted = g_buff_len - g_ibuff;
	fwrite("\x1b[2J\x1b[H$> ", 10, 1, stdout);
	fwrite(g_buff, g_buff_len, 1, stdout);
	fwrite(FILL_MOVE_BACK, cursor_shifted, 1, stdout);
	return HS_CONTINUE;
}

static handler_state_t	__ictrl_d(void) {
	size_t	cursor_shifted = g_buff_len - g_ibuff;

	if (cursor_shifted)
		fwrite(FILL_EMPTY_LINE, cursor_shifted, 1, stdout);
	if (g_buff_len) {
		size_t	start_line_len = sizeof("\r$> ") - 1;
		fwrite("\r$> ", start_line_len, 1, stdout);
		fwrite(FILL_EMPTY_LINE, g_buff_len, 1, stdout);
		fwrite("\r$> ", start_line_len, 1, stdout);
	}
	fwrite("exit\n", sizeof("exit\n") - 1, 1, stdout);

	free_global_input_data();
	strcpy(g_buff, "exit");
	g_ibuff = g_buff_len = sizeof("exit") - 1;
	g_buff[g_ibuff] = 0;
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

static ssize_t find_buff_dup(void *restrict data, void *restrict buff_cmp, size_t idx) {
	(void)idx;
	ssize_t	ret = strcmp(buff_cmp, data);
	if (0 > ret)
		ret = 1;
	return ret;
}

static inline void	__ihistory_updatesave(dll_obj_t *restrict obj) {
	if (obj != g_input_save) {
		if (g_input_save && !dll_findr(g_history, find_buff_dup, g_buff)) {
			char *restrict	save_str = dll_getdata(g_input_save);
			if (strcmp(save_str, g_buff)) {
				dll_freeobj(&g_input_save);
				dll_assert(g_input_save = dll_new(strdup(g_buff), g_buff_len,
					DLL_BIT_EIGN | DLL_BIT_FREE, NULL));
			}
		} else if (!g_input_save) {
			dll_assert(g_input_save = dll_new(strdup(g_buff), g_buff_len,
				DLL_BIT_EIGN | DLL_BIT_FREE, NULL));
		}
	}
}

static inline void	__ihistory_putdata(dll_obj_t *restrict obj) {
	char *restrict	str = dll_getdata(obj);
	size_t	len = dll_getdatasize(obj);
	size_t	cursor_shifted = g_buff_len - g_ibuff;

	if (g_buff_len) {
		if (cursor_shifted)
			fwrite(g_buff + g_ibuff, cursor_shifted, 1, stdout);
		bzero(g_buff, g_buff_len);
	}
	fwrite("\r$> ", sizeof("\r$> ") - 1, 1, stdout);
	fwrite(FILL_EMPTY_LINE, g_buff_len, 1, stdout);
	fwrite(FILL_MOVE_BACK, g_buff_len, 1, stdout);
	fwrite(str, len, 1, stdout);
	strcpy(g_buff, str);
	g_ibuff = g_buff_len = len;
}

static inline void	__ihistory_base(
		dll_obj_t *(*fn_get_next)(const dll_obj_t *restrict),
		dll_obj_t *(*fn_get_head)(const dll_t *restrict)) {
	if (!g_history_current) {
		__ihistory_updatesave((void*)0x1);
		if (!(g_history_current = fn_get_head(g_history))) {
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
			free_global_input_data();
			if (!g_buff_len)
				return INPUT_CONTINUE;
			return strndup(g_buff, g_buff_len);
		}
		case HS_EXIT: return INPUT_EXIT;
		case HS_EOF: return INPUT_EOF;
		default: return INPUT_CONTINUE;
	}
}
