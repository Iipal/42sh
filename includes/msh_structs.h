#ifndef MSH_STRUCTS_H
# define MSH_STRUCTS_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
# endif

struct command {
	char	**argv;
	size_t	argc;
};

# define CMD_FAST_NEW(ac, cmd, ...) \
	(struct command) { \
		.argv = (char *[]){ cmd, __VA_ARGS__ }, \
		.argc = (ac) \
	}

#endif /* MSH_STRUCTS_H */
