/*
	Reading and execution commands like this from code:
	`who | sort | uniq -c | sort -nk1`
*/

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <err.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>

typedef struct {
	char *restrict	command;
	char	**argv;
	size_t	argc;
} command_t;

# define EXEC_ERR_FMTMSG "can not run '%s'"

# define CMD_INIT { .command = 0, .argv = NULL, .argc = 0 }

static inline command_t	*cmddupp(const command_t *restrict src) {
	return memcpy(calloc(1, sizeof(*src)), src, sizeof(*src));
}

static pid_t	child;

static command_t	**cq = { NULL }; // cq - commands queue
static size_t		cq_size = 0;

static int	dbg_level = 0;

static inline void	__dbg_info_none(const char *restrict fmt, ...) {
	(void)fmt;
}

static inline void	__dbg_info_dflt(const char *restrict fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static void	(*__dbg_lvl_callback[2])(const char *restrict fmt, ...) = {
	__dbg_info_none, __dbg_info_dflt
};

static void	wait_child(int s) {
	__dbg_lvl_callback[dbg_level]("waiting for end of: %d\n", child);
	if (-1 == child)
		wait(&s);
	else
		waitpid(child, &s, WUNTRACED | WNOHANG);
}

static inline void	run_command(const command_t *restrict cmd) {
	if (!(child = fork())) {
		__dbg_lvl_callback[dbg_level]("child  | %d(%d) '%s'\n",
							getpid(), getppid(), cmd->command);
		if (-1 == execvp(cmd->command, cmd->argv))
			err(EXIT_FAILURE, EXEC_ERR_FMTMSG, cmd->command);
	} else {
		__dbg_lvl_callback[dbg_level]("parent | %d(%d)\n", getpid(), getppid());
		pause();
	}
}

static inline void
pipe_redir(int from, int to, int trash) {
	dup2(from, to);
	close(from);
	if (trash != -1)
		close(trash);
}

static void	pipe_queuing(const ssize_t isender, const ssize_t ireceiver) {
	if (-1 >= isender)
		return ;

	int	fds[2] = { 0 };

	assert(-1 != pipe(fds));
	assert(-1 != (child = fork()));

	if (!child) {
		__dbg_lvl_callback[dbg_level](
			"child  | %d(%d) '%s' wait for input from '%s'\n",
			getpid(), getppid(), cq[ireceiver]->command, cq[isender]->command);
		pipe_redir(fds[1], STDOUT_FILENO, fds[0]);
		pipe_queuing(isender - 1, isender);
		__dbg_lvl_callback[dbg_level]("child  | %d(%d) '%s' created\n",
			getpid(), getppid(), cq[isender]->command);
		if (-1 == execvp(cq[isender]->command, cq[isender]->argv))
			err(EXIT_FAILURE, EXEC_ERR_FMTMSG, cq[isender]->command);
	} else {
		__dbg_lvl_callback[dbg_level]("parent | %d(%d) '%s'\n",
			getpid(), getppid(), cq[ireceiver]->command);
		pipe_redir(fds[0], STDIN_FILENO, fds[1]);
		if (-1 == execvp(cq[ireceiver]->command, cq[ireceiver]->argv))
			err(EXIT_FAILURE, EXEC_ERR_FMTMSG, cq[ireceiver]->command);
	}
}

static inline char	*cmd_read(void) {
	char	*out = NULL;
	size_t	n = 0;
	ssize_t	nb = getline(&out, &n, stdin);

	if (!nb)
		return NULL;
	*((short*)(out + nb - 1)) = 0;
	return out;
}

static inline size_t	precalc_cq_size(char *restrict line) {
	size_t	n = 1;
	char	*l = strchr(line, '|');

	while (l) {
		l = strchr(l + 1, '|');
		++n;
	}
	return n;
}

static char	*trim_extra_ws(const char *restrict src) {
	char	*copy = strdupa(src);
	size_t	n = 0;
	size_t	start = 0;
	size_t	end = strlen(src);

	while (copy[start] && isspace(start))
		++start;
	while (start < end && isspace(copy[end - 1]))
		--end;
	if (!copy[start] || start == end)
		return NULL;

	for (size_t i = start; end > i && copy[i]; ++i)
		if (!isspace(copy[i]) || (i > 0 && !isspace(copy[i - 1])))
			copy[n++] = copy[i];
	copy[n] = 0;
	return strndup(copy, n);
}

static inline void	parse_cmd(command_t *restrict cmd,
							const char *restrict line) {
	char	*delim = strchr(line, ' ');

	cmd->argc = 1;
	while (delim) {
		++cmd->argc;
		delim = strchr(delim + 1, ' ');
	}
	cmd->argv = calloc(cmd->argc + 1, sizeof(*(cmd->argv)));
	if (1 == cmd->argc) {
		cmd->argv[0] = strdup(line);
	} else {
		delim = strchr(line, ' ');
		cmd->argv[0] = strndup(line, delim - line);
		for (size_t	i = 1; delim && cmd->argc > i; i++) {
			char	*endptr = strchr(++delim, ' ');
			size_t	duplen = 0;
			if (!endptr) {
				duplen = strlen(delim);
			} else {
				duplen = endptr - delim;
			}
			cmd->argv[i] = strndup(delim, duplen);
			delim = endptr;
		}
	}
	cmd->command = cmd->argv[0];
}

static inline void	add_redir_tofile(const char *path) {
	int	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (-1 == fd)
		err(EXIT_FAILURE, "%s", path);
	dup2(fd, STDOUT_FILENO);
	close(fd);
}

static inline void	init_sigchld_handler(void) {
	struct sigaction	sa;
	bzero(&sa, sizeof(sa));
	sa.sa_flags = SA_RESTART | SA_NODEFER;
	sa.sa_handler = wait_child;

	if (-1 == sigaction(SIGCHLD, &sa, NULL))
		err(EXIT_FAILURE, "sigaction");
}

int	main(int argc, char *argv[]) {
	if (argc >= 2) {
		if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--debug"))
			dbg_level = 1;
	}

	__dbg_lvl_callback[dbg_level]("parent: %d\n", getpid());

	add_redir_tofile("/home/ipaldesse/result.out");
	init_sigchld_handler();

	while (1) {
		fprintf(stderr, "$> ");
		char	*line = cmd_read();

		if (!line)
			continue ;
		if (!strcmp(line, "q"))
			break ;

		cq_size = precalc_cq_size(line);
		if (!(cq = calloc(cq_size + 1, sizeof(*cq))))
			continue ;

		command_t	curr_cmd = CMD_INIT;
		char	*token = strtok(line, "|");
		char	*trimed = NULL;
		size_t	i = 0;

		while (token) {
			trimed = trim_extra_ws(token);
			if (!trimed || !*trimed)
				break ;
			parse_cmd(&curr_cmd, trimed);
			cq[i++] = cmddupp(&curr_cmd);
			free(trimed);
			token = strtok(NULL, "|");
		}
		if (cq_size > 1) {
			if (!(child = fork())) {
				pipe_queuing(cq_size - 2, cq_size - 1);
			} else {
				pause();
			}
		} else {
			run_command(cq[0]);
		}
	}
}
