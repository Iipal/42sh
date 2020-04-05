#include "minishell.h"

static inline char	*get_dst_path(void) {
	const char	dest[] = "/.msh_history";
	char *restrict	home = getpwuid(getuid())->pw_dir;
	size_t	home_len = strlen(home);
	char *restrict	dst_path = NULL;

	assert(dst_path = calloc(home_len + sizeof(dest), 1));
	strcpy(dst_path, home);
	strcpy(dst_path + home_len, dest);
	return dst_path;
}

void	save_history(void) {
	char *restrict	dst_path = get_dst_path();
	FILE *restrict	file = fopen(dst_path, "ab+");
	free(dst_path);
	if (!file) {
		perror("fopen(./.msh_history)");
		return ;
	}

	dll_obj_t *restrict	obj = dll_gethead(g_session_history);
	while (obj) {
		fwrite(dll_getdata(obj), dll_getdatasize(obj), 1, file);
		fwrite("\n", 1, 1, file);
		dll_popfront(g_session_history);
		obj = dll_gethead(g_session_history);
	}
	fclose(file);
}

void	read_history(void) {
	dll_assert(g_session_history = dll_init(DLL_BIT_EIGN));
	char *restrict	dst_path = get_dst_path();

	FILE *restrict	file = fopen(dst_path, "r");
	free(dst_path);
	if (!file)
		return ;

	char	*str = NULL;
	size_t	__dummy_nb = 0;
	ssize_t nb = getline(&str, &__dummy_nb, file);

	while (EOF != nb || 0 < nb) {
		--nb;
		dll_pushback(g_session_history, strndup(str, nb), nb,
			DLL_BIT_FREE | DLL_BIT_EIGN, NULL);
		str = NULL;
		__dummy_nb = 0;
		nb = getline(&str, &__dummy_nb, file);
	}
	fclose(file);
}
