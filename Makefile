all:
	gcc -Wall -Wextra -Werror -Ofast -std=c11 shell.c -o shell

debug:
	gcc -Wall -Wextra -Werror -g -std=c11 shell.c -o shell
