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

# include "msh_builtins.h"

// Non-zero value if -d(--debug) option was detected
extern int	g_opt_dbg_level;
# include "msh_dbg_info.h"

// Store globally pid of each single child for handling SIGCHLD signal
extern pid_t	g_child;

// True - if current commands queue is piped
extern bool	g_is_cq_piped;

// Non-zero value if -f(--file) option was detected
extern int	g_opt_stdout_redir;

// Setting-up default output stream if -f flag specified
extern FILE	*g_defout;

// Initialize signal handlers
void	init_sig_handlers(void);

char	*cmd_readline(void);

bool	cmd_parseline(char *restrict line,
				struct command *restrict *restrict cq);
char	*line_prepare(const char *restrict line);

void	cmd_run(const size_t cq_length, struct command *restrict *restrict cq);
void	cmd_solorun(const struct command *restrict cmd);
void	cmd_pipe_queuing(const ssize_t isender,
				const ssize_t ireceiver,
				struct command *restrict *restrict cq);

#endif /* MINISHELL_H */
