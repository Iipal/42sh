#ifndef MSH_INPUT_TOKEN_H
# define MSH_INPUT_TOKEN_H

# include <stddef.h>
# include <stdbool.h>

# include "msh_attr.h"

typedef enum e_token_type {
	e_exec, // first token in always must to be executable and markes as e_exec
	e_opt, // all tokens started with '-' after e_exec
	e_arg, // all tokens after e_opt
	e_pipe, // mark token contain '|'
	e_redir, // mark token contain '>'
	e_redir_app, // mark token contain ">>"
	e_redir_to // mark next token after e_redir of e_redir_app
} msh_attr_pack token_t;

# if defined (__x86_64__)
#  define __S_TOKEN_KEY_ALIGN 7
# elif defined (__i386__)
#  define __S_TOKEN_KEY_ALIGN 3
# else
#  define __S_TOKEN_KEY_ALIGN 1
# endif

struct s_token_key {
	char *restrict	tok;
	size_t	len;
	token_t	type;
	char	__align[__S_TOKEN_KEY_ALIGN] msh_attr_unused;
} msh_attr_align;

struct s_input_token {
	struct s_input_token *restrict	next;
	struct s_input_token *restrict	prev;
	struct s_token_key	key;
} msh_attr_align;

struct s_input_data {
	struct s_input_token *restrict	head;
	struct s_input_token *restrict	last;
	size_t	toks_count;
} msh_attr_align;

/**
 * Create fast compaund literal for key-matching functions
 * \param _token: char*
 * \param _length: size_t
 * \param _type: token_t
 * \return struct s_input_token*
 */
# define sit_fast_new_key(_token, _length, _type) (struct s_token_key) { \
	.tok = (_token), .len = (_length), .type = (_type), \
	.__align = { 0 } \
}

/**
 * Create new token
 * \param _token: char*
 * \param _length: size_t
 * \param _type: token_t
 * \return struct s_input_token*
 */
# define sit_new(_token, _length, _type) __extension__({ \
	struct s_input_token *restrict __out = NULL; \
	const char *restrict __sit_tok = (const char *restrict)(_token); \
	size_t __sit_len = (size_t)(length); \
	token_t __sit_type = (token_t)(_type); \
	assert((__out = calloc(1, sizeof(*__out)))); \
	assert((__out->key.tok = strndup(__sit_tok, __sit_len))); \
	__out->key.len = __sit_len; \
	__out->key.type = __sit_type; \
	__out; \
})

/**
 * Free given token
 * \param _ft: struct s_input_token*
 */
# define sit_freetok(_ft) __extension__({ \
	__typeof__(_ft) *restrict __ft = &(_ft); \
	free((*__ft)->key.tok); \
	free(*__ft); \
	*__ft = NULL; \
})

/**
 * Delete links to given token
 * \param _id: struct s_input_data*
 * \param _t: struct s_input_token*
 */
# define sit_del(_id, _t) __extension__({ \
	__typeof__ (_id) __id = (_id); \
	__typeof__ (_t) __t = (_t); \
	if (__t->prev) { \
		__t->prev->next = __t->next; \
	} else { \
		__id->head = __t->next; \
	} \
	if (__t->next) { \
		__t->next->prev = __t->prev; \
	} \
	--__id->toks_count; \
	sit_freetok(__t); \
})

/**
 * Free all data
 * \param _id: struct s_input_data*
 */
# define sit_free(_id) __extension__({ \
	__typeof__ (_id)	__id = (_id); \
	while (sit_delhead(__id)) \
		; \
	free(_id); \
})


/**
 * Returns a size of tokens in linked list
 * \param _id: struct s_input_data*
 */
# define sit_getsize(_id) __extension__({ (_id)->toks_count; })

/**
 * Create new token at the front of list
 */
static inline void	sit_pushfront(struct s_input_data *restrict id,
		const char *restrict token, size_t length, token_t type) {
	struct s_input_token *restrict	new = sit_new(token, length, type);

	++id->toks_count;
	if (!id->head) {
		id->head = new;
		id->last = new;
	} else {
		id->head->prev = new;
		new->next = id->head;
		id->head = new;
	}
}

/**
 * Create new token at the end of list
 */
static inline void	sit_pushback(struct s_input_data *restrict id,
		const char *restrict token, size_t length, token_t type) {
	struct s_input_token *restrict	new = sit_new(token, length, type);

	++id->toks_count;
	if (!id->head) {
		id->head = new;
		id->last = new;
	} else {
		id->last->next = new;
		new->prev = id->last;
		id->last = new;
	}
}

static inline void	sit_printone(struct s_input_token *restrict t) {
	struct s_token_key *restrict	k = &t->key;
	DBG_INFO("'%s'(%zu): %d - ", k->tok, k->len, k->type);
}

/**
 * Print all tokens from begin
 */
static inline void	sit_print(struct s_input_data *restrict id) {
	if (!g_opt_dbg_level)
		return ;
	DBG_INFO("%zu elements\nFrom begin: ", id->toks_count);
	for (struct s_input_token *i = id->head; i; i = i->next)
		sit_printone(i);
	fprintf(stderr, "(null)\n");
}

/**
 * Print all tokens from end
 */
static inline void	sit_rprint(struct s_input_data *restrict id) {
	if (!g_opt_dbg_level)
		return ;
	DBG_INFO("%zu elements\nFrom end: ", id->toks_count);
	for (struct s_input_token *i = id->last; i; i = i->prev)
		sit_printone(i);
	fprintf(stderr, "(null)\n");
}

/**
 * Find token by given key
 * \return NULL if token with given key not exist, pointer to token if exist
 */
static inline struct s_input_token
*sit_findkey(const struct s_input_data *restrict id,
			const struct s_token_key *restrict key) {
	struct s_input_token *restrict	match = id->head;

	while (match) {
		struct s_token_key *restrict	k = &match->key;
		if (k->type == key->type
		&& k->len == key->len
		&& !strcmp(k->tok, key->tok))
			return match;
	}
	return NULL;
}

/**
 * Delete token by given key
 */
static inline bool	sit_delkey(struct s_input_data *restrict id,
		const struct s_token_key *restrict key) {
	struct s_input_token *restrict	match = sit_findkey(id, key);
	if (!match)
		return false;
	sit_del(id, match);
	if (!id->head || !id->last)
		id->head = id->last = NULL;
	return true;
}

/**
 * Find token by index(starts from 1)
 */
static inline struct s_input_token
*sit_findid(const struct s_input_data *restrict id, size_t index) {
	if (index > id->toks_count)
		return NULL;
	struct s_input_token	*match = id->head;
	for (size_t indx = 1; match && indx != index; ++indx, match = match->next)
		;
	return match;
}

/**
 * Delete token by index(starts from 1)
 */
static inline bool	sit_delid(struct s_input_data *restrict id, size_t index) {
	struct s_input_token *restrict	match = sit_findid(id, index);
	if (!match)
		return false;
	sit_del(id, match);
	if (!id->head || !id->last)
		id->head = id->last = NULL;
	return true;
}

/**
 * Delete first token
 */
static inline bool	sit_delhead(struct s_input_data *restrict id) {
	if (!id || !id->head)
		return false;
	struct s_input_token *restrict	save = id->head->next;
	sit_del(id, id->head);
	id->head = save;
	if (!id->head)
		id->last = NULL;
	return true;
}

/**
 * Delete last token
 */
static inline bool	sit_dellast(struct s_input_data *restrict id) {
	if (!id || !id->last)
		return false;
	struct s_input_token *restrict	save = id->last->prev;
	sit_del(id, id->last);
	id->last = save;
	if (!id->last)
		id->head = NULL;
	return true;
}

/**
 * Find a token by a key
 * \return pointer to that token
 */

#endif /* MSH_INPUT_TOKEN_H */
