#ifndef MSH_CMD_H
# define MSH_CMD_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
# endif

# include <stddef.h>
# include "msh_attr.h"

typedef enum e_command_queue_type {
	CQ_DEFAULT,
# define CQ_DEFAULT CQ_DEFAULT
	CQ_PIPE,
# define CQ_PIPE CQ_PIPE
} msh_attr_pack cq_type_t;

struct command {
	char	**argv;
	size_t	argc;
} msh_attr_align;

struct command_queue {
	struct command *restrict *restrict	cmd;
	size_t	size;
	cq_type_t	type;
} msh_attr_align;

# define CMD_FAST_NEW(ac, cmd, ...) \
	(struct command) { \
		.argv = (char *[]){ cmd, __VA_ARGS__ }, \
		.argc = (ac) \
	}

#endif /* MSH_CMD_H */
