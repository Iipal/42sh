#include "minishell.h"

static char	*line_ws_trimm(const char *restrict src) {
	size_t	start = 0;
	size_t	end = strlen(src);

	while (src[start] && isspace(src[start]))
		++start;
	while (start < end && isspace(src[end - 1]))
		--end;
	if (!src[start] || start == end)
		return NULL;

	char	*out = NULL;
	char	*acopy = strndupa(src + start, end - start);

	size_t	n = 1;
	for (size_t i = 1; acopy[i]; ++i)
		if (!isspace(acopy[i]) || !isspace(acopy[i - 1]))
			acopy[n++] = acopy[i];
	acopy[n] = '\0';
	assert(out = strndup(acopy, n));
	return out;
}

static char	*line_space_sep(const char *restrict str, int sep) {
	char *restrict	out;
	char *restrict	sep_ptr;

	assert(out = strdup(str));
	sep_ptr = strchr(str, sep);
	while (sep_ptr) {
		if (sep_ptr > str && !isspace(sep_ptr[-1])) {
			ptrdiff_t	sep_len = strstr(out, sep_ptr) - out;
			assert(out = realloc(out, strlen(out) + 1));
			out[sep_len++] = ' ';
			out[sep_len++] = sep;
			strcpy(out + sep_len, sep_ptr + 1);
		}
		sep_ptr = strchr(sep_ptr + 1, sep);
	}
	return out;
}

char	*line_prepare(const char *restrict line) {
	char *restrict	line_no_ws;
	char *restrict	line_seps;

	if (!(line_no_ws = line_ws_trimm(line)))
		return NULL;
	line_seps = line_space_sep(line_no_ws, '|');

	free(line_no_ws);
	return line_seps;
}

inline bool	cmd_parseline(char *restrict line,
				struct command *restrict *restrict cq) {
	struct command *restrict	c = NULL;
	size_t	cq_iter = 0;
	char	*save = NULL;
	char	*token = NULL;
	char	*cmd_line = NULL;

	if (!(cmd_line = line_prepare(line)))
		return false;
	token = strtok_r(cmd_line, " |", &save);
	while (token) {
		if (!cq[cq_iter])
			assert(c = cq[cq_iter] = calloc(1, sizeof(*c)));
		assert(c->argv = realloc(c->argv, sizeof(*(c->argv)) * (c->argc + 2)));
		assert(c->argv[c->argc++] = strdup(token));
		c->argv[c->argc] = NULL;
		if ('|' == *save)
			++cq_iter;
		token = strtok_r(NULL, " |", &save);
	}
	free(cmd_line);
	return true;
}
