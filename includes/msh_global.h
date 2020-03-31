#ifndef MSH_GLOBAL_H
# define MSH_GLOBAL_H

// Non-zero value if -d(--debug) option was detected
extern int	g_opt_dbg_level;
# include "msh_dbg_info.h"

// Non-zero value if -h(--help) option was detected
extern int	g_opt_help;

// Store globally pid of each single child for handling SIGCHLD signal
extern pid_t	g_child;

#endif /* MSH_GLOBAL_H */