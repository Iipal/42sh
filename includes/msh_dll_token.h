#ifndef MSH_DLL_TOKEN_H
# define MSH_DLL_TOKEN_H

# include <stddef.h>
# include <stdbool.h>

# include "libdll.h"
# include "msh_attr.h"

# define TK_DLL_BITS DLL_BIT_EIGN | DLL_BIT_DUP

typedef enum e_token_type {
	TK_EXEC, // first token for each command always must to be executable and markes as TK_EXEC
# define TK_EXEC TK_EXEC
	TK_OPT, // all tokens started with '-' after TK_EXEC
# define TK_OPT TK_OPT
	TK_ARG, // all tokens after TK_OPT
# define TK_ARG TK_ARG
	TK_PIPE, // mark token contain '|'
# define TK_PIPE TK_PIPE
	TK_REDIR, // mark token contain '>'
# define TK_REDIR TK_REDIR
	TK_REDIR_APP, // mark token contain ">>"
# define TK_REDIR_APP TK_REDIR_APP
	TK_REDIR_DST, // mark next token after TK_REDIR or TK_REDIR_APP
# define TK_REDIR_DST TK_REDIR_DST
	TK_MULTI_CMD // mark token contaion ';'
# define TK_MULTI_CMD TK_MULTI_CMD
} msh_attr_pack tk_type_t;

struct s_token_key {
	char *restrict	str;
	size_t	len;
	tk_type_t	type;
} msh_attr_align;

#endif /* MSH_DLL_TOKEN_H */
