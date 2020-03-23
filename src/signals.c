#include "minishell.h"

static void	sigchld_handler(int sig) {
	DBG_INFO("wait   | %d | %d(%s)\n", child, sig, strsignal(sig));

	int	dummy = 0;
	if (1 > child)
		wait(&dummy);
	else
		waitpid(child, &dummy, WUNTRACED | WNOHANG);
}

static inline void	sig_set_handler(int __sig, sighandler_t __handler) {
	struct sigaction	sa;
	bzero(&sa, sizeof(sa));

	sa.sa_flags = SA_RESTART | SA_NODEFER;
	sa.sa_handler = __handler;

	if (-1 == sigaction(__sig, &sa, NULL))
		err(EXIT_FAILURE, "sigaction");
}

void	init_sig_handlers(void) {
	sig_set_handler(SIGCHLD, sigchld_handler);
}
