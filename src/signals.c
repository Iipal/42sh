#include "minishell.h"

static void	sigchld_handler(int sig) {
	DBG_INFO("wait   | %d | %d(%s)\n", child, sig, strsignal(sig));

	int	dummy = 0;
	if (1 > child)
		wait(&dummy);
	else
		waitpid(child, &dummy, WUNTRACED | WNOHANG);
}

static inline void	sig_set_handler(int __sig, int __flags, sighandler_t __h) {
	struct sigaction	sa;
	bzero(&sa, sizeof(sa));

	sa.sa_flags = __flags;
	sa.sa_handler = __h;

	if (-1 == sigaction(__sig, &sa, NULL))
		err(EXIT_FAILURE, "sigaction");
}

void	init_sig_handlers(void) {
	sig_set_handler(SIGCHLD, SA_RESTART | SA_NODEFER, sigchld_handler);
}
