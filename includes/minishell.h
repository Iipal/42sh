#ifndef MINISHELL_H
# define MINISHELL_H

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <err.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <getopt.h>

static pid_t	child;

# define EXEC_ERR_FMTMSG "can not run '%s'"

/*
** ----------
** command_t:
** ----------
*/
typedef struct {
	char	**argv;
	int	argc;
} command_t;

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
