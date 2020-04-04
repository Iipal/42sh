#ifndef MSH_INPUT_DATA_H
# define MSH_INPUT_DATA_H

# ifndef MSH_INPUT_DATA
#  error "deprecated inlcude msh_input_data.h"
# endif

# include <stddef.h>

# include "libdll.h"

# include "msh_dll_token.h"
# include "msh_attr.h"
//	hs == handler state from Input Lookup State[]
typedef enum e_handler_state {
	HS_CONTINUE = 0, // All input, just continue parsing
# define HS_CONTINUE HS_CONTINUE
	HS_STOP,         // Ctrl+D, Ctrl+C or '\n'
# define HS_STOP HS_STOP
	HS_EXIT,         // Ctrl+Q
# define HS_EXIT HS_EXIT
	HS_EOF           // read() error
# define HS_EOF HS_EOF
} msh_attr_pack handler_state_t;

//	is == input state from key_read()
typedef enum e_input_state {
	IS_CHAR = 0, // Single character input
# define IS_CHAR IS_CHAR
	IS_SEQ,      // Escape Sequence
# define IS_SEQ IS_SEQ
	IS_EOF,      // read() error
# define IS_EOF IS_EOF
} msh_attr_pack input_state_t;

#define INPUT_BUFF_SIZE 8096

typedef handler_state_t	(*input_handler_t)(void);

static char	g_ch[4] = { 0 };
static char	g_buff[INPUT_BUFF_SIZE] = { 0 };
static size_t	g_ibuff = 0;
static size_t	g_buff_len = 0;

static dll_obj_t *restrict	g_input_save = NULL;
static dll_obj_t *restrict	g_history_current = NULL;

static inline void	refresh_global_input_data(void) {
	bzero(g_buff, g_buff_len);
	if (g_input_save) {
		dll_freeobj(g_input_save);
		g_input_save = NULL;
	}
	g_ibuff = g_buff_len = 0;
}

# define KEY_DEL 0x7f
# define KEY_ESC 0x1b
# define KEY_CTRL(k) ((k) & 0x1f)

static inline handler_state_t	__ispace(void);
static inline handler_state_t	__inew_line(void);
static inline handler_state_t	__iprintable(void);
static inline handler_state_t	__ihome_path(void);
static inline handler_state_t	__idelch(void);

static inline handler_state_t	__ictrl_cd(void);
static inline handler_state_t	__ictrl_l(void);
static inline handler_state_t	__ictrl_q(void);

static inline handler_state_t	__iseq(void);

// IHLT - Input Handlers Lookup Table
static const input_handler_t	__ihlt[] = {
	[KEY_CTRL('C') ... KEY_CTRL('D')] = __ictrl_cd,
	[KEY_CTRL('|') /* '\t' */ ] = __ispace,
	[KEY_CTRL('J') /* '\n' */ ] = __inew_line,
	['\v'] = __ispace,
	[KEY_CTRL('L') /* '\f' */ ] = __ictrl_l,
	['\r'] = __ispace,
	[KEY_CTRL('Q')] = __ictrl_q,
	[' '] = __ispace,
	['!' ... '}'] = __iprintable,
	['~'] = __ihome_path,
	[KEY_DEL] = __idelch,
};

#endif /* MSH_INPUT_DATA_H */
