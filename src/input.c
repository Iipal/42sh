#include "minishell.h"

char	*cmd_readline(void) {
	static char	buff[BUFSIZ] = { 0 };
	int	ch = 0;
	size_t	ibuff = 0;

	while (EOF != (ch = getchar())) {
		if ('\n' == ch)
			break ;
		switch (ch) {
			case '\t':
			case '\v':
			case '\f':
			case '\r':
			case ' ': {
				if (ibuff && ' ' != buff[ibuff - 1])
					buff[ibuff++] = ' ';
				break ;
			}
			case '|': {
				g_is_cq_piped = true;
				if (ibuff && ' ' != buff[ibuff - 1])
					buff[ibuff++] = ' ';
				buff[ibuff++] = ch;
				++g_cq_len;
				break ;
			}
			default: buff[ibuff++] = ch; break ;
		}
		buff[ibuff] = 0;
	}
	if (-1 == ch)
		return (char*)-1;
	char	*out;
	assert(out = strndup(buff, ibuff));
	return out;
}
