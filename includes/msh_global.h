#ifndef MSH_GLOBAL_H
# define MSH_GLOBAL_H

// Non-zero value if -d(--debug) option was detected
extern int	g_opt_dbg_level;

// Non-zero value if -h(--help) option was detected
extern int	g_opt_help;

// Store globally pid of each single child for handling SIGCHLD signal
extern pid_t	g_child;

// Current session commands history
extern dll_t *restrict	g_history;

# define MAX_HISTORY_SIZE 50

#endif /* MSH_GLOBAL_H */
