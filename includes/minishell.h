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

# include "libdll.h"

# include "msh_global.h"
# include "msh_cmd.h"
# include "msh_builtins.h"
# include "msh_dll_token.h"

void	parse_options(int ac, char *const *av);

void	init_sig_handlers(void);

void	shell(void);

# define INPUT_EOF      ((dll_t*)-1)
# define INPUT_EXIT     ((dll_t*)-2)
# define INPUT_CONTINUE NULL

dll_t	*input_read(void);

void	cmd_run(struct command_queue *restrict cq);
void	cmd_solorun(const struct command *restrict cmd);
void	cmd_pipe_queuing(const ssize_t isender,
				const ssize_t ireceiver,
				struct command *restrict *restrict cq);

#endif /* MINISHELL_H */
