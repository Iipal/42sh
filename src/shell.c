#include "minishell.h"

void	cq_free(struct command_queue *restrict cq) {
	for (size_t i = 0; cq->size > i && cq->cmd[i]; ++i) {
		if (cq->cmd[i]->argv) {
			for (size_t j = 0; cq->cmd[i]->argc >= j; j++)
				free(cq->cmd[i]->argv[j]);
			free(cq->cmd[i]->argv);
		}
		free(cq->cmd[i]);
	}
	free(cq);
}

void	shell(void) {
	char *restrict	line = NULL;
	while (1) {
		g_child = 0;
		printf("$> ");
		line = input_read();

		if (INPUT_EOF == line) {
			rewind(stdin);
			puts("");
			continue ;
		} else if (INPUT_EXIT == line) {
			break ;
		} else if (INPUT_CONTINUE == line) {
			continue ;
		}
		free(line);
	}
}
