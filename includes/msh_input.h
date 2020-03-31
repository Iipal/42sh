#ifndef MSH_INPUT_H
# define MSH_INPUT_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
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

# define __S_INPUT_STATE_SEQ_MAX 4
# define __S_INPUT_STATE_ALIGN 3

struct s_input_state {
	char	ch[__S_INPUT_STATE_SEQ_MAX];
	input_state_t	state;
	char	__align[__S_INPUT_STATE_ALIGN] msh_attr_unused;
	dll_t	*tokens;
} msh_attr_align;

typedef handler_state_t	(*input_handler)(struct s_input_state *restrict is);

#endif /* MSH_INPUT_H */
