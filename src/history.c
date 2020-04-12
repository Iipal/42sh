#include "minishell.h"

void	add_to_history(char *cmd_line, size_t line_len) {
	if (!line_len)
		return ;

	while (MAX_HISTORY_SIZE <= dll_getsize(g_history))
		dll_popfront(g_history);
	dll_pushback(g_history, cmd_line, line_len, DLL_BIT_FREE | DLL_BIT_EIGN, NULL);
}

static inline char	*get_history_file_path(void) {
	char	*history_file_path;
	assert_perror(0 >= asprintf(&history_file_path, "%s/%s",
				getpwuid(getuid())->pw_dir, ".msh_history"));
	return history_file_path;
}

void	save_history(void) {
	char *restrict	dst_path = get_history_file_path();
	FILE *restrict	file = fopen(dst_path, "w+");
	free(dst_path);
	if (!file) {
		perror("fopen(./.msh_history)");
		return ;
	}

	dll_obj_t *restrict	obj = dll_gethead(g_history);
	size_t	history_size = dll_getsize(g_history);
	while (obj) {
		fwrite(dll_getdata(obj), dll_getdatasize(obj), 1, file);
		fwrite("\n", 1, 1, file);
		dll_popfront(g_history);
		obj = dll_gethead(g_history);
	}
	fclose(file);
	DBG_INFO("Saved %zu history elements\n", history_size);
}

void	read_history(void) {
	dll_assert(g_history = dll_init(DLL_BIT_EIGN));
	char *restrict	dst_path = get_history_file_path();

	FILE *restrict	file = fopen(dst_path, "r");
	free(dst_path);
	if (!file)
		return ;

	char	*str = NULL;
	size_t	__dummy_nb = 0;
	ssize_t nb;

	while (EOF != (nb = getline(&str, &__dummy_nb, file))) {
		if (!nb)
			continue ;
		if (MAX_HISTORY_SIZE <= dll_getsize(g_history))
			dll_popfront(g_history);

		--nb;
		dll_pushback(g_history, strndup(str, nb), nb, DLL_BIT_FREE | DLL_BIT_EIGN, NULL);

		free(str);
		str = NULL;
		__dummy_nb = 0;
	}
	if (str)
		free(str);
	fclose(file);
	DBG_INFO("Readed %zu history elements\n", dll_getsize(g_history));
}
