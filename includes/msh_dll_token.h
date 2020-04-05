#ifndef MSH_DLL_TOKEN_H
# define MSH_DLL_TOKEN_H

# include <stddef.h>
# include <stdbool.h>

# include "libdll.h"
# include "msh_attr.h"

typedef enum e_token_type {
	TK_EXEC, // first token for each command
	TK_OPT, // all tokens started with '-' after TK_EXEC
	TK_ARG, // all tokens after TK_OPT or TK_ENV_VAR
	TK_PIPE, // pipes '|', separate commands
	TK_REDIR, // redirection output '>'
	TK_REDIR_APP, // redirection output with appending ">>"
	TK_REDIR_DST, // mark next token after TK_REDIR or TK_REDIR_APP
	TK_MULTI_CMD, // ';', separate commands
	TK_ENV_VAR // environment variable '$'
} msh_attr_pack tk_type_t;

struct tk_key {
	char *restrict	str;
	size_t	len;
	tk_type_t	type;
} msh_attr_align;

#endif /* MSH_DLL_TOKEN_H */
