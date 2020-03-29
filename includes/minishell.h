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
# include <termios.h>

# include "msh_builtins.h"

// Non-zero value if -d(--debug) option was detected
extern int	g_opt_dbg_level;
# include "msh_dbg_info.h"

// Non-zero value if -f(--file) option was detected
extern int	g_opt_stdout_redir;

// Non-zero value if -h(--help) option was detected
extern int	g_opt_help;

// Store globally pid of each single child for handling SIGCHLD signal
extern pid_t	g_child;

// True - if current commands queue is piped
extern bool	g_is_cq_piped;

// Setting-up default output stream if -f flag specified
extern FILE	*g_defout;

extern struct command *restrict *restrict	g_cq;
extern size_t	g_cq_len;

void	parse_options(int ac, char *const *av);

void	init_sig_handlers(void);

void	shell(void);

# define INPUT_EOF      ((char*)-1)
# define INPUT_EXIT     ((char*)-2)
# define INPUT_CONTINUE NULL

char	*input_read(void);

void	cmd_run(const size_t cq_length, struct command *restrict *restrict cq);
void	cmd_solorun(const struct command *restrict cmd);
void	cmd_pipe_queuing(const ssize_t isender,
				const ssize_t ireceiver,
				struct command *restrict *restrict cq);

#endif /* MINISHELL_H */
