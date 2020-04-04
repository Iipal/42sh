#include "minishell.h"

dll_t *restrict	g_session_history = NULL;
dll_obj_t *restrict g_history_last = NULL;

int	main(int argc, char *argv[]) {
	read_history();
	setbuf(stdout, NULL);
	parse_options(argc, argv);
	init_sig_handlers();
	shell();
	cmd_fast_builtinrun(CMD_FAST_NEW(1, "exit", NULL));
}
