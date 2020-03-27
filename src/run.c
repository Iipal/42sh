#include "minishell.h"

static inline void	pipe_redir(int from, int to, int trash) {
	dup2(from, to);
	close(from);
	close(trash);
}

void	cmd_pipe_queuing(const ssize_t isender,
				const ssize_t ireceiver,
				struct command *restrict *restrict cq) {
	if (-1 >= isender)
		return ;

	int	fds[2] = { 0, 0 };
	assert(-1 != pipe(fds));
	assert(-1 != (g_child = fork()));

	if (!g_child) {
		pipe_redir(fds[1], STDOUT_FILENO, fds[0]);
		cmd_pipe_queuing(isender - 1, isender, cq);
		DBG_INFO("child  | %d(%d) %s created\n",
			getpid(), getppid(), cq[isender]->argv[0]);
		execvp(cq[isender]->argv[0], cq[isender]->argv);
		errx(EXIT_FAILURE, "%s: command not found...", cq[isender]->argv[0]);
	} else if (0 < g_child) {
		DBG_INFO("child  | %d(%d) %s wait for input from %s\n",
			getpid(), getppid(),
			cq[ireceiver]->argv[0], cq[isender]->argv[0]);
		pipe_redir(fds[0], STDIN_FILENO, fds[1]);
		execvp(cq[ireceiver]->argv[0], cq[ireceiver]->argv);
		errx(EXIT_FAILURE, "%s: command not found...", cq[ireceiver]->argv[0]);
	}
}

inline void	cmd_solorun(const struct command *restrict cmd) {
	g_child = vfork();
	switch (g_child) {
		case  0:
			DBG_INFO("child  | %d(%d) %s\n", getpid(), getppid(), cmd->argv[0]);
			execvp(cmd->argv[0], cmd->argv);
		case -1:
			errx(EXIT_FAILURE, "%s: command not found...", cmd->argv[0]);
			break ;
		default: {
			DBG_INFO("wait   | %d\n", g_child);
			waitpid(g_child, NULL, WUNTRACED);
			break ;
		}
	}
}

void	cmd_run(const size_t cq_length, struct command *restrict *restrict cq) {
	for (size_t i = 0; cq_length > i; i++)
		if (cmd_builtinrun(cq[i]))
			return ;

	DBG_INFO("parent | %d(%d)\n", getpid(), getppid());
	if (1 < cq_length) {
		if (!(g_child = fork())) {
			cmd_pipe_queuing(cq_length - 2, cq_length - 1, cq);
		} else if (0 < g_child) {
			DBG_INFO("wait   | %d\n", g_child);
			waitpid(g_child, NULL, WUNTRACED);
		}
	} else {
		cmd_solorun(cq[0]);
	}
}
