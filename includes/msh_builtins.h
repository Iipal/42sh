#ifndef MSH_BUILTINS_H
# define MSH_BUILTINS_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
# endif

# include <stdbool.h>
# include <pwd.h>

# include "msh_cmd.h"

/*
**	Check is current \param cmd is a builtin and run
*/
bool	cmd_builtinrun(const struct command *restrict cmd, cq_type_t cq_type);
bool	cmd_fast_builtinrun(const struct command cmd);
bool	cmd_fast_tbuiltinrun(const struct command cmd, cq_type_t cq_type);

#endif /* MSH_BUILTINS_H */
