#ifndef MSH_DBG_INFO_H
# define MSH_DBG_INFO_H

# ifndef MINISHELL_H
#  error "include only minishell.h"
# endif

# include <stdio.h>
# include <stdarg.h>

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
	if (g_opt_dbg_level) { \
		__dbg_lvl_callback[g_opt_dbg_level](fmt, __VA_ARGS__); \
	} \
})

#endif /* MSH_DBG_INFO_H */
