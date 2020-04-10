#include "minishell.h"

dll_t *restrict	g_history = NULL;
dll_obj_t *restrict g_history_last = NULL;

int	main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	parse_options(argc, argv);
	read_history();
	init_sig_handlers();
	shell();
	cmd_fast_builtinrun(CMD_FAST_NEW(1, "exit", NULL));
}
