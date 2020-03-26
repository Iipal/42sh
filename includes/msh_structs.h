#ifndef MSH_STRUCTS_H
# define MSH_STRUCTS_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
# endif

struct command {
	char	**argv;
	size_t	argc;
};

#endif /* MSH_STRUCTS_H */
