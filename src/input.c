#include "minishell.h"

inline char	*cmd_readline(void) {
	char	*out = NULL;
	size_t	n = 0;
	ssize_t	nb = getline(&out, &n, stdin);

	if (1 > nb)
		return (char*)-1;
	if (out)
		*((short*)(out + nb - 1)) = 0;
	return out;
}
