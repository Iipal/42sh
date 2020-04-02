#include "minishell.h"

int	main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	parse_options(argc, argv);
	init_sig_handlers();
	shell();
}
