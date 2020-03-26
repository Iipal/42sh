#ifndef MINISHELL_H
# define MINISHELL_H

# define _GNU_SOURCE

# include <stdlib.h>
# include <stddef.h>
# include <stdint.h>
# include <string.h>
# include <getopt.h>
# include <assert.h>
# include <errno.h>
# include <err.h>
# include <ctype.h>
# include <unistd.h>
# include <fcntl.h>
# include <signal.h>
# include <sys/wait.h>
# include <sys/types.h>

# define EXEC_ERR_FMTMSG "'%s': command not found..."

# include "msh_builtins.h"

extern int	g_opt_dbg_level;
# include "msh_dbg_info.h"

extern pid_t	g_child;

extern bool	g_is_cq_piped;

extern int	g_opt_stdout_redir;

extern FILE	*g_defout;

/*
**	Initialize signal handlers
*/
void	init_sig_handlers(void);

#endif /* MINISHELL_H */
