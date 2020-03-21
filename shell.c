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
#include <getopt.h>

# define EXEC_ERR_FMTMSG "can not run '%s'"

typedef struct {
	char	**argv;
	int	argc;
} command_t;

# define CMD_INIT { .argv = NULL, .argc = 0 }

static inline command_t	*cmddupp(const command_t *restrict src) {
	return memcpy(calloc(1, sizeof(*src)), src, sizeof(*src));
}

static pid_t	child;

static command_t	**g_cq = { NULL }; // g_cq - commands queue
static size_t	g_cq_size = 0;

static int	dbg_level = 0;
static int	stdout_tofile = 0;

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

static inline void
pipe_redir(int from, int to, int trash) {
	dup2(from, to);
	close(from);
	if (trash != -1)
		close(trash);
}

static void	cmd_pipe_queuing(const ssize_t isender, const ssize_t ireceiver) {
	if (-1 >= isender)
		return ;

	int	fds[2] = { 0 };

	assert(-1 != pipe(fds));
	assert(-1 != (child = fork()));

	if (!child) {
		__dbg_lvl_callback[dbg_level](
			"child  | %d(%d) '%s' wait for input from '%s'\n",
			getpid(), getppid(),
			g_cq[ireceiver]->argv[0], g_cq[isender]->argv[0]);
		pipe_redir(fds[1], STDOUT_FILENO, fds[0]);
		cmd_pipe_queuing(isender - 1, isender);
		__dbg_lvl_callback[dbg_level]("child  | %d(%d) '%s' created\n",
			getpid(), getppid(), g_cq[isender]->argv[0]);
		execvp(g_cq[isender]->argv[0], g_cq[isender]->argv);
		err(EXIT_FAILURE, EXEC_ERR_FMTMSG, g_cq[isender]->argv[0]);
	} else {
		__dbg_lvl_callback[dbg_level]("parent | %d(%d) '%s'\n",
			getpid(), getppid(), g_cq[ireceiver]->argv[0]);
		pipe_redir(fds[0], STDIN_FILENO, fds[1]);
		execvp(g_cq[ireceiver]->argv[0], g_cq[ireceiver]->argv);
		err(EXIT_FAILURE, EXEC_ERR_FMTMSG, g_cq[ireceiver]->argv[0]);
	}
}

static inline char	*cmd_readline(void) {
	char	*out = NULL;
	size_t	n = 0;
	ssize_t	nb = getline(&out, &n, stdin);

	if (!nb || !out)
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
		if (!isspace(copy[i]) || (0 < i && !isspace(copy[i - 1])))
			copy[n++] = copy[i];
	copy[n] = 0;
	return strndup(copy, n);
}

static inline void	add_redir_tofile(const char *path) {
	int	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	if (-1 == fd) {
		err(EXIT_FAILURE, "open(%s)", path);
	}
	dup2(fd, STDOUT_FILENO);
	close(fd);
}

static void	wait_child(int s) {
	__dbg_lvl_callback[dbg_level]("waiting for end of: %d\n", child);
	if (1 > child) // -> (0 == child || -1 == child)
		wait(&s);
	else
		waitpid(child, &s, WUNTRACED | WNOHANG);
}

static inline void	init_sigchld_handler(void) {
	struct sigaction	sa;
	bzero(&sa, sizeof(sa));
	sa.sa_flags = SA_RESTART | SA_NODEFER;
	sa.sa_handler = wait_child;

	if (-1 == sigaction(SIGCHLD, &sa, NULL))
		err(EXIT_FAILURE, "sigaction");
}

static inline void	parse_opt(int ac, char *const *av) {
	static const struct option	l_opts[] = {
		{ "debug", no_argument, &dbg_level    , 1 },
		{ "file" , no_argument, &stdout_tofile, 1 },
		{ 0      , 0          , 0             , 0 }
	};

	int	opt;
	while (-1 != (opt = getopt_long(ac, av, "df", l_opts, NULL))) {
		switch (opt) {
			case 'd': dbg_level      = 1; break ;
			case 'f': stdout_tofile  = 1; break ;
			case '?': exit(EXIT_FAILURE); break ;
			default :                     break ;
		}
	}
}

static inline void	cmd_separate_av_cmd(command_t *restrict cmd,
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
		for (int i = 1; delim && cmd->argc > i; i++) {
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
}

static inline void	cmd_solorun(const command_t *restrict cmd) {
	if (!(child = fork())) {
		__dbg_lvl_callback[dbg_level]("child  | %d(%d) '%s'\n",
			getpid(), getppid(), cmd->argv[0]);
		execvp(cmd->argv[0], cmd->argv);
		err(EXIT_FAILURE, EXEC_ERR_FMTMSG, cmd->argv[0]);
	} else {
		__dbg_lvl_callback[dbg_level]("parent | %d(%d)\n",
			getpid(), getppid());
		pause();
	}
}

static inline void	cmd_parseline(char *restrict line) {
	command_t	curr_cmd = CMD_INIT;
	char *restrict	token = strtok(line, "|");
	char *restrict	trimed = NULL;
	size_t	i = 0;

	while (token) {
		trimed = trim_extra_ws(token);
		if (!trimed || !*trimed)
			break ;
		cmd_separate_av_cmd(&curr_cmd, trimed);
		g_cq[i++] = cmddupp(&curr_cmd);
		free(trimed);
		token = strtok(NULL, "|");
	}
	if (1 < g_cq_size) {
		if (!(child = fork()))
			cmd_pipe_queuing(g_cq_size - 2, g_cq_size - 1);
		else if (child)
			pause();
	} else {
		cmd_solorun(g_cq[0]);
	}
}

int	main(int argc, char *argv[]) {
	parse_opt(argc, argv);

	__dbg_lvl_callback[dbg_level]("parent: %d\n", getpid());

	init_sigchld_handler();
	if (stdout_tofile) {
		add_redir_tofile("./result.out");
	}

	while (1) {
		fprintf(stderr, "$> ");
		char	*line = cmd_readline();

		if (!line)
			continue ;
		if (!strcmp(line, "q"))
			break ;

		g_cq_size = precalc_cq_size(line);
		if (!(g_cq = calloc(g_cq_size + 1, sizeof(*g_cq))))
			continue ;
		cmd_parseline(line);
	}
}
