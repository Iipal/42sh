#include "minishell.h"

pid_t	g_child = 0;
bool	g_is_cq_piped = false;
int	g_opt_dbg_level = 0;
int	g_opt_stdout_redir = 0;
int	g_opt_help = 0;
FILE	*g_defout = NULL;
struct command *restrict *restrict	g_cq = NULL;
size_t	g_cq_len = 0;

int	main(int argc, char *argv[]) {
	g_defout = stdout;
	setbuf(stdout, NULL);
	parse_options(argc, argv);
	init_sig_handlers();
	shell();
}
