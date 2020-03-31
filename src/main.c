#include "minishell.h"

pid_t	g_child = 0;
int	g_opt_dbg_level = 0;
int	g_opt_help = 0;

int	main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	parse_options(argc, argv);
	init_sig_handlers();
	shell();
}
