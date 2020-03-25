#ifndef MSH_BUILTINS_H
# define MSH_BUILTINS_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
# endif

# include <stdbool.h>
# include <pwd.h>

# include "msh_structs.h"

/*
**	Check is current \param cmd is a builtin and run
*/
bool	cmd_builtinrun(const struct command *restrict cmd);

#endif /* MSH_BUILTINS_H */
