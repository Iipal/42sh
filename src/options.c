#include "minishell.h"

void	parse_options(int ac, char *const *av) {
	static struct option	l_opts[] = {
		{ "debug", no_argument, &g_opt_dbg_level   , 1 },
		{ "help" , no_argument, &g_opt_help        , 1 },
		{ 0      , 0          , 0                  , 0 }
	};

	int	opt;
	while (-1 != (opt = getopt_long(ac, av, "dfh", l_opts, NULL))) {
		switch (opt) {
			case 'd': g_opt_dbg_level = 1;    break ;
			case 'h': g_opt_help = 1;         break ;
			case '?': exit(EXIT_FAILURE);     break ;
			default : break ;
		}
	}
	if (g_opt_help) {
		printf("Mini-Shell help:\n"
"\t-d(--debug)\toutput additional debug information to stderr\n"
"\t-f(--file) \tre-direct stdout to ./.msh.out (not applicable to builtins)\n"
"\t-h(--help) \tprint this info message and exit\n");
		cmd_fast_builtinrun(CMD_FAST_NEW(1, "help", NULL));
		exit(EXIT_SUCCESS);
	}
}
