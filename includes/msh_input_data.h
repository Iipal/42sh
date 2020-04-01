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
	IS_CTRL,     // Character pressed with Ctrl
# define IS_CTRL IS_CTRL
	IS_SEQ,      // Escape Sequence
# define IS_SEQ IS_SEQ
	IS_EOF,      // read() error
# define IS_EOF IS_EOF
} msh_attr_pack input_state_t;

#define INPUT_BUFF_SIZE 8096

typedef handler_state_t	(*input_handler_t)(void);

static char	g_ch[4] = { 0 };
static input_state_t	g_is_state = 0;
static dll_t *restrict	g_tokens = NULL;
static struct tk_key *restrict	g_lkey = NULL;

static char	g_buff[INPUT_BUFF_SIZE] = { 0 };
static size_t	g_ibuff = 0;
static size_t	g_ilword = 0;

static inline void	refresh_global_input_data(void) {
	bzero(g_buff, g_ibuff);
	g_lkey = NULL;
	g_is_state = g_ibuff = g_ilword = 0;
	if (g_tokens) {
		while (dll_popfront(g_tokens))
			;
		NULL;
	}
}

# define get_key(_obj) ((struct tk_key*)dll_getdata(_obj))

# define KEY_DEL 0x7f
# define KEY_ESC 0x1b
# define KEY_CTRL(k) ((k) & 0x1f)

static inline handler_state_t	__ispace(void);
static inline handler_state_t	__inew_line(void);
static inline handler_state_t	__imulticmd(void);
static inline handler_state_t	__iredir(void);
static inline handler_state_t	__ipipe(void);
static inline handler_state_t	__iprintable(void);
static inline handler_state_t	__ihome_path(void);
static inline handler_state_t	__idelch(void);

static inline handler_state_t	__ictrl_cd(void);
static inline handler_state_t	__ictrl_l(void);
static inline handler_state_t	__ictrl_q(void);

static inline handler_state_t	__iseq(void);

static inline handler_state_t	__ieof(void);

// IHLT - Input Handlers Lookup Table
static const input_handler_t *restrict	__ihlt[] = {
	[IS_CHAR] = (input_handler_t[]) {
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
	[IS_CTRL] = (input_handler_t[]) {
		[KEY_CTRL('C') ... KEY_CTRL('D')] = __ictrl_cd, // Ctrl+C, Ctrl+D
		[KEY_CTRL('J')] = __inew_line, // Ctrl+J
		[KEY_CTRL('L')] = __ictrl_l,   // Ctrl+L
		[KEY_CTRL('Q')] = __ictrl_q,   // Ctrl+Q
		[KEY_DEL] = __idelch // DEL
	},
	[IS_SEQ] = (input_handler_t[]) { [0 ... 127] = __iseq },
	[IS_EOF] = (input_handler_t[]) { [0 ... 127] = __ieof }
};

#endif /* MSH_INPUT_DATA_H */
