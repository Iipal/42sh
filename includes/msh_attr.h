#ifndef MSH_ATTR_H
# define MSH_ATTR_H

# define msh_attr_unused __attribute__((unused))
# define msh_attr_pack __attribute__((packed))
# define msh_attr_align __attribute__((aligned(__BIGGEST_ALIGNMENT__)))

#endif /* MSH_ATTR_H */
