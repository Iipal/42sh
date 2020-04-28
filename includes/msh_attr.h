#ifndef MSH_ATTR_H
# define MSH_ATTR_H

# define msh_attr_unused __attribute__((unused))
# define msh_attr_pack __attribute__((packed))
# define msh_attr_align __attribute__((aligned(__BIGGEST_ALIGNMENT__)))

# ifndef __clang__
#  define msh_attr_fallthrough __attribute__((fallthrough))
# else
#  define msh_attr_fallthrough
# endif /* __clang__ */

#endif /* MSH_ATTR_H */
