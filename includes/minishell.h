#ifndef MINISHELL_H
# define MINISHELL_H

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <errno.h>
#include <err.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

static pid_t	child;

# define EXEC_ERR_FMTMSG "'%s': command not found..."

struct command {
	char	**argv;
	int	argc;
};

/*
** --------
** options:
** --------
*/

static int	dbg_level = 0;
static int	stdout_tofile = 0;

/*
** ------
** flags:
** ------
*/
static int	is_pipe = 0;

/*
** -----------------
** debug trace info:
** -----------------
*/
static inline void	__dbg_info_none(const char *restrict fmt, ...) {
	(void)fmt;
}

static inline void	__dbg_info_dflt(const char *restrict fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void	(*__dbg_lvl_callback[2])(const char *restrict fmt, ...) = {
	__dbg_info_none, __dbg_info_dflt
};

# define DBG_INFO(fmt, ...) __extension__({ \
	if (dbg_level) { \
		__dbg_lvl_callback[dbg_level](fmt, __VA_ARGS__); \
	} \
})

#endif /* MINISHELL_H */
