#include "minishell.h"

// run.c
extern pid_t	g_child;

static void	sigchld_handler(int sig) {
	DBG_INFO("SIGCHLD | %d | %d(%s)\n", g_child, sig, strsignal(sig));
	waitpid(g_child, NULL, WUNTRACED | WNOHANG);
}

static void	sigint_handler(int sig) {
	DBG_INFO("SIGINT | %d | %d(%s)\n", g_child, sig, strsignal(sig));
}

static inline void	sig_set_handler(int __sig, int __flags,
									sighandler_t __handler) {
	struct sigaction	sa;
	bzero(&sa, sizeof(sa));

	sa.sa_flags = __flags;
	sa.sa_handler = __handler;

	if (-1 == sigaction(__sig, &sa, NULL))
		err(EXIT_FAILURE, "sigaction");
}

void	init_sig_handlers(void) {
	sig_set_handler(SIGCHLD, SA_RESTART | SA_NODEFER, sigchld_handler);
	sig_set_handler(SIGINT , SA_RESTART | SA_NODEFER, sigint_handler);
}
